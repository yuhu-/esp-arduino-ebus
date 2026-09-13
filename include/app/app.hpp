#pragma once

#include "config/app_config.hpp"

class ConfigManager;

class App {
 public:
  explicit App(ConfigManager& config_manager);

  bool begin();
  // Main supervision loop. Never returns: the main task owns this App
  // instance, so returning would delete the task and free its stack while
  // service tasks still reference it.
  void loop();
  void stop();

  // Loads NVS contents into the owned snapshot. Caller must have called
  // ConfigManager::begin() first. Returns loader result.
  bool loadConfig();

  const AppConfig& config() const { return config_; }

 private:
  ConfigManager& config_manager_;
  AppConfig config_;
};