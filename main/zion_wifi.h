#pragma once
#include "esp_err.h"
#include <stdbool.h>
#include "esp_wifi_types.h"
#define ZION_WIFI_SCAN_MAX 16u
esp_err_t zion_wifi_ap_start(char name[33],char password[9]);
void zion_wifi_ap_stop(void);
void zion_wifi_connect_result(esp_err_t error);
bool zion_wifi_ap_expired(void);
bool zion_service_wifi(const char *ssid,const char *password);
bool zion_service_scan(void);
void zion_wifi_scan_complete(const wifi_ap_record_t *records,unsigned count,esp_err_t error);
