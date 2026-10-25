#include "zion592_wire.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void feed(z592_transfer_t *t, unsigned type, const uint8_t *data, unsigned n) {
    uint8_t begin[]={0x0a,(uint8_t)type,(uint8_t)n,(uint8_t)(n>>8)};
    assert(z592_begin(t,begin,sizeof(begin)));
    for (unsigned at=0;at<n;) {
        unsigned len=n-at>17?17:n-at;
        uint8_t p[20]={0x0b,(uint8_t)at,(uint8_t)(at>>8)};
        memcpy(p+3,data+at,len); assert(z592_chunk(t,p,len+3)); at+=len;
    }
}
int main(void) {
    assert(z592_connection_usable(0,true));
    assert(z592_connection_usable(0x1a,true));
    assert(!z592_connection_usable(0,false));
    assert(!z592_connection_usable(0x1a,false));
    assert(!z592_connection_usable(0x3e,true));
    assert(!z592_connection_usable(531,true));
    z592_transfer_t t={0}; z592_request_t r;
    uint8_t wifi[]={3,0,'l','a','b'};
    feed(&t,1,wifi,sizeof(wifi)); assert(z592_commit(&t,&r));
    assert(!strcmp(r.ssid,"lab") && !r.password[0]);
    assert(!z592_commit(&t,&r));
    uint8_t payload[Z592_TRANSFER_MAX]={0};
    payload[2]=1; payload[6]=2; memset(payload+8,0x35,32);
    const char *url="https://example.com/public.bin";
    memcpy(payload+40,url,strlen(url));
    feed(&t,2,payload,40+(unsigned)strlen(url)); assert(z592_commit(&t,&r));
    assert(r.source_offset==0x10000 && r.file_size==0x20000 && !strcmp(r.url,url));
    uint8_t begin[]={0x0a,2,0xff,0xff}; assert(!z592_begin(&t,begin,4));
    begin[2]=5;begin[3]=0;assert(z592_begin(&t,begin,4));
    uint8_t wrong[]={0x0b,1,0,0}; assert(!z592_chunk(&t,wrong,4));
    assert(!z592_commit(&t,&r));
    payload[40]='x'; feed(&t,2,payload,40+(unsigned)strlen(url)); assert(!z592_commit(&t,&r));
    wifi[2]=0;feed(&t,1,wifi,sizeof(wifi)); assert(!z592_commit(&t,&r));
    memset(payload,0,sizeof(payload));payload[6]=1;memcpy(payload+40,"https://",8);
    memset(payload+48,'a',Z592_URL_MAX-8);
    feed(&t,2,payload,sizeof(payload));assert(z592_commit(&t,&r));assert(strlen(r.url)==512);
    z592_abort(&t);assert(!t.received && !t.expected && !t.bytes[40]);
    puts("Zion592 wire: PASS (fragmentation, lengths, ordering, bounds, invalid URLs, reset)");
}
