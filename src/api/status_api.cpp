#include "api/status_api.hpp"

#include "network/http.hpp"
#include "network/http_utils.hpp"

namespace {

// cppcheck-suppress syntaxError
extern const char status_html_start[] asm("_binary_status_html_start");

}  // namespace

StatusApi::StatusApi() {}

bool StatusApi::registerHandlers(httpd_handle_t server) {
  if (server == nullptr) return false;

  RegisterUri("/status", HTTP_GET, handleStatusPage);

  return true;
}

esp_err_t StatusApi::handleStatusPage(httpd_req_t* req) {
  HttpUtils::sendResponse(req, "200 OK", "text/html", status_html_start);
  return ESP_OK;
}
