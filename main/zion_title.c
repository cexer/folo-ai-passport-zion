#include "zion_title.h"
#include "meta_name.h"
#include "meta_store_json.h"
#include <stdio.h>
#include <string.h>

enum { UNKNOWN,ROOT,PLAY,TITLE,FIRMWARE };
enum { KEY,COLON,VALUE,AFTER };
bool zion_title_lookup_url(const char *url,char *out,size_t capacity) {
    const char prefix[]="https://ai-passport.folotoy.cn/api/download/community/";
    if(!url || strncmp(url,prefix,sizeof(prefix)-1)) return false;
    const char *slug=url+sizeof(prefix)-1;size_t n=strcspn(slug,"?#");
    if(!n || n>80) return false;
    for(size_t i=0;i<n;i++) if(!((slug[i]>='a'&&slug[i]<='z') || (slug[i]>='0'&&slug[i]<='9') || slug[i]=='-')) return false;
    int used=snprintf(out,capacity,"https://ai-passport.folotoy.cn/api/plays/%.*s",(int)n,slug);
    return used>0 && (size_t)used<capacity;
}
void zion_title_init(zion_title_parser_t *p) {memset(p,0,sizeof(*p));}
static bool bad(zion_title_parser_t *p) {p->failed=true;return false;}
static bool capture(zion_title_parser_t *p,char c) {
    if(!p->capture) return true;
    char *out=p->capture==TITLE?p->title:p->firmware;
    size_t *n=p->capture==TITLE?&p->title_size:&p->firmware_size;
    size_t cap=p->capture==TITLE?sizeof(p->title):sizeof(p->firmware);
    if(*n>=cap) return bad(p);
    out[(*n)++]=c;return true;
}
bool zion_title_ready(const zion_title_parser_t *p) {return !p->failed && p->title_done && p->firmware_done;}
bool zion_title_feed(zion_title_parser_t *p,const char *data,size_t size) {
    if(p->failed) return false;
    for(size_t at=0;at<size;at++) {
        char c=data[at];
        if(!capture(p,c)) return false;
        zion_title_frame_t *f=p->depth?&p->frames[p->depth-1]:NULL;
        if(p->in_string) {
            if(p->escaped) {p->escaped=false;continue;}
            if(c=='\\') {p->escaped=true;if(p->key_string) p->key_escape=true;continue;}
            if(c=='"') {
                p->in_string=false;
                if(p->key_string) {f->stage=COLON;if(p->key_escape || f->key_overflow) f->key[0]=0;}
                else f->stage=AFTER;
                continue;
            }
            if((unsigned char)c<32) return bad(p);
            if(p->key_string && !p->key_escape) {
                if((size_t)f->key_len+1<sizeof(f->key)) {f->key[f->key_len++]=c;f->key[f->key_len]=0;}
                else f->key_overflow=true;
            }
            continue;
        }
        if(p->primitive) {
            if(c!=',' && c!='}' && c!=']' && c!=' ' && c!='\r' && c!='\n' && c!='\t') continue;
            p->primitive=false;if(!f) return bad(p);f->stage=AFTER;
        }
        if(c==' ' || c=='\r' || c=='\n' || c=='\t') continue;
        if(c=='"') {
            if(!f || (f->stage!=VALUE && (f->array || f->stage!=KEY))) return bad(p);
            p->in_string=true;p->key_string=!f->array && f->stage==KEY;p->key_escape=false;
            if(p->key_string) {f->key_len=0;f->key_overflow=false;f->key[0]=0;}
        } else if(c=='{' || c=='[') {
            if(p->depth>=ZION_TITLE_DEPTH || (f && f->stage!=VALUE)) return bad(p);
            unsigned tag=UNKNOWN;
            if(!f) {if(c!='{') return bad(p);tag=ROOT;}
            else {
                if(!f->array && c=='{') {
                    if(f->tag==ROOT && !strcmp(f->key,"play")) tag=PLAY;
                    else if(f->tag==PLAY && !strcmp(f->key,"title")) tag=TITLE;
                    else if(f->tag==PLAY && !strcmp(f->key,"firmware")) tag=FIRMWARE;
                }
                f->stage=AFTER;
            }
            zion_title_frame_t *next=&p->frames[p->depth++];memset(next,0,sizeof(*next));
            next->tag=(uint8_t)tag;next->array=c=='[';next->stage=next->array?VALUE:KEY;
            if(tag==TITLE || tag==FIRMWARE) {
                if(p->capture || (tag==TITLE?p->title_done:p->firmware_done)) return bad(p);
                p->capture=tag;p->capture_depth=p->depth;if(!capture(p,c)) return false;
            }
        } else if(c=='}' || c==']') {
            if(!f || f->array!=(c==']') || (f->stage!=AFTER && f->stage!=(f->array?VALUE:KEY))) return bad(p);
            if(p->capture && p->capture_depth==p->depth) {
                if(p->capture==TITLE) p->title_done=true;else p->firmware_done=true;
                p->capture=0;
            }
            p->depth--;
        } else if(c==':') {
            if(!f || f->array || f->stage!=COLON) return bad(p);
            f->stage=VALUE;
        } else if(c==',') {
            if(!f || f->stage!=AFTER) return bad(p);
            f->stage=f->array?VALUE:KEY;
        } else {
            if(!f || f->stage!=VALUE || !strchr("-0123456789tfn",c)) return bad(p);
            p->primitive=true;
        }
    }
    return true;
}
bool zion_title_result(const zion_title_parser_t *p,uint32_t size,const uint8_t expected[32],char out[33]) {
    if(!zion_title_ready(p)) return false;
    int64_t declared=0;char hex[65],name[256];uint8_t actual[32];
    if(!meta_store_json_get_int(p->firmware,p->firmware_size,"size",&declared) || declared!=size ||
       !meta_store_json_get_string(p->firmware,p->firmware_size,"sha256",hex,sizeof(hex)) ||
       !meta_store_json_parse_sha256(hex,strlen(hex),actual) || memcmp(actual,expected,32)) return false;
    if(!meta_store_json_get_string(p->title,p->title_size,"zh",name,sizeof(name)) || !name[0]) {
        if(!meta_store_json_get_string(p->title,p->title_size,"en",name,sizeof(name)) || !name[0]) return false;
    }
    size_t n=strlen(name);
    if(!meta_name_utf8_ok(name,n)) return false;
    if(n>32) {n=32;while(n && ((unsigned char)name[n]&0xc0)==0x80) n--;}
    if(!n) return false;
    memcpy(out,name,n);out[n]=0;return true;
}
