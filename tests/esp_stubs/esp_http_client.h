#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>
typedef struct host_http_client *esp_http_client_handle_t;
enum { HTTP_ADDR_TYPE_INET=2,HTTP_TLS_DYN_BUF_RX_STATIC=1 };
#define ESP_ERR_HTTP_EAGAIN 0x7007
typedef struct {
    const char *url;
    esp_err_t (*crt_bundle_attach)(void *);
    int timeout_ms,buffer_size,buffer_size_tx,addr_type,tls_dyn_buf_strategy;
    bool disable_auto_redirect;
} esp_http_client_config_t;
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *config);
esp_err_t esp_http_client_open(esp_http_client_handle_t client,int write_len);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t client);
int esp_http_client_get_status_code(esp_http_client_handle_t client);
int esp_http_client_read(esp_http_client_handle_t client,char *buffer,int size);
esp_err_t esp_http_client_close(esp_http_client_handle_t client);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client);

esp_err_t esp_http_client_get_and_clear_last_tls_error(esp_http_client_handle_t client,int *code,int *flags);
