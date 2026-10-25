#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define Z592_URL_MAX 512u
#define Z592_TRANSFER_MAX (Z592_URL_MAX + 40u)
typedef struct {
    uint8_t bytes[Z592_TRANSFER_MAX];
    uint16_t expected, received;
    uint8_t type;
} z592_transfer_t;
typedef struct {
    uint8_t type;
    char ssid[33], password[65], url[Z592_URL_MAX + 1];
    uint32_t source_offset, file_size;
    uint8_t sha256[32];
} z592_request_t;
void z592_abort(z592_transfer_t *transfer);
bool z592_begin(z592_transfer_t *transfer, const uint8_t *packet, size_t size);
bool z592_chunk(z592_transfer_t *transfer, const uint8_t *packet, size_t size);
bool z592_commit(z592_transfer_t *transfer, z592_request_t *request);
bool z592_connection_usable(int connect_status, bool live_connection_found);
