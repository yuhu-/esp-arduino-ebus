#include "app/app.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "config/app_config_loader.hpp"
#include "config_manager.hpp"

#if defined(EBUS_INTERNAL)
#include "cron.hpp"
#include "ebus_accessor.hpp"
#include "mqtt.hpp"
#include "system_monitor.hpp"
#else
#include "legacy/client.hpp"
#endif

App::App(ConfigManager& config_manager) : config_manager_(config_manager) {
  config_.reset();
}

bool App::begin() { return true; }

bool App::loadConfig() {
  AppConfigLoader loader(config_manager_);
  return loader.load(config_);
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