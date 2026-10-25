#pragma once
#include "esp_err.h"
#include <stdint.h>
esp_err_t zion_portal_dns_start(uint32_t ap_address);
void zion_portal_dns_stop(void);
