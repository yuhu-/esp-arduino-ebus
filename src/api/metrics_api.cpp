#include "api/metrics_api.hpp"

#if defined(EBUS_INTERNAL)

#include "app/ebus_accessor.hpp"
#include "app/mqtt.hpp"
#include "network/http.hpp"
#include "network/http_utils.hpp"
#include "system/device_status.hpp"
#include "system/logger.hpp"
#include "system/system_monitor.hpp"

namespace {

// cppcheck-suppress syntaxError
extern const char metrics_html_start[] asm("_binary_metrics_html_start");

}  // namespace

MetricsApi::MetricsApi() {}

bool MetricsApi::registerHandlers(httpd_handle_t server) {
  if (server == nullptr) return false;

  RegisterUri("/metrics", HTTP_GET, handleMetricsPage);
  RegisterUri("/api/v1/metrics/lib", HTTP_GET, handleMetrics);
  RegisterUri("/api/v1/metrics/app", HTTP_GET, handleMetricsApp);
  RegisterUri("/api/v1/metrics/lib/reset", HTTP_POST, handleMetricsReset);

  return true;
}

esp_err_t MetricsApi::handleMetricsPage(httpd_req_t* req) {
  HttpUtils::sendResponse(req, "200 OK", "text/html", metrics_html_start);
  return ESP_OK;
}

esp_err_t MetricsApi::handleMetrics(httpd_req_t* req) {
  httpd_resp_set_type(req, "application/json;charset=utf-8");
  HttpUtils::applyCustomHeaders(req);
  getEbusController().fetchMetrics([req](std::string_view chunk) {
    httpd_resp_send_chunk(req, chunk.data(), chunk.size());
  });
  httpd_resp_send_chunk(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t MetricsApi::handleMetricsApp(httpd_req_t* req) {
  httpd_resp_set_type(req, "application/json;charset=utf-8");
  HttpUtils::applyCustomHeaders(req);
  ebus::detail::JsonWriter writer([req](std::string_view chunk) {
    httpd_resp_send_chunk(req, chunk.data(), chunk.size());
  });
  {
    auto scope = writer.objectScope();
    {
      auto logger_scope = writer.objectScope("logger");
      writer.writeField("ring_overwrites", logger.getRingOverwriteCount());
      writer.writeField("print_drops", logger.getPrintDropCount());
    }
    {
      auto heap_scope = writer.objectScope("heap");
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
      auto mqtt_scope = writer.objectScope("mqtt");
      const Mqtt& mqtt = DeviceStatus::mqtt();
      writer.writeField("published", mqtt.getPublishedCount());
      writer.writeField("publish_failed", mqtt.getPublishFailedCount());
      writer.writeField("queue_dropped", mqtt.getQueueDropCount());
      writer.writeField("connects", mqtt.getConnectCount());
    }
  }
  httpd_resp_send_chunk(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t MetricsApi::handleMetricsReset(httpd_req_t* req) {
  getEbusController().resetMetrics();
  HttpUtils::sendSuccessResponse(req, "reset");
  return ESP_OK;
}

#endif
