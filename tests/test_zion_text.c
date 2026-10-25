#include "zion_text.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void)
{
    assert(strcmp(zion_text_zh(NULL), "") == 0);
    assert(strcmp(zion_text_zh("waiting for phone upload"), "等待手机上传") == 0);
    assert(strcmp(zion_text_zh("partition missing"), "分区层：未找到分区") == 0);
    assert(strstr(zion_text_zh("WiFi failed (code 123)."), "123"));
    assert(strcmp(zion_text_zh("ESP_ERR_INVALID_STATE"), "ESP_ERR_INVALID_STATE") == 0);
    puts("Zion Chinese presentation: PASS (network, upload, layer-specific errors, codes)");
}
