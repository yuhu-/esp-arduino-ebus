#include "api/system_api.hpp"

#if defined(EBUS_INTERNAL)

#if __has_include("app/build_info_gen.hpp")
#include "app/build_info_gen.hpp"
#endif

#include <esp_chip_info.h>
#include <esp_flash.h>
#include <esp_heap_caps.h>
#include <esp_idf_version.h>
#include <esp_private/esp_clk.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app/app_limits.hpp"
#include "app/cron.hpp"
#include "app/mqtt.hpp"
#include "network/captive_dns.hpp"
#include "network/http.hpp"
#include "network/http_utils.hpp"
#include "network/wifi_network_manager.hpp"
#include "system/adapter_version.hpp"
#include "system/device_identity.hpp"
#include "system/device_status.hpp"
#include "system/esp_ota_manager.hpp"
#include "system/logger.hpp"
#include "system/system_monitor.hpp"

SystemApi::SystemApi() {}

bool SystemApi::registerHandlers(httpd_handle_t server) {
  if (server == nullptr) return false;

  RegisterUri("/api/v1/system", HTTP_GET, handleSystem);

  return true;
}

esp_err_t SystemApi::handleSystem(httpd_req_t* req) {
  httpd_resp_set_type(req, "application/json;charset=utf-8");
  HttpUtils::applyCustomHeaders(req);
  ebus::detail::JsonWriter writer([req](std::string_view chunk) {
    httpd_resp_send_chunk(req, chunk.data(), chunk.size());
  });
  {
    auto scope = writer.objectScope();
    {
      auto firmware = writer.objectScope("firmware");
      writer.writeField("version", AUTO_VERSION);
#if __has_include("app/build_info_gen.hpp")
      writer.writeField("build", build_time_str);
#else
      writer.writeField("build", __DATE__ " " __TIME__);
#endif
      writer.writeField("esp_idf_version", esp_get_idf_version());
      writer.writeField("unique_id", getUniqueId());
      writer.writeField("adapter_hw_version", getAdapterHwVersionString());
      writer.writeField("adapter_hw_version_raw", getAdapterHwVersionRaw());
      writer.writeField("clock_speed", esp_clk_cpu_freq() / 1000000U);
      writer.writeField("apb_speed", esp_clk_apb_freq());
    }
    {
      auto chip = writer.objectScope("chip");
      esp_chip_info_t chip_info{};
      esp_chip_info(&chip_info);
      writer.writeField("chip_revision", chip_info.revision);
      uint32_t flash_size = 0;
      if (esp_flash_default_chip != nullptr &&
          esp_flash_get_size(esp_flash_default_chip, &flash_size) == ESP_OK) {
        writer.writeField("flash_size", flash_size);
      }
    }
    writer.writeField("uptime",
                      static_cast<uint64_t>(esp_timer_get_time() / 1000ULL));
    writer.writeField("reset_code", DeviceStatus::resetCode());
    {
      auto heap = writer.objectScope("heap");
      multi_heap_info_t info;
      heap_caps_get_info(&info, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
      {
        auto current = writer.objectScope("current");
        writer.writeField("free", info.total_free_bytes);
        writer.writeField("largest", info.largest_free_block);
        writer.writeField("min", info.minimum_free_bytes);
      }
      SystemMonitor::HeapSample trend[SystemMonitor::heap_trend_capacity];
      size_t n = DeviceStatus::monitor().fetchHeapTrend(
          trend, SystemMonitor::heap_trend_capacity);
      auto arr = writer.arrayScope("trend");
      for (size_t i = 0; i < n; ++i) {
        auto item = writer.objectScope();
        writer.writeField("uptime", trend[i].uptime_seconds);
        writer.writeField("free", trend[i].free_bytes);
        writer.writeField("min", trend[i].min_bytes);
        writer.writeField("largest", trend[i].largest_block);
      }
    }
    {
      auto threads = writer.arrayScope("threads");
      auto addThread = [&](const char* name, TaskHandle_t handle,
                           uint32_t stack_size) {
        if (!handle) return;
        ebus::ThreadStatus ts(
            name, static_cast<int32_t>(stack_size),
            static_cast<int32_t>(uxTaskGetStackHighWaterMark(handle) *
                                 sizeof(StackType_t)));
        writer.writeValue(ts);
      };
      addThread("mqtt", DeviceStatus::mqtt().getTaskHandle(),
                app::limits::Task::mqtt_stack);
      addThread("cron", cron.getTaskHandle(), app::limits::Task::cron_stack);
      addThread("logger", logger.getTaskHandle(),
                app::limits::Task::logger_stack);
      addThread("dns", getCaptiveDnsTaskHandle(), app::limits::Task::dns_stack);
      addThread("espota", DeviceStatus::espOtaManager().getTaskHandle(),
                app::limits::Task::espota_stack);
      addThread("status_led", WifiNetworkManager::getStatusLedTaskHandle(),
                app::limits::Task::status_led_stack);
      addThread("system_monitor", DeviceStatus::monitor().task_handle(),
                app::limits::Task::system_monitor_stack);
    }
    {
      auto logger_scope = writer.objectScope("logger");
      writer.writeField("ring_overwrites", logger.getRingOverwriteCount());
      writer.writeField("print_drops", logger.getPrintDropCount());
    }
  }
  httpd_resp_send_chunk(req, nullptr, 0);
  return ESP_OK;
}

#endif
