#pragma once
#include "esp_err.h"
#include <stdbool.h>
esp_err_t zion_wifi_ap_start(char name[33],char password[9]);
void zion_wifi_ap_stop(void);
bool zion_service_wifi(const char *ssid,const char *password);
