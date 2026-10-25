#include "zion592_image.h"
#include <string.h>
static uint32_t le32(const uint8_t *b) {return (uint32_t)b[0]|((uint32_t)b[1]<<8)|((uint32_t)b[2]<<16)|((uint32_t)b[3]<<24);}
static bool fail(z592_image_t *p,const char *message) {p->error=message;return false;}
static void copy_span(uint8_t *out,uint32_t wanted,uint32_t length,const uint8_t *b,uint32_t offset,uint32_t n) {
    uint32_t lo=offset>wanted?offset:wanted,hi=offset+n<wanted+length?offset+n:wanted+length;
    if(lo<hi) memcpy(out+lo-wanted,b+lo-offset,hi-lo);
}
void z592_image_init(z592_image_t *p,uint32_t bytes,bool merged) {
    memset(p,0,sizeof(*p));p->file_size=bytes;p->merged=merged;p->selected=-1;
    if(!bytes || bytes>0x800000) p->error="source-size";
    if(!merged) {p->table_ready=true;p->app_count=1;p->apps[0].size=bytes;}
}
static bool read_table(z592_image_t *p) {
    meta_pt_t table;
    if(!meta_pt_decode(p->table,&table)) return fail(p,"source-partition-md5");
    for(unsigned i=0;i<table.count;i++) {
        const meta_pt_entry_t *e=&table.e[i];
        if(!e->size || e->offset<0x9000 || e->offset>=0x800000 || e->size>0x800000-e->offset ||
           e->offset%4096 || e->size%4096 || e->type>1) return fail(p,"source-partition-range");
        for(unsigned j=0;j<i;j++) {
            const meta_pt_entry_t *q=&table.e[j];
            if(!strcmp(e->label,q->label) || (e->offset<q->offset+q->size && q->offset<e->offset+e->size))
                return fail(p,"source-partition-overlap");
        }
        if(e->type==0) {
            if(e->subtype!=0 && (e->subtype<0x10 || e->subtype>0x1f)) continue;
            if(e->offset%0x10000 || e->offset<0x10000 || p->app_count==Z592_SOURCE_APPS)
                return fail(p,"source-app-layout");
            z592_source_app_t *a=&p->apps[p->app_count++];a->source=e->offset;a->size=e->size;a->subtype=e->subtype;
        } else if(e->subtype>2 && strcmp(e->label,"cardid") && strcmp(e->label,"nvs")) {
            if(meta_carve_data_label_reserved(e->label) ||
               (e->subtype!=0x81 && e->subtype!=0x82 && e->subtype!=0x83 && strcmp(e->label,"e2edata")) ||
               p->data_count==META_DATA_MAX) return fail(p,"unsupported-child-data");
            z592_source_data_t *d=&p->data[p->data_count++];d->source=e->offset;d->size=e->size;d->subtype=e->subtype;
            memcpy(d->label,e->label,sizeof(d->label));
        }
    }
    p->table_ready=true;return true;
}
static void feed_app(z592_source_app_t *a,const uint8_t *b,uint32_t off,uint32_t n,uint32_t file_size) {
    if(a->source>=file_size || off+n<=a->source || off>=a->source+a->size) return;
    copy_span(a->prefix,a->source,sizeof(a->prefix),b,off,n);
    if(!a->header_ready && off+n>=a->source+24) {
        a->header_ready=true;
        if(a->prefix[0]!=0xe9) {a->bad=true;return;}
        a->present=true;a->count=a->prefix[1];
        if(!a->count || a->count>16 || a->prefix[12]!=5 || a->prefix[13]!=0) {a->bad=true;return;}
        a->cursor=24;
    }
    if(!a->header_ready || a->bad) return;
    while(!a->done) {
        const uint32_t target=a->source+a->cursor+a->segment_used;
        if(target>=off+n) break;
        if(target<off) {a->bad=true;return;}
        unsigned take=8-a->segment_used;
        if(take>off+n-target) take=off+n-target;
        memcpy(a->segment+a->segment_used,b+target-off,take);a->segment_used+=(uint8_t)take;
        if(a->segment_used!=8) break;
        uint32_t len=le32(a->segment+4);
        if(a->cursor>a->size-8 || len>a->size-a->cursor-8) {a->bad=true;return;}
        a->cursor+=8+len;a->segment_used=0;a->seen++;
        if(a->seen==a->count) {
            uint32_t end=((a->cursor+16)&~15u)+((a->prefix[23]&1)?32:0);
            if(end>a->size || a->source>file_size || end>file_size-a->source) {a->bad=true;return;}
            a->image_len=end;a->done=true;
        }
    }
    if(a->done) {
        uint32_t tail=a->source+((a->image_len+4095)&~4095u);
        if(tail<=a->source+a->size-4) copy_span(a->tail_magic,tail,4,b,off,n);
    }
}
bool z592_image_feed(z592_image_t *p,const uint8_t *b,size_t size) {
    if(p->error || size>p->file_size-p->received) return fail(p,"source-stream-length");
    uint32_t n=(uint32_t)size,off=p->received;
    if(p->merged && !p->table_ready) {
        copy_span(p->table,0x8000,META_PT_SIZE,b,off,n);
        if(off+n>=0x8000+META_PT_SIZE && !read_table(p)) return false;
    }
    if(p->table_ready) {
        for(unsigned i=0;i<p->app_count;i++) feed_app(&p->apps[i],b,off,n,p->file_size);
        for(unsigned i=0;i<p->data_count;i++) {
            z592_source_data_t *d=&p->data[i];
            uint32_t lo=off>d->source?off:d->source,hi=off+n<d->source+d->size?off+n:d->source+d->size;
            for(uint32_t at=lo;at<hi;at++) if(b[at-off]!=0xff) d->initial=at-d->source+1;
        }
    }
    p->received+=n;return true;
}
bool z592_image_finish(z592_image_t *p) {
    if(p->error || p->received!=p->file_size || !p->table_ready) return fail(p,"source-truncated");
    int factory=-1,ota=-1;unsigned ota_count=0;
    for(unsigned i=0;i<p->app_count;i++) {
        z592_source_app_t *a=&p->apps[i];
        if(a->subtype==0) {
            if(factory>=0 || !a->done || a->bad) return fail(p,"factory-image-invalid");
            factory=(int)i;
        } else if(a->present) {
            if(a->bad || !a->done) return fail(p,"ota-image-invalid");
            ota=(int)i;ota_count++;
        }
    }
    if(factory<0 && ota_count!=1) return fail(p,"ota-image-ambiguous");
    p->selected=factory>=0?factory:ota;
    const z592_source_app_t *a=&p->apps[p->selected];
    uint32_t tail=(a->image_len+4095)&~4095u;
    if(!memcmp(a->tail_magic,"MSIG",4) && (tail+4096>a->size || a->source+tail+4096>p->file_size))
        return fail(p,"signature-tail-truncated");
    return true;
}
static bool allocate_size(const z592_image_t *p,uint32_t size,const meta_carve_t *cur,int replace,uint32_t id,meta_carve_t *next,int *slot) {
    if(!id || !meta_carve_valid(cur)) return false;
    *next=*cur;
    if(replace>=0) {
        if(replace>=cur->count) return false;
        uint32_t old=cur->slot[replace].play_id;
        if(!meta_carve_remove(next,(uint8_t)replace)) return false;
        for(unsigned i=0;i<next->data_count;) {
            if(old && next->data[i].play_id==old) meta_carve_remove_data(next,(uint8_t)i);else i++;
        }
    }
    for(unsigned i=0;i<next->data_count;i++) if(next->data[i].play_id==id) return false;
    int index=meta_carve_place(next,size,META_CARVE_KIND_APP,next);
    if(index<0) return false;
    next->slot[index].play_id=id;
    for(unsigned i=0;i<p->data_count;i++) {
        uint32_t at;
        if(!meta_carve_place_data(next,p->data[i].size,&at)) return false;
        meta_carve_data_t data={.play_id=id,.offset=at,.size=p->data[i].size,.state=META_DATA_PRISTINE,
            .type=1,.subtype=p->data[i].subtype};
        memcpy(data.label,p->data[i].label,sizeof(data.label));
        if(!meta_carve_data_append(next,&data)) return false;
    }
    *slot=index;return meta_carve_valid(next);
}
bool z592_image_allocate(const z592_image_t *p,const meta_carve_t *cur,int replace,uint32_t id,meta_carve_t *next,int *slot) {
    return p->selected>=0 && allocate_size(p,meta_carve_need(p->apps[p->selected].image_len),cur,replace,id,next,slot);
}
int z592_image_stream_app(const z592_image_t *p) {
    int factory=-1,ota=-1;
    for(unsigned i=0;i<p->app_count;i++) {
        const z592_source_app_t *a=&p->apps[i];
        if(a->subtype==0) {
            if(factory>=0 || (a->header_ready && a->bad)) return -2;
            factory=(int)i;
        }
    }
    if(factory>=0) return factory;
    for(unsigned i=0;i<p->app_count;i++) {
        const z592_source_app_t *a=&p->apps[i];
        if(a->present) {
            if(a->bad || ota>=0) return -2;
            ota=(int)i;
        }
    }
    return ota;
}
uint32_t z592_image_minimum_image(const z592_source_app_t *a) {
    if(!a->header_ready || a->bad) return 0;
    if(a->done) return a->image_len;
    uint32_t cursor=a->cursor+(a->count>=a->seen?(uint32_t)(a->count-a->seen)*8:0);
    return ((cursor+16)&~15u)+((a->prefix[23]&1)?32:0);
}
bool z592_image_stream_plan(const z592_image_t *p,const meta_carve_t *cur,int replace,uint32_t id,meta_carve_t *next,int *slot) {
    if(!p->table_ready || !p->app_count || p->error) return false;
    int candidate=z592_image_stream_app(p);
    if(candidate==-2) return false;
    uint32_t bound=0;
    for(unsigned i=0;i<p->app_count;i++) {
        if(candidate>=0 && (int)i!=candidate) continue;
        const z592_source_app_t *a=&p->apps[i];
        uint32_t present=a->source<p->file_size?p->file_size-a->source:0;
        if(present>a->size) present=a->size;
        if(a->done) present=a->image_len;
        if(present>bound) bound=present;
    }
    if(!bound) return false;
    uint32_t minimum=META_CARVE_MIN_SLOT;
    if(candidate>=0) minimum=meta_carve_need(z592_image_minimum_image(&p->apps[candidate]));
    uint32_t need=meta_carve_need(bound);
    const uint32_t pool_capacity=(META_POOL0_END-META_POOL0_START)+(META_POOL1_END-META_POOL1_START);
    if(need>pool_capacity) need=pool_capacity;
    need=need/META_CARVE_SIZE_GRANULE*META_CARVE_SIZE_GRANULE;
    /* First-fit placement is not monotonic across the split pools, so a binary
     * search would skip feasible layouts. At most 2048 pure, bounded attempts. */
    for(uint32_t size=need;size>=minimum;size-=META_CARVE_SIZE_GRANULE) {
        if(allocate_size(p,size,cur,replace,id,next,slot)) return true;
    }
    return false;
}
