#pragma once

#if defined(EBUS_INTERNAL)

#include <esp_http_server.h>

class BusApi {
 public:
  explicit BusApi();

  bool registerHandlers(httpd_handle_t server);

 private:
  static esp_err_t handleBus(httpd_req_t* req);
  static esp_err_t handleDevices(httpd_req_t* req);
  static esp_err_t handleDevicesScan(httpd_req_t* req);
  static esp_err_t handleDevicesScanFull(httpd_req_t* req);
};

#endif
