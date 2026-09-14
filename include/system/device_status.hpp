#pragma once

#include <cstdint>
#include <ebus/types.hpp>

struct AppConfig;

#if defined(EBUS_INTERNAL)
class SystemMonitor;
#endif
class EspOtaManager;

/**
 * Device status model: gathers and serializes the full device status JSON.
 * Consumed by all status publishers (HTTP status API, MQTT telemetry).
 */
class DeviceStatus {
 public:
  static void setConfig(const AppConfig* config);
  static void setResetCode(uint32_t code);
  static void setEspOtaManager(EspOtaManager* manager);
#if defined(EBUS_INTERNAL)
  static void setMonitor(SystemMonitor* monitor);
#endif

  static const AppConfig& config();
  static uint32_t resetCode();
  static EspOtaManager& espOtaManager();
#if defined(EBUS_INTERNAL)
  static SystemMonitor& monitor();
#endif

  static void fetchStatus(const ebus::JsonChunkVisitor& visitor);

#if defined(EBUS_INTERNAL)
  static void fetchAppStatus(const ebus::JsonChunkVisitor& visitor);
#endif

 private:
  static const AppConfig* config_;
  static uint32_t reset_code_;
  static EspOtaManager* esp_ota_manager_;
#if defined(EBUS_INTERNAL)
  static SystemMonitor* monitor_;
#endif
};
