#include "app/app.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config/app_config_loader.hpp"
#include "config/app_config_validator.hpp"
#include "config_manager.hpp"

#if defined(EBUS_INTERNAL)
#include "cron.hpp"
#include "ebus_accessor.hpp"
#include "mqtt.hpp"
#include "system_monitor.hpp"
#else
#include "legacy/client.hpp"
#endif

App* App::instance_ = nullptr;

App::App(ConfigManager& config_manager) : config_manager_(config_manager) {
  config_.reset();
  instance_ = this;
}

App* App::instance() { return instance_; }

bool App::begin() { return true; }

bool App::loadConfig() {
  AppConfigLoader loader(config_manager_);
  return loader.load(config_);
}

bool App::applyFlatConfigJson(std::string_view body, std::string& error) {
  AppConfig staging = config_;
  std::vector<std::pair<std::string, std::string>> unknowns;
  if (!staging.mergeFlatJson(body, &unknowns)) {
    error = "Invalid config JSON";
    return false;
  }
  if (!config::AppConfigValidator::validate(staging)) {
    error = "Config values out of range";
    return false;
  }
  AppConfigLoader loader(config_manager_);
  if (!loader.save(staging)) {
    error = "Failed to write NVS";
    return false;
  }
  // Preserve legacy behavior: unknown keys are stored to NVS directly.
  for (const auto& kv : unknowns) {
    if (!config_manager_.writeString(kv.first.c_str(), kv.second)) {
      error = "Failed to write NVS";
      return false;
    }
  }
  // No live swap: the UI contract is restart-to-apply, so the snapshot keeps
  // boot values (matching the running services) until the next reboot loads
  // the saved state.
  return true;
}

void App::loop() {
  // The main task owns this App instance (including the AppConfig snapshot
  // referenced by DeviceStatus and the OTA hooks). Never return: falling off
  // the end of app_main deletes the main task and frees its stack out from
  // under all still-running service tasks (use-after-free, heap poison).
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void App::stop() {
#if defined(EBUS_INTERNAL)
  // CRITICAL: Stop MQTT first and wait for task to fully exit
  // This prevents the MQTT task from accessing eBUS/Cron/SystemMonitor
  // resources while they are being stopped
  mqtt.stopTask();

  // Now safe to stop other components
  cron.stop();
  stopEbus();
  SystemMonitor::stop();

  vTaskDelay(pdMS_TO_TICKS(500));
#else
  stopClientRuntime();
#endif
}