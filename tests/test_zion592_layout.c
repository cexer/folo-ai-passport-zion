#include "meta_carve_store.h"
#include <assert.h>
#include <stdio.h>
int main(int argc,char **argv) {
    assert(META_FACTORY_SIZE==0x200000 && META_POOL0_START==0x210000);
    uint8_t table[META_PT_SIZE];meta_pt_safe(table);
    meta_pt_t pt;assert(meta_pt_decode(table,&pt));bool factory=false;
    for(unsigned i=0;i<pt.count;i++) if(pt.e[i].type==0) {
        assert(pt.e[i].offset==0x10000 && pt.e[i].size==0x200000);factory=true;
    }
    assert(factory);
    meta_carve_t empty={0},next;int slot=meta_carve_place(&empty,0x104000,META_CARVE_KIND_APP,&next);
    assert(slot==0 && next.slot[0].offset==0x210000 && meta_carve_valid(&next));
    if(argc>1) {FILE *f=fopen(argv[1],"wb");assert(f);assert(fwrite(table,1,sizeof(table),f)==sizeof(table));fclose(f);}
    puts("Zion592 2MiB layout: PASS (factory, pool boundary, safe table and allocator agree)");
}
