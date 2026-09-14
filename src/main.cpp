#include "main.hpp"

#include <esp_rom_sys.h>
#include <freertos/task.h>

#include <algorithm>
#include <cstdio>
#include <string_view>

#include "app/app.hpp"
#include "config/config_manager.hpp"
#include "system/device_status.hpp"
#include "system/logger.hpp"

#if defined(EBUS_INTERNAL)
#include <ebus/controller.hpp>
#endif

ConfigManager configManager;

extern "C" void app_main(void) {
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

  DeviceStatus::setResetCode((uint32_t)esp_rom_get_reset_reason(0));

  App app(configManager);
  if (!app.begin()) {
    logger.error("Application initialization failed, restarting");
    restart();
  }

  app.loop();
  vTaskDelete(nullptr);
}
