#include "app/app.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <cstdlib>

#include "config/app_config_loader.hpp"
#include "config/config_manager.hpp"
#include "hardware/board_control.hpp"
#include "hardware/pwm.hpp"
#include "main.hpp"
#include "network/captive_dns.hpp"
#include "network/http.hpp"
#include "network/http_utils.hpp"
#include "network/sntp.hpp"
#include "network/wifi_network_manager.hpp"
#include "system/adapter_version.hpp"
#include "system/device_identity.hpp"
#include "system/device_status.hpp"
#include "system/esp_ota_manager.hpp"
#include "system/logger.hpp"
#include "system/upgrade_manager.hpp"

#if defined(EBUS_INTERNAL)
#include "app/command_manager.hpp"
#include "app/cron.hpp"
#include "app/ebus_accessor.hpp"
#include "app/mqtt.hpp"
#include "app/mqtt_ha.hpp"
#include "system/system_monitor.hpp"
#else
#include "legacy/bus_type.hpp"
#include "legacy/client.hpp"
#endif

App* App::instance_ = nullptr;

App::App(ConfigManager& config_manager) : config_manager_(config_manager) {
  config_.reset();
  instance_ = this;
}

App* App::instance() { return instance_; }

bool App::begin() {
  if (!initPlatform()) return false;
  if (!initConfig()) return false;
  if (!initNetwork()) return false;
  if (!initServices()) return false;
  if (!initHttp()) return false;
  if (!startTasks()) return false;
  return true;
}

bool App::initPlatform() {
  check_reset();

  calcUniqueId();
  loadAdapterHwVersionFromEfuse();
  if (getAdapterHwVersionRaw() ==
      static_cast<uint8_t>(AdapterHwVersionEfuse::V7_0)) {
    WifiNetworkManager::setStatusLedPin(5);
  } else {
    WifiNetworkManager::setStatusLedPin(3);
  }

#if !defined(EBUS_INTERNAL)
  Bus.begin();
#endif

  disableTX();

#if defined(PWM_PIN)
  initPwm();
#endif

  return true;
}

bool App::initConfig() {
  configManager.begin();
  if (!loadConfig()) return false;
  DeviceStatus::setConfig(&config_);
  return true;
}

bool App::initNetwork() {
  WifiNetworkManager::begin(&config_manager_);
  startCaptiveDns();
  return true;
}

#if defined(EBUS_INTERNAL)
void App::onStaIpAssigned(const std::string& ipAddress) {
  if (ipAddress.empty()) return;

  mqtt_ha_.setThingConfigurationUrl("http://" + ipAddress + "/");

  if (mqtt_ha_.isEnabled()) {
    Mqtt::publishDiscovery();
    Mqtt::publishComponentDiscovery();
  }
}
#endif

