#include "zion_portal.h"
#include <string.h>
void zion_portal_begin(zion_portal_t *p) {*p=(zion_portal_t){.phase=ZION_PORTAL_CONNECTING};}
void zion_portal_result(zion_portal_t *p,int error,uint64_t now) {
    p->phase=error?ZION_PORTAL_FAILED:ZION_PORTAL_READY;p->error=error;
    p->close_at=error?0:now+ZION_PORTAL_RESULT_GRACE_MS;
}
void zion_portal_finish(zion_portal_t *p,uint64_t now) {
    if(p->phase==ZION_PORTAL_READY && p->close_at>now+ZION_PORTAL_EXIT_GRACE_MS)
        p->close_at=now+ZION_PORTAL_EXIT_GRACE_MS;
}
bool zion_portal_expired(const zion_portal_t *p,uint64_t now) {return p->close_at && now>=p->close_at;}
static unsigned be16(const uint8_t *p) {return ((unsigned)p[0]<<8)|p[1];}
size_t zion_portal_dns_reply(const uint8_t *q,size_t n,uint8_t *out,size_t cap,const uint8_t ip[4]) {
    if(!q || !out || !ip || n<17 || n>ZION_PORTAL_DNS_MAX || cap<12 ||
       (q[2]&0xfa) || (q[3]&0x0f) || be16(q+4)!=1 || be16(q+6) || be16(q+8)) return 0;
    size_t at=12,labels=0;
    while(at<n && q[at]) {
        unsigned length=q[at++];
        if(length>63 || length>n-at || (at-12)+length>254) return 0;
        at+=length;labels++;
    }
    if(!labels || at>=n || n-at<5) return 0;
    at++;unsigned type=be16(q+at),class_id=be16(q+at+2);at+=4;
    bool answer=type==1 && class_id==1;
    size_t length=at+(answer?16:0);
    if(length>cap) return 0;
    /* Strip EDNS/additional records and retain only the parsed question. */
    memcpy(out,q,at);out[2]=(uint8_t)(0x84|(q[2]&1));out[3]=0;
    out[6]=0;out[7]=answer?1:0;memset(out+8,0,4);
    if(answer) {
        static const uint8_t record[]={0xc0,0x0c,0,1,0,1,0,0,0,0,0,4};
        memcpy(out+at,record,sizeof(record));memcpy(out+at+sizeof(record),ip,4);
    }
    return length;
}
