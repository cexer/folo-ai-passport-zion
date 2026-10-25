#include "zion_title.h"
#include "meta_name.h"
#include "meta_store_json.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const char sample[]={
    "{\"ok\":true,\"title\":{\"zh\":\"wrong root\"},\"play\":{\"description\":\"escaped \\\"title\\\":{}\","
    "\"nested\":[{\"title\":{\"zh\":\"wrong nested\"}}],"
    "\"firmware\":{\"size\":2483680,\"sha256\":\"0000000000000000000000000000000000000000000000000000000000000000\"},"
    "\"title\":{\"zh\":\"小智情绪胸牌\",\"en\":\"Badge\"}}}"
};
int main(void) {
    char url[192],name[33];uint8_t sha[32]={0};
    assert(zion_title_lookup_url("https://ai-passport.folotoy.cn/api/download/community/community-cc0ff250",url,sizeof(url)));
    assert(!strcmp(url,"https://ai-passport.folotoy.cn/api/plays/community-cc0ff250"));
    assert(!zion_title_lookup_url("https://ai-passport.folotoy.cn.evil/api/download/community/a",url,sizeof(url)));
    assert(!zion_title_lookup_url("https://ai-passport.folotoy.cn/api/download/community/../a",url,sizeof(url)));
    assert(!zion_title_lookup_url("https://example.invalid/file.bin",url,sizeof(url)));
    assert(!zion_title_lookup_url("https://ai-passport.folotoy.cn/api/download/community/a/b",url,sizeof(url)));
    for(unsigned chunk=1;chunk<=1024;chunk=chunk==1?7:chunk==7?37:chunk==37?1024:1025) {
        zion_title_parser_t p;zion_title_init(&p);
        for(size_t at=0;at<strlen(sample);) {
            size_t n=strlen(sample)-at;if(n>chunk) n=chunk;
            assert(zion_title_feed(&p,sample+at,n));at+=n;
        }
        assert(zion_title_result(&p,2483680,sha,name) && !strcmp(name,"小智情绪胸牌"));
        strcpy(name,"unchanged");assert(!zion_title_result(&p,1,sha,name) && !strcmp(name,"unchanged"));
        sha[0]=1;assert(!zion_title_result(&p,2483680,sha,name));sha[0]=0;
    }
    const char *unicode="{\"name\":\"\\u5c0f\\u667a\"}";
    assert(meta_store_json_get_string(unicode,strlen(unicode),"name",name,sizeof(name)) && !strcmp(name,"小智"));
    const char *surrogate="{\"name\":\"\\ud83d\\ude00\"}";
    assert(meta_store_json_get_string(surrogate,strlen(surrogate),"name",name,sizeof(name)) && !strcmp(name,"\xf0\x9f\x98\x80"));
    const char *invalid="{\"name\":\"\\ud83dX\"}";
    assert(!meta_store_json_get_string(invalid,strlen(invalid),"name",name,sizeof(name)));
    zion_title_parser_t p;zion_title_init(&p);
    assert(zion_title_feed(&p,sample,20) && !zion_title_result(&p,2483680,sha,name));
    char huge[ZION_TITLE_OBJECT+128];memset(huge,'a',sizeof(huge));
    const char prefix[]="{\"play\":{\"title\":{\"zh\":\"";
    zion_title_init(&p);assert(zion_title_feed(&p,prefix,strlen(prefix)));
    assert(!zion_title_feed(&p,huge,sizeof(huge)) && !zion_title_result(&p,2483680,sha,name));
    puts("PASS community titles: official URL scope, streaming/nested/escaped JSON, size+SHA binding, Unicode, bounded memory");
}
