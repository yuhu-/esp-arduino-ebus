#pragma once

// Host mock for ESP-IDF esp_http_server.h: types + no-op responses.
// Only what app TU declarations need to compile and link on host.

#include <sys/types.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct httpd_req httpd_req_t;
typedef struct httpd_data* httpd_handle_t;
typedef struct httpd_uri httpd_uri_t;

typedef enum {
  HTTP_GET = 0,
  HTTP_PUT,
  HTTP_POST,
  HTTP_DELETE,
  HTTP_HEAD,
  HTTP_OPTIONS,
  HTTP_PATCH,
} httpd_method_t;

inline esp_err_t httpd_resp_set_type(httpd_req_t* req, const char* type) {
  (void)req;
  (void)type;
  return ESP_OK;
}

inline esp_err_t httpd_resp_send_chunk(httpd_req_t* req, const char* buf,
                                       ssize_t buf_len) {
  (void)req;
  (void)buf;
  (void)buf_len;
  return ESP_OK;
}

#ifdef __cplusplus
}
#endif