bool App::initServices() {
  set_pwm(config_.pwm.value);
#if defined(EBUS_INTERNAL) && defined(PWM_PIN)
  getEbusController().resetMetrics();
#endif

#if defined(EBUS_INTERNAL)
  if (config_.sntp.enabled) {
    initSNTP(config_.sntp);
    setTimezone(config_.sntp);
  }

  const AppConfig::Mqtt& mqtt_config = config_.mqtt;
  mqtt_.setEnabled(mqtt_config.enabled);
  mqtt_.setup(getUniqueId());
  mqtt_.setServer(mqtt_config.server.c_str(), 1883);
  mqtt_.setCredentials(mqtt_config.user.c_str(), mqtt_config.pass.c_str());
  if (!mqtt_config.root_topic.empty()) {
    mqtt_.setRootTopic(std::string(mqtt_config.root_topic.c_str()));
  }
  mqtt_.start();
  mqtt_.setStatusProvider(DeviceStatus::fetchStatus);

  mqtt_ha_.setUniqueId(mqtt_.getUniqueId());
  mqtt_ha_.setRootTopic(mqtt_.getRootTopic());
  mqtt_ha_.setWillTopic(mqtt_.getWillTopic());
  mqtt_ha_.setEnabled(config_.mqtt_ha.enabled);

  mqtt_ha_.setThingName(std::string(config_.mqtt_ha.thing_name.c_str()));
  mqtt_ha_.setThingHwVersion(getAdapterHwVersionString());
  mqtt_ha_.setThingModel("esp-eBus Adapter");
  mqtt_ha_.setThingModelId("esp-ebus-adapter");

  // Wire the two directions explicitly: HA publishes through MQTT transport,
  // MQTT consults HA hooks. No globals in either direction.
  mqtt_ha_.setTransport(
      {[this](const char* topic, uint8_t qos, bool retain, const char* payload,
              bool prefix) {
         mqtt_.publish(topic, qos, retain, payload, prefix);
       },
       [this](const char* topic, uint8_t qos, bool retain,
              const std::function<void(const ebus::JsonChunkVisitor&)>& builder,
              bool prefix) {
         mqtt_.publishStream(topic, qos, retain, builder, prefix);
       }});
  mqtt_.setHaHooks(
      {[this]() { return mqtt_ha_.isEnabled(); },
       [this](bool enable) { mqtt_ha_.setEnabled(enable); },
       [this]() { mqtt_ha_.publishDeviceInfo(); },
       [this]() { mqtt_ha_.publishComponents(); },
       [this](const Command* command, size_t field_idx, bool remove) {
         mqtt_ha_.publishComponent(command, field_idx, remove);
       },
       [this]() { mqtt_ha_.onMqttConnected(); }});
  WifiNetworkManager::setStaIpAssignedCallback(
      [](const std::string& ipAddress) {
        if (App* app = App::instance()) app->onStaIpAssigned(ipAddress);
      });
#endif

  esp_ota_manager_.begin();
  enableTX();

#if defined(EBUS_INTERNAL)

#if defined(EBUS_SIMULATION)
  logger.info("Running in eBUS simulation mode");

  // RuntimeConfig
  ebus::RuntimeConfig runtime_config{};
  runtime_config.log_level = ebus::LogLevel::debug;

  runtime_config.address = 0x01;  // slave address 0x06
  runtime_config.lock_counter = 3;
  runtime_config.system_inquiry = false;
  runtime_config.system_response = false;

  // Bus
  runtime_config.bus.window_us = config_.bus.window_us;
  runtime_config.bus.offset_us = config_.bus.offset_us;
  runtime_config.bus.watchdog_timeout_ms = 250;
  runtime_config.bus.syn_gen = true;

  // Network
  runtime_config.network.session_timeout_ms = 2000;
  runtime_config.network.transmit_timeout_ms = 1000;
  runtime_config.network.outbound_buffer_size = 2048;
  runtime_config.network.enable_server = true;
  runtime_config.network.port_regular = 3333;
  runtime_config.network.port_readonly = 3334;
  runtime_config.network.port_enhanced = 3335;

  // Device
  runtime_config.device.scan_on_startup = false;
  runtime_config.device.initial_delay_s = 5;
  runtime_config.device.startup_interval_s = 25;
  runtime_config.device.max_startup_scans = 5;

  // Scheduler
  runtime_config.scheduler.max_attempts = 1;
  runtime_config.scheduler.base_backoff_ms = 100;
  runtime_config.scheduler.fsm_timeout_ms = 1000;
  runtime_config.scheduler.total_timeout_ms = 2000;

#else
  logger.info("Running in normal eBUS mode");

  // General
  ebus::RuntimeConfig runtime_config{};
  runtime_config.log_level = ebus::LogLevel::debug;

  runtime_config.address = static_cast<uint8_t>(
      std::strtoul(config_.bus.address.c_str(), nullptr, 16));
  runtime_config.lock_counter = 3;
  runtime_config.system_inquiry = config_.bus.system_inquiry;
  runtime_config.system_response = config_.bus.system_response;

  // Bus
  runtime_config.bus.window_us = config_.bus.window_us;
  runtime_config.bus.offset_us = config_.bus.offset_us;
  runtime_config.bus.watchdog_timeout_ms = 250;
  runtime_config.bus.syn_gen = false;

  // Network
  runtime_config.network.session_timeout_ms = 2000;
  runtime_config.network.transmit_timeout_ms = 1000;
  runtime_config.network.outbound_buffer_size = 2048;
  runtime_config.network.enable_server = true;
  runtime_config.network.port_regular = 3333;
  runtime_config.network.port_readonly = 3334;
  runtime_config.network.port_enhanced = 3335;

  // Device
  runtime_config.device.scan_on_startup = config_.bus.scan_on_startup;
  runtime_config.device.initial_delay_s = 5;
  runtime_config.device.startup_interval_s = 25;
  runtime_config.device.max_startup_scans = 5;

  // Scheduler
  runtime_config.scheduler.max_attempts = 1;
  runtime_config.scheduler.base_backoff_ms = 100;
  runtime_config.scheduler.fsm_timeout_ms = 1000;
  runtime_config.scheduler.total_timeout_ms = 2000;

  // BusConfig
  ebus::BusConfig bus_config = {.uart_port = UART_NUM_1,
                                .rx_pin = UART_RX,
                                .tx_pin = UART_TX,
                                .timer_group = 1,
                                .timer_idx = 0};
  getEbusConfig().bus = bus_config;
#endif
  getEbusConfig().runtime = runtime_config;

  // CRITICAL: Ensure configuration is applied or we will crash
  if (!getEbusController().configure(getEbusConfig())) {
    logger.error("eBUS: Global Configuration failed! Simulation may crash.");
    return false;
  }

  // Optimized callbacks: Avoid heap-heavy JSON work inside library threads
  getEbusController().setProtocolCallback(
      [this](const ebus::ProtocolInfo& info) {
        char buf[128];
        if (info.is_error)
          snprintf(buf, sizeof(buf), "%s / %s -> '%s'",
                   ebus::toString(info.master_view).c_str(),
                   ebus::toString(info.slave_view).c_str(),
                   ebus::toString(info.protocol_error));
        else
          snprintf(buf, sizeof(buf), "%s / %s",
                   ebus::toString(info.master_view).c_str(),
                   ebus::toString(info.slave_view).c_str());
        logger.info(buf, false, info.session_id, info.poll_id);
        monitor_.enqueueProtocolInfo(info);
      });

  // getEbusController().setTraceCallback([](const ebus::BusEventInfo& info) {
  //   logger.debug(ebus::toJson(info, 256));
  // });

  startEbus();  // This will start the ebus controller

#if defined(EBUS_SIMULATION)
  startEbusSimulation();
#endif

  monitor_.begin();
  DeviceStatus::setMonitor(&monitor_);
  DeviceStatus::setEspOtaManager(&esp_ota_manager_);
  DeviceStatus::setMqtt(&mqtt_);
  DeviceStatus::setMqttHa(&mqtt_ha_);

  commandManager.setDataUpdatedCallback(Mqtt::publishValue);

  commandManager.setDataUpdatedLogCallback(
      [this](std::string_view key) { monitor_.enqueueLogRequest(key); });

  // Setup lifecycle listeners to keep ebusController in sync with the
  // CommandManager
  commandManager.setCommandChangedCallback([this](Command* cmd) {
    // Remove existing poll item if it was already registered
    if (cmd->getPollId() != 0) {
      char log_buf[128];
      snprintf(log_buf, sizeof(log_buf),
               "Releasing Poll ID %lu for key '%.*s' during command change.",
               (unsigned long)cmd->getPollId(), (int)cmd->getKey().size(),
               cmd->getKey().data());
      logger.warn(log_buf);
      getEbusController().removePollItem(cmd->getPollId());
      cmd->setPollId(0);
    }
    // Add new poll item if active and has a valid read command
    if (cmd->getActive() && !cmd->getReadCmd().empty()) {
      std::string_view key = cmd->getKey();
      uint32_t id = getEbusController().addPollItem(3, cmd->getReadCmd(),
                                                    cmd->getInterval() * 1000);
      char log_buf[128];
      snprintf(log_buf, sizeof(log_buf),
               "Re-registering Poll ID %lu for key '%.*s'", (unsigned long)id,
               (int)key.size(), key.data());
      logger.info(log_buf);
      cmd->setPollId(id);
    } else {
      char log_buf[128];
      snprintf(log_buf, sizeof(log_buf),
               "No valid poll item to register/update for command change on "
               "key '%.*s'",
               (int)cmd->getKey().size(), cmd->getKey().data());
      logger.info(log_buf);
    }
    // HA: publish components for this command if HA enabled and MQTT connected
    if (mqtt_ha_.isEnabled()) {
      for (size_t i = 0; i < cmd->getFieldCount(); ++i) {
        if (cmd->hasFieldHA(i)) {
          Mqtt::enqueueOutgoing(OutgoingAction(cmd, i, false));
        }
      }
    }
  });

  commandManager.setCommandRemovedCallback([this](Command* cmd) {
    if (cmd->getPollId() != 0) {
      getEbusController().removePollItem(cmd->getPollId());
      cmd->setPollId(0);
    }
    if (mqtt_ha_.isEnabled()) {
      mqtt_ha_.removeComponent(cmd);
    }
  });

  if (!commandManager.initFileSystem()) {
    logger.error("LittleFS initialization failed");
    return false;
  }

#if defined(EBUS_INTERNAL)
  // Emergency recovery: uncomment to wipe commands.json on boot
  // std::remove("/littlefs/commands.json");
  // std::remove("/littlefs/commands.json.tmp");
#endif

  commandManager
      .loadCommands();  // Automatically registers poll items via the callback

  cron.initFileSystem();  // This should be called before cron.loadRules()
  cron.loadRules();
  cron.start();

  return true;
#else
  return true;
#endif
}

bool App::initHttp() {
#if defined(EBUS_INTERNAL)
  SetupHttpHandlers(mqtt_ha_);
#else
  SetupHttpHandlers();
#endif
  HttpUtils::setCustomHeaders(std::string(config_.http.headers.c_str()));
  upgrade_manager_.begin();
  SetupHttpFallbackHandlers();
  upgrade_manager_.setPreUpgradeHook([this]() { stop(); });
  esp_ota_manager_.setPreUpgradeHook([this]() { stop(); });
  return true;
}

bool App::startTasks() {
#if defined(EBUS_INTERNAL)
  mqtt_.startTask();
  return true;
#else
  if (!startClientRuntime()) {
    logger.error("Failed to start client runtime");
    return false;
  }
  return true;
#endif
}

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
  if (!staging.isValid()) {
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
  mqtt_.stopTask();

  // Now safe to stop other components
  cron.stop();
  stopEbus();
  monitor_.stop();

  vTaskDelay(pdMS_TO_TICKS(500));
#else
  stopClientRuntime();
#endif
}