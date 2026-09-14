// Host link stubs for HTTP server plumbing (never executed by Tier-1 tests).
// config_manager.cpp references these symbols; the real implementations need
// a live httpd server.

#include <string>

#include "network/http.hpp"
#include "network/http_utils.hpp"

bool RegisterUri(const char* uri, httpd_method_t method,
                 esp_err_t (*handler)(httpd_req_t*)) {
  (void)uri;
  (void)method;
  (void)handler;
  return true;
}

void SetupHttpHandlers() {}
void SetupHttpFallbackHandlers() {}

namespace HttpUtils {

void applyCustomHeaders(httpd_req_t* req) { (void)req; }

void sendErrorResponse(httpd_req_t* req, const char* status,
                       std::string_view id, std::string_view message) {
  (void)req;
  (void)status;
  (void)id;
  (void)message;
}

void sendSuccessResponse(httpd_req_t* req, std::string_view id,
                         std::string_view result, std::string_view message) {
  (void)req;
  (void)id;
  (void)result;
  (void)message;
}

void setCustomHeaders(const std::string& raw) { (void)raw; }

StreamingReader::StreamingReader(httpd_req_t* req) : req_(req) {}

StreamingReader::~StreamingReader() = default;

bool StreamingReader::feedAll() { return false; }

void StreamingReader::endOfInput() const {}

ebus::detail::JsonReader& StreamingReader::jsonReader() {
  return fallback_reader_;
}

}  // namespace HttpUtils
