#include "api/bus_api.hpp"

#if defined(EBUS_INTERNAL)

#include "app/ebus_accessor.hpp"
#include "config/app_config.hpp"
#include "network/http.hpp"
#include "network/http_utils.hpp"
#include "system/device_status.hpp"

BusApi::BusApi() {}

bool BusApi::registerHandlers(httpd_handle_t server) {
  if (server == nullptr) return false;

  RegisterUri("/api/v1/bus", HTTP_GET, handleBus);
  RegisterUri("/api/v1/bus/devices", HTTP_GET, handleDevices);
  RegisterUri("/api/v1/bus/devices/scan", HTTP_POST, handleDevicesScan);
  RegisterUri("/api/v1/bus/devices/scan/full", HTTP_POST,
              handleDevicesScanFull);

  return true;
}

esp_err_t BusApi::handleBus(httpd_req_t* req) {
  httpd_resp_set_type(req, "application/json;charset=utf-8");
  HttpUtils::applyCustomHeaders(req);
  auto sender = [req](std::string_view chunk) {
    httpd_resp_send_chunk(req, chunk.data(), chunk.size());
  };
  ebus::detail::JsonWriter writer(sender);
  {
    auto scope = writer.objectScope();
    // Metrics slices share this writer sequentially (no nesting: each
    // toJson completes before the next starts).
    getEbusController().fetchMetrics([&](const ebus::Metrics& m) {
      writer.appendKey("handler");
      m.handler.toJson(writer);
      writer.appendKey("request");
      m.request.toJson(writer);
      writer.appendKey("bus");
      m.bus.toJson(writer);
      writer.appendKey("device_counts");
      m.devices.toJson(writer);
    });
    // Device rows stream through their own writer; flush the key first
    // so chunk order stays valid, then complete the value state.
    writer.appendKey("devices");
    writer.flush();
    getEbusController().fetchDevices(sender);
    writer.externalValue();
    {
      auto config = writer.objectScope("config");
      const AppConfig& cfg = DeviceStatus::config();
      writer.writeField("address", cfg.bus.address.c_str());
      writer.writeField("window_us", cfg.bus.window_us);
      writer.writeField("offset_us", cfg.bus.offset_us);
      writer.writeField("pwm", cfg.pwm.value);
    }
  }
  httpd_resp_send_chunk(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t BusApi::handleDevices(httpd_req_t* req) {
  httpd_resp_set_type(req, "application/json;charset=utf-8");
  HttpUtils::applyCustomHeaders(req);
  getEbusController().fetchDevices([req](std::string_view chunk) {
    httpd_resp_send_chunk(req, chunk.data(), chunk.size());
  });
  httpd_resp_send_chunk(req, nullptr, 0);
  return ESP_OK;
}

esp_err_t BusApi::handleDevicesScan(httpd_req_t* req) {
  getEbusController().scanObservedDevices();
  HttpUtils::sendSuccessResponse(req, "scan", "initiated");
  return ESP_OK;
}

esp_err_t BusApi::handleDevicesScanFull(httpd_req_t* req) {
  getEbusController().initFullScan(true);
  HttpUtils::sendSuccessResponse(req, "scan_full", "initiated");
  return ESP_OK;
}

#endif
