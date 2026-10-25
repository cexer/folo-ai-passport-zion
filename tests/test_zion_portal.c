#include "zion_portal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t random_state=42;
static uint8_t random_byte(void) {random_state=random_state*1664525u+1013904223u;return (uint8_t)(random_state>>24);}
int main(void) {
    zion_portal_t p={0};
    assert(!zion_portal_expired(&p,999999));
    zion_portal_finish(&p,100);assert(!p.close_at);
    zion_portal_begin(&p);assert(p.phase==ZION_PORTAL_CONNECTING && !p.close_at);
    zion_portal_result(&p,123,100);assert(p.phase==ZION_PORTAL_FAILED && p.error==123 && !p.close_at);
    zion_portal_finish(&p,200);assert(!p.close_at);
    zion_portal_begin(&p);assert(!p.error);
    zion_portal_result(&p,0,1000);assert(p.phase==ZION_PORTAL_READY && !p.error);
    assert(!zion_portal_expired(&p,90999));assert(zion_portal_expired(&p,91000));
    zion_portal_finish(&p,2000);assert(p.close_at==12000);
    zion_portal_finish(&p,3000);assert(p.close_at==12000); /* refresh cannot extend the AP */
    assert(!zion_portal_expired(&p,11999));assert(zion_portal_expired(&p,12000));
    const uint8_t ip[]={192,168,4,1};
    uint8_t q[ZION_PORTAL_DNS_MAX]={0x12,0x34,1,0,0,1,0,0,0,0,0,0,3,'a','b','c',0,0,1,0,1};
    uint8_t out[ZION_PORTAL_DNS_MAX+2];memset(out,0xa5,sizeof(out));
    size_t n=zion_portal_dns_reply(q,21,out+1,ZION_PORTAL_DNS_MAX,ip);
    assert(n==37 && out[0]==0xa5 && out[ZION_PORTAL_DNS_MAX+1]==0xa5);
    uint8_t *r=out+1;
    assert(r[0]==0x12 && r[1]==0x34 && r[2]==0x85 && r[7]==1 && !r[11]);
    assert(!memcmp(r+n-4,ip,4));assert(!memcmp(r+12,q+12,9));
    for(size_t i=0;i<21;i++) assert(!zion_portal_dns_reply(q,i,r,ZION_PORTAL_DNS_MAX,ip));
    assert(!zion_portal_dns_reply(q,21,r,36,ip));
    q[18]=28;n=zion_portal_dns_reply(q,21,r,ZION_PORTAL_DNS_MAX,ip);assert(n==21 && !r[7]);
    q[18]=1;q[20]=3;n=zion_portal_dns_reply(q,21,r,ZION_PORTAL_DNS_MAX,ip);assert(n==21 && !r[7]);
    q[20]=1;q[11]=1; /* strip optional EDNS; do not reflect unparsed bytes */
    n=zion_portal_dns_reply(q,32,r,ZION_PORTAL_DNS_MAX,ip);assert(n==37 && !r[11]);
    q[12]=0xc0;assert(!zion_portal_dns_reply(q,21,r,ZION_PORTAL_DNS_MAX,ip));
    q[12]=64;assert(!zion_portal_dns_reply(q,21,r,ZION_PORTAL_DNS_MAX,ip));
    q[12]=3;q[2]=0x81;assert(!zion_portal_dns_reply(q,21,r,ZION_PORTAL_DNS_MAX,ip));
    q[2]=9;assert(!zion_portal_dns_reply(q,21,r,ZION_PORTAL_DNS_MAX,ip));
    q[2]=1;q[5]=2;assert(!zion_portal_dns_reply(q,21,r,ZION_PORTAL_DNS_MAX,ip));
    /* Fuzz bounded, malformed datagrams with output guard bytes. */
    for(unsigned run=0;run<50000;run++) {
        for(size_t i=0;i<sizeof(q);i++) q[i]=random_byte();
        size_t len=random_byte()*2u;
        memset(out,0xa5,sizeof(out));n=zion_portal_dns_reply(q,len,out+1,ZION_PORTAL_DNS_MAX,ip);
        assert(n<=ZION_PORTAL_DNS_MAX && out[0]==0xa5 && out[ZION_PORTAL_DNS_MAX+1]==0xa5);
    }
    puts("Zion portal: PASS (success/failure handoff deadlines, DNS A/AAAA, malformed packets, bounds, fuzz)");
}
