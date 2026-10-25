#include "zion592_image.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void u32(uint8_t *p,uint32_t x) {for(unsigned i=0;i<4;i++) p[i]=(uint8_t)(x>>(i*8));}
static void app(uint8_t *b) {
    memset(b,0,336);b[0]=0xe9;b[1]=1;b[12]=5;b[23]=1;u32(b+28,256);
    memcpy(b+80,"unit-app",8);
}
static void feed(z592_image_t *p,const uint8_t *b,uint32_t size,unsigned chunk) {
    for(uint32_t at=0;at<size;) {unsigned n=size-at>chunk?chunk:size-at;assert(z592_image_feed(p,b+at,n));at+=n;}
}
static void fixture(uint8_t *b,bool ota) {
    memset(b,0xff,0x60000);
    meta_pt_t t={.count=4};
    t.e[0]=(meta_pt_entry_t){.type=1,.subtype=2,.offset=0x9000,.size=0x6000,.label="nvs"};
    t.e[1]=(meta_pt_entry_t){.type=0,.subtype=ota?0x10:0,.offset=0x10000,.size=0x20000,.label="app"};
    t.e[2]=(meta_pt_entry_t){.type=1,.subtype=0x82,.offset=0x30000,.size=0x1000,.label="storage"};
    t.e[3]=(meta_pt_entry_t){.type=0,.subtype=0x11,.offset=0x40000,.size=0x20000,.label="spare"};
    meta_pt_encode(&t,b+0x8000);app(b+0x10000);b[0x30010]=7;
}
int main(int argc,char **argv) {
    uint8_t *b=malloc(0x60000);z592_image_t *p=malloc(sizeof(*p));assert(b&&p);
    for(unsigned chunk=1;chunk<=4096;chunk=chunk==1?7:chunk==7?1023:chunk==1023?4096:4097) {
        fixture(b,false);z592_image_init(p,0x60000,true);feed(p,b,0x60000,chunk);assert(z592_image_finish(p));
        assert(p->selected==0 && p->apps[0].image_len==336 && p->data_count==1 && p->data[0].initial==17);
        meta_carve_t empty={0},next;int slot;
        assert(z592_image_allocate(p,&empty,-1,590,&next,&slot));
        assert(next.count==1 && next.data_count==1 && next.slot[slot].image_len==0 && next.slot[slot].state==META_SLOT_EMPTY);
        assert(next.data[0].play_id==590 && !strcmp(next.data[0].label,"storage") && meta_carve_valid(&next));
    }
    fixture(b,true);z592_image_init(p,0x60000,true);feed(p,b,0x60000,91);assert(z592_image_finish(p));
    app(b+0x40000);z592_image_init(p,0x60000,true);feed(p,b,0x60000,233);assert(!z592_image_finish(p));
    fixture(b,false);b[0x1000c]=0;z592_image_init(p,0x60000,true);feed(p,b,0x60000,1024);assert(!z592_image_finish(p));
    app(b);z592_image_init(p,336,false);feed(p,b,336,1);assert(z592_image_finish(p) && p->apps[0].image_len==336);
    z592_image_init(p,335,false);feed(p,b,335,1);assert(!z592_image_finish(p));
    /* 725-like sparse source: reserve only a feasible APP capacity alongside
     * DATA, then reclaim the difference once its actual length is known. */
    memset(p,0,sizeof(*p));p->selected=-1;p->table_ready=true;p->file_size=7616468;p->app_count=1;
    p->apps[0]=(z592_source_app_t){.source=0x20000,.size=0x300000,.subtype=0x10,.present=true,.header_ready=true};
    p->data_count=1;p->data[0]=(z592_source_data_t){.source=0x600000,.size=0x200000,.subtype=0x82,.label="assets"};
    meta_carve_t base={0},reserved;int existing=meta_carve_place(&base,1064960,META_CARVE_KIND_APP,&base),planned=-1;
    assert(existing>=0 && z592_image_stream_plan(p,&base,-1,725,&reserved,&planned));
    assert(reserved.slot[planned].size>=meta_carve_need(2395392) && reserved.data_count==1 && meta_carve_valid(&reserved));
    uint32_t before=meta_carve_free(&reserved),capacity=reserved.slot[planned].size;
    reserved.slot[planned].size=meta_carve_need(2395392);
    assert(meta_carve_valid(&reserved) && meta_carve_free(&reserved)==before+capacity-meta_carve_need(2395392));
    puts("PASS sparse stream reservation: oversized source partition, 2 MiB resource, existing APP, exact final shrink");
    if(argc>1) {
        FILE *f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);long n=ftell(f);rewind(f);
        uint8_t *real=malloc((size_t)n);assert(real && fread(real,1,(size_t)n,f)==(size_t)n);fclose(f);
        z592_image_init(p,(uint32_t)n,true);feed(p,real,(uint32_t)n,733);assert(z592_image_finish(p));
        assert(p->apps[p->selected].image_len==2418144 && p->data_count==0);
        puts("Published play 590 streamed layout: PASS (2418144 app bytes, no child DATA)");free(real);
    }
    free(p);free(b);puts("Zion592 image: PASS (raw/factory/OTA, fragmentation, truncation, ambiguity, allocation)");
}
