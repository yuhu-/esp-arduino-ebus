#include "main.hpp"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_mac.h>
#include <esp_private/esp_clk.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <inttypes.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <ebus/detail/json_writer.hpp>

#include "app/app.hpp"
#include "app/app_limits.hpp"
#include "logger.hpp"

#if defined(EBUS_INTERNAL)
#include "command_manager.hpp"
#include "cron.hpp"
#include "ebus_accessor.hpp"
#include "mqtt.hpp"
#include "mqtt_ha.hpp"
#include "system_monitor.hpp"
#else
#include "legacy/bus_type.hpp"
#include "legacy/client.hpp"
#endif

#include "config_manager.hpp"
#include "dns_server.hpp"
#include "esp_ota_manager.hpp"
#include "esp_rom_sys.h"
#include "esp_sntp.h"
#include "hardware/pwm.hpp"
#include "http.hpp"
#include "network/captive_dns.hpp"
#include "system/adapter_version.hpp"
#include "system/device_status.hpp"
#if defined(EBUS_INTERNAL)
#include "network/sntp.hpp"
#endif
#include "http_utils.hpp"
#include "upgrade_manager.hpp"
#include "wifi_network_manager.hpp"

ConfigManager configManager;
UpgradeManager upgradeManager;
EspOtaManager espOtaManager;

