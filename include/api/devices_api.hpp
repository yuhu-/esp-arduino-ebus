#pragma once

#if defined(EBUS_INTERNAL)

#include <esp_http_server.h>

class DevicesApi {
 public:
  explicit DevicesApi();

  bool registerHandlers(httpd_handle_t server);

 private:
  static esp_err_t handleDevicesPage(httpd_req_t* req);
};

#endif