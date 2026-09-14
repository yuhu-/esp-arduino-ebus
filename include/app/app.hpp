#pragma once

#include <string>
#include <string_view>

#include "config/app_config.hpp"

#if defined(EBUS_INTERNAL)
#include "system/system_monitor.hpp"
#endif
#include "system/esp_ota_manager.hpp"
#include "system/upgrade_manager.hpp"

class ConfigManager;

class App {
 public:
  explicit App(ConfigManager& config_manager);

  // Single instance registry (mirrors StatusApi/StringPool): exactly one App
  // exists on the main task. Lets static HTTP handlers reach the live App.
  static App* instance();

  bool begin();
  // Main supervision loop. Never returns: the main task owns this App
  // instance, so returning would delete the task and free its stack while
  // service tasks still reference it.
  void loop();
  void stop();

  // Loads NVS contents into the owned snapshot. Caller must have called
  // ConfigManager::begin() first. Returns loader result.
  bool loadConfig();

  // Applies a flat NVS-key JSON object (as posted by /api/v1/config) onto a
  // copy of the snapshot, validates it and persists via
  // AppConfigLoader::save(). Unknown keys are stored to NVS directly,
  // preserving the legacy writeConfigJson behavior. The live snapshot is
  // intentionally left untouched: the UI contract is restart-to-apply.
  bool applyFlatConfigJson(std::string_view body, std::string& error);

  const AppConfig& config() const { return config_; }

 private:
  static App* instance_;

  UpgradeManager upgrade_manager_;
  EspOtaManager esp_ota_manager_;
#if defined(EBUS_INTERNAL)
  SystemMonitor monitor_;
#endif

  // Init phases, executed in order by begin(). Each phase owns one slice of
  // the former app_main inline sequence; begin() short-circuits on failure.
  bool initPlatform();
  bool initConfig();
  bool initNetwork();
  bool initServices();
  bool initHttp();
  bool startTasks();

  ConfigManager& config_manager_;
  AppConfig config_;
};