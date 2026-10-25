#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define ZION_TITLE_DEPTH 24
#define ZION_TITLE_OBJECT 512
#define ZION_FIRMWARE_OBJECT 1024
typedef struct {char key[12];uint8_t tag,stage,key_len;bool array,key_overflow;} zion_title_frame_t;
typedef struct {
    zion_title_frame_t frames[ZION_TITLE_DEPTH];
    char title[ZION_TITLE_OBJECT],firmware[ZION_FIRMWARE_OBJECT];
    size_t title_size,firmware_size;
    unsigned depth,capture_depth,capture;
    bool in_string,escaped,key_string,key_escape,primitive,failed,title_done,firmware_done;
} zion_title_parser_t;
/* Only the official public community-download route has an unambiguous slug. */
bool zion_title_lookup_url(const char *firmware_url,char *out,size_t capacity);
void zion_title_init(zion_title_parser_t *parser);
bool zion_title_feed(zion_title_parser_t *parser,const char *data,size_t size);
bool zion_title_ready(const zion_title_parser_t *parser);
/* Match BOTH published size and SHA before treating a community title as fact. */
bool zion_title_result(const zion_title_parser_t *parser,uint32_t size,const uint8_t sha256[32],char out[33]);
