#include "zion592_wire.h"
#include <string.h>
bool z592_connection_usable(int status, bool live) {
    /* IDF NimBLE forwards remote-feature status through CONNECT even when the
     * link exists and ATT works. Accept only this measured exception, never an
     * arbitrary failed CONNECT merely because a stale descriptor is present. */
    return live && (status==0 || status==0x1a);
}
static uint16_t le16(const uint8_t *p) { return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
void z592_abort(z592_transfer_t *t) { memset(t, 0, sizeof(*t)); }
bool z592_begin(z592_transfer_t *t, const uint8_t *p, size_t n) {
    z592_abort(t);
    if (n != 4 || p[0] != 0x0a || (p[1] != 1 && p[1] != 2)) return false;
    uint16_t len=le16(p+2);
    if (!len || len>Z592_TRANSFER_MAX) return false;
    t->type=p[1]; t->expected=len; return true;
}
bool z592_chunk(z592_transfer_t *t, const uint8_t *p, size_t n) {
    if (n<4 || p[0]!=0x0b || !t->expected || le16(p+1)!=t->received ||
        n-3>(size_t)(t->expected-t->received)) return false;
    memcpy(t->bytes+t->received,p+3,n-3); t->received+=(uint16_t)(n-3); return true;
}
bool z592_commit(z592_transfer_t *t, z592_request_t *r) {
    memset(r,0,sizeof(*r));
    bool ok=false;
    if (!t->expected || t->received!=t->expected) { z592_abort(t); return false; }
    r->type=t->type;
    if (r->type==1 && t->expected>=2) {
        unsigned s=t->bytes[0], p=t->bytes[1];
        if (s && s<=32 && p<=64 && t->expected==2+s+p &&
            !memchr(t->bytes+2,0,s+p)) {
            memcpy(r->ssid,t->bytes+2,s); memcpy(r->password,t->bytes+2+s,p); ok=true;
        }
    } else if (r->type==2 && t->expected>40) {
        unsigned len=t->expected-40;
        r->source_offset=le32(t->bytes); r->file_size=le32(t->bytes+4);
        if (len<=Z592_URL_MAX && len>8 && !memcmp(t->bytes+40,"https://",8) &&
            !memchr(t->bytes+40,0,len) &&
            (r->source_offset==0 || r->source_offset==0x10000) &&
            r->file_size>r->source_offset && r->file_size<=0x800000) {
            memcpy(r->sha256,t->bytes+8,32); memcpy(r->url,t->bytes+40,len); ok=true;
        }
    }
    z592_abort(t);
    if (!ok) memset(r,0,sizeof(*r));
    return ok;
}