extern "C" void app_main(void) {
  App app(configManager);
  app.begin();

  DebugSer.begin(115200);
  DebugSer.setDebugOutput(true);

  logger.info("Starting esp-ebus adapter version " AUTO_VERSION);

#if defined(EBUS_INTERNAL)
  // Connect library logger to app logger
  ebus::Controller::setLogSink([](ebus::LogLevel level, std::string_view msg) {
    char buf[max_msg_length];
    int n = snprintf(buf, sizeof(buf), "eBUS-Lib: %.*s", (int)msg.size(),
                     msg.data());
    if (n < 0) return;
    std::string_view out(buf, std::min((size_t)n, sizeof(buf) - 1));

    switch (level) {
      case ebus::LogLevel::error:
        logger.error(out);
        break;
      case ebus::LogLevel::info:
        logger.info(out);
        break;
      case ebus::LogLevel::debug:
        logger.debug(out);
        break;
      default:
        break;
    }
  });
#endif

  check_reset();

  DeviceStatus::setResetCode((uint32_t)esp_rom_get_reset_reason(0));

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

  WifiNetworkManager::begin(&configManager);
  startCaptiveDns();
  SetupHttpHandlers();
  configManager.begin();
  app.loadConfig();
  DeviceStatus::setConfig(&app.config());
  HttpUtils::setCustomHeaders(std::string(app.config().http.headers.c_str()));
  upgradeManager.begin();
  SetupHttpFallbackHandlers();
  upgradeManager.setPreUpgradeHook([&app]() { app.stop(); });
  espOtaManager.setPreUpgradeHook([&app]() { app.stop(); });

  set_pwm(app.config().pwm.value);
#if defined(EBUS_INTERNAL) && defined(PWM_PIN)
  getEbusController().resetMetrics();
#endif

#if defined(EBUS_INTERNAL)
  if (app.config().sntp.enabled) {
    initSNTP(app.config().sntp);
    setTimezone(app.config().sntp);
  }

  const AppConfig::Mqtt& mqttConfig = app.config().mqtt;
  mqtt.setEnabled(mqttConfig.enabled);
  mqtt.setup(getUniqueId());
  mqtt.setServer(mqttConfig.server.c_str(), 1883);
  mqtt.setCredentials(mqttConfig.user.c_str(), mqttConfig.pass.c_str());
  if (!mqttConfig.root_topic.empty()) {
    mqtt.setRootTopic(std::string(mqttConfig.root_topic.c_str()));
  }
  mqtt.start();
  mqtt.setStatusProvider(DeviceStatus::fetchStatus);

  mqttha.setUniqueId(mqtt.getUniqueId());
  mqttha.setRootTopic(mqtt.getRootTopic());
  mqttha.setWillTopic(mqtt.getWillTopic());
  mqttha.setEnabled(app.config().mqtt_ha.enabled);

  mqttha.setThingName(std::string(app.config().mqtt_ha.thing_name.c_str()));
  mqttha.setThingHwVersion(getAdapterHwVersionString());
  mqttha.setThingModel("esp-eBus Adapter");
  mqttha.setThingModelId("esp-ebus-adapter");
  WifiNetworkManager::setStaIpAssignedCallback(
      [](const std::string& ipAddress) {
        if (ipAddress.empty()) return;

        mqttha.setThingConfigurationUrl("http://" + ipAddress + "/");

        if (mqttha.isEnabled()) {
          Mqtt::publishDiscovery();
          Mqtt::publishComponentDiscovery();
        }
      });
#endif

  espOtaManager.begin();
  enableTX();

#if defined(EBUS_INTERNAL)

#if defined(EBUS_SIMULATION)
  logger.info("Running in eBUS simulation mode");

  // RuntimeConfig
  ebus::RuntimeConfig runtimeConfig{};
  runtimeConfig.log_level = ebus::LogLevel::debug;

  runtimeConfig.address = 0x01;  // slave address 0x06
  runtimeConfig.lock_counter = 3;
  runtimeConfig.system_inquiry = false;
  runtimeConfig.system_response = false;

  // Bus
  runtimeConfig.bus.window_us = app.config().bus.window_us;
  runtimeConfig.bus.offset_us = app.config().bus.offset_us;
  runtimeConfig.bus.watchdog_timeout_ms = 250;
  runtimeConfig.bus.syn_gen = true;

  // Network
  runtimeConfig.network.session_timeout_ms = 2000;
  runtimeConfig.network.transmit_timeout_ms = 1000;
  runtimeConfig.network.outbound_buffer_size = 2048;
  runtimeConfig.network.enable_server = true;
  runtimeConfig.network.port_regular = 3333;
  runtimeConfig.network.port_readonly = 3334;
  runtimeConfig.network.port_enhanced = 3335;

  // Device
  runtimeConfig.device.scan_on_startup = false;
  runtimeConfig.device.initial_delay_s = 5;
  runtimeConfig.device.startup_interval_s = 25;
  runtimeConfig.device.max_startup_scans = 5;

  // Scheduler
  runtimeConfig.scheduler.max_attempts = 1;
  runtimeConfig.scheduler.base_backoff_ms = 100;
  runtimeConfig.scheduler.fsm_timeout_ms = 1000;
  runtimeConfig.scheduler.total_timeout_ms = 2000;

#else
  logger.info("Running in normal eBUS mode");

  // General
  ebus::RuntimeConfig runtimeConfig{};
  runtimeConfig.log_level = ebus::LogLevel::debug;

  runtimeConfig.address = static_cast<uint8_t>(
      std::strtoul(app.config().bus.address.c_str(), nullptr, 16));
  runtimeConfig.lock_counter = 3;
  runtimeConfig.system_inquiry = app.config().bus.system_inquiry;
  runtimeConfig.system_response = app.config().bus.system_response;

  // Bus
  runtimeConfig.bus.window_us = app.config().bus.window_us;
  runtimeConfig.bus.offset_us = app.config().bus.offset_us;
  runtimeConfig.bus.watchdog_timeout_ms = 250;
  runtimeConfig.bus.syn_gen = false;

  // Network
  runtimeConfig.network.session_timeout_ms = 2000;
  runtimeConfig.network.transmit_timeout_ms = 1000;
  runtimeConfig.network.outbound_buffer_size = 2048;
  runtimeConfig.network.enable_server = true;
  runtimeConfig.network.port_regular = 3333;
  runtimeConfig.network.port_readonly = 3334;
  runtimeConfig.network.port_enhanced = 3335;

  // Device
  runtimeConfig.device.scan_on_startup = app.config().bus.scan_on_startup;
  runtimeConfig.device.initial_delay_s = 5;
  runtimeConfig.device.startup_interval_s = 25;
  runtimeConfig.device.max_startup_scans = 5;

  // Scheduler
  runtimeConfig.scheduler.max_attempts = 1;
  runtimeConfig.scheduler.base_backoff_ms = 100;
  runtimeConfig.scheduler.fsm_timeout_ms = 1000;
  runtimeConfig.scheduler.total_timeout_ms = 2000;

  // BusConfig
  ebus::BusConfig busConfig = {.uart_port = UART_NUM_1,
                               .rx_pin = UART_RX,
                               .tx_pin = UART_TX,
                               .timer_group = 1,
                               .timer_idx = 0};
  getEbusConfig().bus = busConfig;
#endif
  getEbusConfig().runtime = runtimeConfig;

  // CRITICAL: Ensure configuration is applied or we will crash
  if (!getEbusController().configure(getEbusConfig())) {
    logger.error("eBUS: Global Configuration failed! Simulation may crash.");
  }

  // Optimized callbacks: Avoid heap-heavy JSON work inside library threads
  getEbusController().setProtocolCallback([](const ebus::ProtocolInfo& info) {
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
    SystemMonitor::enqueueProtocolInfo(info);
  });

  // getEbusController().setTraceCallback([](const ebus::BusEventInfo& info) {
  //   logger.debug(ebus::toJson(info, 256));
  // });

  startEbus();  // This will start the ebus controller

#if defined(EBUS_SIMULATION)
  startEbusSimulation();
#endif

  SystemMonitor::begin();

  commandManager.setDataUpdatedCallback(Mqtt::publishValue);

  commandManager.setDataUpdatedLogCallback(
      [](std::string_view key) { SystemMonitor::enqueueLogRequest(key); });

  // Setup lifecycle listeners to keep ebusController in sync with the
  // CommandManager
  commandManager.setCommandChangedCallback([](Command* cmd) {
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
    if (mqttha.isEnabled()) {
      for (size_t i = 0; i < cmd->getFieldCount(); ++i) {
        if (cmd->hasFieldHA(i)) {
          Mqtt::enqueueOutgoing(OutgoingAction(cmd, i, false));
        }
      }
    }
  });

  commandManager.setCommandRemovedCallback([](Command* cmd) {
    if (cmd->getPollId() != 0) {
      getEbusController().removePollItem(cmd->getPollId());
      cmd->setPollId(0);
    }
    if (mqttha.isEnabled()) {
      mqttha.removeComponent(cmd);
    }
  });

  if (!commandManager.initFileSystem()) {
    logger.error("LittleFS initialization failed");
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

  mqtt.startTask();
#else
  if (!startClientRuntime()) {
    logger.error("Failed to start client runtime");
  }
#endif
  app.loop();
  vTaskDelete(nullptr);
}
