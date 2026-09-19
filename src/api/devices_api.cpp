#include "api/devices_api.hpp"

#if defined(EBUS_INTERNAL)

#include "network/http.hpp"
#include "network/http_utils.hpp"

namespace {

// cppcheck-suppress syntaxError
extern const char devices_html_start[] asm("_binary_devices_html_start");

}  // namespace

DevicesApi::DevicesApi() {}

bool DevicesApi::registerHandlers(httpd_handle_t server) {
  if (server == nullptr) return false;

  RegisterUri("/devices", HTTP_GET, handleDevicesPage);

  return true;
}

esp_err_t DevicesApi::handleDevicesPage(httpd_req_t* req) {
  HttpUtils::sendResponse(req, "200 OK", "text/html", devices_html_start);
  return ESP_OK;
}

#endif