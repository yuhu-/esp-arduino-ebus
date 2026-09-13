#include "system/device_status.hpp"

#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_private/esp_clk.h>
#include <esp_sntp.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <ebus/detail/json_writer.hpp>
#include <ebus/status.hpp>

#include "app/app_limits.hpp"
#include "config/app_config.hpp"
#include "esp_ota_manager.hpp"
#include "logger.hpp"
#include "main.hpp"
#include "mqtt.hpp"
#include "mqtt_ha.hpp"
#include "network/captive_dns.hpp"
#include "system/adapter_version.hpp"
#include "system/device_identity.hpp"
#include "system_monitor.hpp"
#include "wifi_network_manager.hpp"

#if !defined(EBUS_INTERNAL)
#include "legacy/bus_type.hpp"
#else
#include "command_manager.hpp"
#include "cron.hpp"
#endif

const AppConfig* DeviceStatus::config_ = nullptr;
uint32_t DeviceStatus::reset_code_ = 0;

namespace {

const AppConfig& statusConfig() { return DeviceStatus::config(); }

struct StatusInfo {
  static void toJson(ebus::detail::JsonWriter& writer) {
    auto scope = writer.objectScope();
    writer.writeField("reset_code", DeviceStatus::resetCode());
    writer.writeField("uptime",
                      static_cast<uint32_t>(esp_timer_get_time() / 1000ULL));
  }
};

struct HeapStatus {
  static void toJson(ebus::detail::JsonWriter& writer) {
    auto scope = writer.objectScope();
    multi_heap_info_t info;
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    writer.writeField("total_free_bytes", info.total_free_bytes);
    writer.writeField("largest_free_block", info.largest_free_block);
    writer.writeField("minimum_free_bytes", info.minimum_free_bytes);
    writer.writeField("free_blocks", info.free_blocks);
    writer.writeField("total_blocks", info.total_blocks);
  }
};

#if !defined(EBUS_INTERNAL)
struct ArbitrationInfo {
  static void toJson(ebus::detail::JsonWriter& writer) {
    auto scope = writer.objectScope();
    writer.writeField("total", static_cast<int>(Bus.nbr_arbitrations_));
    writer.writeField("restarts1", static_cast<int>(Bus.nbr_restarts_1_));
    writer.writeField("restarts2", static_cast<int>(Bus.nbr_restarts_2_));
    writer.writeField("won1", static_cast<int>(Bus.nbr_won_1_));
    writer.writeField("won2", static_cast<int>(Bus.nbr_won_2_));
    writer.writeField("lost1", static_cast<int>(Bus.nbr_lost_1_));
    writer.writeField("lost2", static_cast<int>(Bus.nbr_lost_2_));
    writer.writeField("late", static_cast<int>(Bus.nbr_late_));
    writer.writeField("errors", static_cast<int>(Bus.nbr_errors_));
  }
};
#endif

struct FirmwareStatus {
  void toJson(ebus::detail::JsonWriter& writer) const {
    auto scope = writer.objectScope();
    writer.writeField("version", AUTO_VERSION);
    writer.writeField("esp_idf_version", esp_get_idf_version());
#if !defined(EBUS_INTERNAL)
    writer.writeField("async", static_cast<bool>(USE_ASYNCHRONOUS));
    writer.writeField("software_serial",
                      static_cast<bool>(USE_SOFTWARE_SERIAL));
#endif
    writer.writeField("unique_id", getUniqueId());
    writer.writeField("adapter_hw_version", getAdapterHwVersionString());
    writer.writeField("adapter_hw_version_raw", getAdapterHwVersionRaw());
    writer.writeField("clock_speed", esp_clk_cpu_freq() / 1000000U);
    writer.writeField("apb_speed", esp_clk_apb_freq());
  }
};

struct ChipStatus {
  static void toJson(ebus::detail::JsonWriter& writer) {
    auto scope = writer.objectScope();
    esp_chip_info_t chip_info{};
    esp_chip_info(&chip_info);
    uint32_t flash_size = 0;
    if (esp_flash_default_chip != nullptr)
      esp_flash_get_size(esp_flash_default_chip, &flash_size);
    writer.writeField("chip_revision", static_cast<int>(chip_info.revision));
    writer.writeField("flash_size", flash_size);
  }
};

struct WifiStatus {
  static void toJson(ebus::detail::JsonWriter& writer) {
    auto scope = writer.objectScope();
    writer.writeField("last_connect", WifiNetworkManager::getLastConnect());
    writer.writeField("reconnect_count",
                      WifiNetworkManager::getReconnectCount());
    writer.writeField("rssi", WifiNetworkManager::RSSI());
    if (WifiNetworkManager::isStaticIpEnabled()) {
      writer.writeField("static_ip", true);
      writer.writeField("ip_address",
                        WifiNetworkManager::getConfiguredIpAddress());
      writer.writeField("gateway", WifiNetworkManager::getConfiguredGateway());
      writer.writeField("netmask", WifiNetworkManager::getConfiguredNetmask());
      writer.writeField("dns1", WifiNetworkManager::getConfiguredDns1());
      writer.writeField("dns2", WifiNetworkManager::getConfiguredDns2());
    } else {
      esp_netif_ip_info_t staIpInfo{};
      const bool hasStaIp = WifiNetworkManager::getStaIpInfo(&staIpInfo);
      esp_ip4_addr_t dnsMain{}, dnsBackup{};
      const bool hasDnsMain = WifiNetworkManager::getDnsIp(0, &dnsMain);
      const bool hasDnsBackup = WifiNetworkManager::getDnsIp(1, &dnsBackup);
      writer.writeField("static_ip", false);
      writer.writeField(
          "ip_address",
          hasStaIp ? WifiNetworkManager::ipToString(staIpInfo.ip) : "");
      writer.writeField(
          "gateway",
          hasStaIp ? WifiNetworkManager::ipToString(staIpInfo.gw) : "");
      writer.writeField(
          "netmask",
          hasStaIp ? WifiNetworkManager::ipToString(staIpInfo.netmask) : "");
      writer.writeField(
          "dns1", hasDnsMain ? WifiNetworkManager::ipToString(dnsMain) : "");
      writer.writeField("dns2", hasDnsBackup
                                    ? WifiNetworkManager::ipToString(dnsBackup)
                                    : "");
    }
    writer.writeField("ssid", WifiNetworkManager::SSID());
    writer.writeField("bssid", WifiNetworkManager::BSSIDstr());
    writer.writeField("channel", WifiNetworkManager::channel());
    writer.writeField("hostname", WifiNetworkManager::getHostname());
    writer.writeField("mac_address", WifiNetworkManager::macAddress());
  }
};

#if defined(EBUS_INTERNAL)
struct SntpStatus {
  static void toJson(ebus::detail::JsonWriter& writer) {
    auto scope = writer.objectScope();
    writer.writeField("enabled", statusConfig().sntp.enabled);
    const char* activeSntpServer = esp_sntp_getservername(0);
    if (activeSntpServer != nullptr) {
      writer.writeField("server", activeSntpServer);
    } else {
      writer.writeField("server", statusConfig().sntp.server.c_str());
    }
    writer.writeField("timezone", statusConfig().sntp.timezone.c_str());
  }
};

struct EbusStatus {
  static void toJson(ebus::detail::JsonWriter& w) {
    auto obj_scope = w.objectScope();
    w.writeField("pwm", statusConfig().pwm.value);
    w.writeField("ebus_address", statusConfig().bus.address.c_str());
    w.writeField("bus_window", statusConfig().bus.window_us);
    w.writeField("bus_offset", statusConfig().bus.offset_us);
  }
};

struct ScheduleStatus {
  static void toJson(ebus::detail::JsonWriter& w) {
    auto obj_scope = w.objectScope();
    w.writeField("system_inquiry", statusConfig().bus.system_inquiry);
    w.writeField("system_response", statusConfig().bus.system_response);
    w.writeField("scan_on_startup", statusConfig().bus.scan_on_startup);
    w.writeField("active_commands",
                 static_cast<uint32_t>(commandManager.getActiveCommands()));
    w.writeField("passive_commands",
                 static_cast<uint32_t>(commandManager.getPassiveCommands()));
  }
};

struct MqttStatus {
  static void toJson(ebus::detail::JsonWriter& w) {
    auto obj_scope = w.objectScope();
    w.writeField("enabled", mqtt.isEnabled());
    w.writeField("server", statusConfig().mqtt.server.c_str());
    w.writeField("user", statusConfig().mqtt.user.c_str());
    w.writeField("connected", mqtt.isConnected());
  }
};

struct HaStatus {
  static void toJson(ebus::detail::JsonWriter& w) {
    auto obj_scope = w.objectScope();
    w.writeField("enabled", mqttha.isEnabled());
  }
};

struct SocketsStatus {
  static void toJson(ebus::detail::JsonWriter& w) {
    auto obj_scope = w.objectScope();
    int detected = 0;
    int connected = 0;
    SystemMonitor::getSocketStatus(detected, connected);
    w.writeField("detected", detected);
    w.writeField("connected", connected);
    w.writeField("max", CONFIG_LWIP_MAX_SOCKETS);
  }
};
#endif

}  // namespace

