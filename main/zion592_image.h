#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "meta_carve.h"
#define Z592_SOURCE_APPS 8
typedef struct {
    uint32_t source,size,cursor,image_len;
    uint8_t prefix[112],segment[8],tail_magic[4];
    uint8_t segment_used,seen,count,subtype;
    bool header_ready,bad,done,present;
} z592_source_app_t;
typedef struct {
    uint32_t source,size,initial;
    uint8_t subtype;
    char label[17];
} z592_source_data_t;
typedef struct {
    uint32_t file_size,received;
    bool merged,table_ready;
    const char *error;
    uint8_t table[META_PT_SIZE];
    z592_source_app_t apps[Z592_SOURCE_APPS];
    z592_source_data_t data[META_DATA_MAX];
    uint8_t app_count,data_count;
    int selected;
} z592_image_t;
void z592_image_init(z592_image_t *p,uint32_t file_size,bool merged);
bool z592_image_feed(z592_image_t *p,const uint8_t *bytes,size_t size);
bool z592_image_finish(z592_image_t *p);
/* Allocate without touching Flash. replace=-1 creates; >=0 removes only that
 * APP and its owned DATA from the proposal. Caller must separately authorize it. */
bool z592_image_allocate(const z592_image_t *image,const meta_carve_t *current,
                         int replace,uint32_t play_id,meta_carve_t *next,int *slot);
/* One-pass reservation: choose the largest APP capacity that still permits all
 * declared owned DATA. No image bytes need to be buffered or downloaded twice.
 * The reserved APP shrinks to meta_carve_need(actual_image_len) at commit. */
bool z592_image_stream_plan(const z592_image_t *image,const meta_carve_t *current,
                            int replace,uint32_t play_id,meta_carve_t *next,int *slot);
/* -1 means no non-empty OTA header has arrived; -2 means conflicting/bad APP. */
int z592_image_stream_app(const z592_image_t *image);
uint32_t z592_image_minimum_image(const z592_source_app_t *app);
