#pragma once

#if defined(EBUS_INTERNAL)

#include <esp_http_server.h>

class SystemApi {
 public:
  explicit SystemApi();

  bool registerHandlers(httpd_handle_t server);

 private:
  static esp_err_t handleSystem(httpd_req_t* req);
};

#endif