void DeviceStatus::setConfig(const AppConfig* config) { config_ = config; }

void DeviceStatus::setResetCode(uint32_t code) { reset_code_ = code; }

const AppConfig& DeviceStatus::config() { return *config_; }

uint32_t DeviceStatus::resetCode() { return reset_code_; }

void DeviceStatus::fetchStatus(const ebus::JsonChunkVisitor& visitor) {
  ebus::detail::JsonWriter writer(visitor);
  auto scope = writer.objectScope();
  writer.writeField("status", StatusInfo{});
  writer.writeField("heap", HeapStatus{});

#if !defined(EBUS_INTERNAL)
  writer.writeField("arbitration", ArbitrationInfo{});
#endif
  writer.writeField("firmware", FirmwareStatus{});
  writer.writeField("chip", ChipStatus{});
  writer.writeField("wifi", WifiStatus{});

#if defined(EBUS_INTERNAL)
  writer.writeField("sntp", SntpStatus{});
  writer.writeField("ebus", EbusStatus{});
  writer.writeField("schedule", ScheduleStatus{});
  writer.writeField("mqtt", MqttStatus{});
  writer.writeField("home_assistant", HaStatus{});
  writer.writeField("sockets", SocketsStatus{});
#endif
}

#if defined(EBUS_INTERNAL)
void DeviceStatus::fetchAppStatus(const ebus::JsonChunkVisitor& visitor) {
  ebus::detail::JsonWriter writer(visitor);
  auto scope = writer.objectScope();

  auto addThread = [&](const char* name, TaskHandle_t handle,
                       uint32_t stack_size) {
    if (!handle) return;
    ebus::ThreadStatus ts(
        name, static_cast<int32_t>(stack_size),
        static_cast<int32_t>(uxTaskGetStackHighWaterMark(handle) *
                             sizeof(StackType_t)));
    writer.writeValue(ts);
  };

  writer.appendKey("threads");
  {
    auto array = writer.arrayScope();
    addThread("mqtt", mqtt.getTaskHandle(), app::limits::Task::mqtt_stack);
    addThread("cron", cron.getTaskHandle(), app::limits::Task::cron_stack);
    addThread("logger", logger.getTaskHandle(),
              app::limits::Task::logger_stack);
    addThread("dns", getCaptiveDnsTaskHandle(), app::limits::Task::dns_stack);
    addThread("espota", espOtaManager.getTaskHandle(),
              app::limits::Task::espota_stack);
    addThread("status_led", WifiNetworkManager::getStatusLedTaskHandle(),
              app::limits::Task::status_led_stack);
    addThread("system_monitor", SystemMonitor::task_handle(),
              app::limits::Task::system_monitor_stack);
  }

  writer.appendKey("queues");
  {
    auto array = writer.arrayScope();
    auto addQueue = [&](const char* qname, size_t size, size_t cap,
                        size_t max_size) {
      ebus::QueueStatus qs(qname, size, cap, max_size);
      writer.writeValue(qs);
    };

    addQueue("mqtt_out", mqtt.getOutgoingQueueSize(),
             mqtt.getOutgoingQueueCapacity(),
             mqtt.getOutgoingQueueHighWatermark());

    addQueue("logger", logger.getQueueSize(), logger.getQueueCapacity(),
             logger.getQueueHighWatermark());

    addQueue("system_monitor_log", SystemMonitor::getLogQueueSize(),
             SystemMonitor::getLogQueueCapacity(),
             SystemMonitor::getLogQueueHighWatermark());

    addQueue("system_monitor_protocol", SystemMonitor::getProtocolQueueSize(),
             SystemMonitor::getProtocolQueueCapacity(),
             SystemMonitor::getProtocolQueueHighWatermark());
  }
}
#endif
