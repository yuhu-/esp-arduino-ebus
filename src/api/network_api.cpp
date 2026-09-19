#include "api/network_api.hpp"

#if defined(EBUS_INTERNAL)

#include <esp_timer.h>

#include "app/mqtt.hpp"
#include "config/app_config.hpp"
#include "network/http.hpp"
#include "network/http_utils.hpp"
#include "network/sntp.hpp"
#include "network/wifi_network_manager.hpp"
#include "system/device_status.hpp"
#include "system/system_monitor.hpp"

NetworkApi::NetworkApi() {}

bool NetworkApi::registerHandlers(httpd_handle_t server) {
  if (server == nullptr) return false;

  RegisterUri("/api/v1/network", HTTP_GET, handleNetwork);

  return true;
}

esp_err_t NetworkApi::handleNetwork(httpd_req_t* req) {
  httpd_resp_set_type(req, "application/json;charset=utf-8");
  HttpUtils::applyCustomHeaders(req);
  ebus::detail::JsonWriter writer([req](std::string_view chunk) {
    httpd_resp_send_chunk(req, chunk.data(), chunk.size());
  });
  {
    auto scope = writer.objectScope();
    {
      auto wifi = writer.objectScope("wifi");
      appendWifiStatus(writer);
    }
    {
      auto mqtt = writer.objectScope("mqtt");
      appendMqttStatus(writer, DeviceStatus::config().mqtt);
      writer.writeField("published", DeviceStatus::mqtt().getPublishedCount());
      writer.writeField("publish_failed",
                        DeviceStatus::mqtt().getPublishFailedCount());
      writer.writeField("queue_dropped",
                        DeviceStatus::mqtt().getQueueDropCount());
      writer.writeField("connects", DeviceStatus::mqtt().getConnectCount());
    }
    {
      auto sockets = writer.objectScope("sockets");
      int detected = 0;
      int connected = 0;
      DeviceStatus::monitor().getSocketStatus(detected, connected);
      writer.writeField("detected", detected);
      writer.writeField("connected", connected);
      writer.writeField("max", CONFIG_LWIP_MAX_SOCKETS);
    }
    {
      auto sntp = writer.objectScope("sntp");
      appendSntpStatus(writer, DeviceStatus::config().sntp);
    }
  }
  httpd_resp_send_chunk(req, nullptr, 0);
  return ESP_OK;
}

#endif
