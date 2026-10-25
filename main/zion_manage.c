#include "zion_manage.h"
#include "zion_prefs.h"
#include "meta_carve_flash.h"
#include "esp_flash.h"
#include <string.h>
int zion_default_find(const meta_carve_t *c,const zion_prefs_t *p) {
    if(!(p->flags&ZION_PREFS_ENABLED)) return -1;
    for(unsigned i=0;i<c->count;i++) {
        const meta_carve_slot_t *s=&c->slot[i];
        if(s->kind==META_CARVE_KIND_APP && s->state==META_SLOT_VALID && s->play_id==p->play_id && s->offset==p->offset &&
           !memcmp(s->image_sha256,p->sha256,32)) return (int)i;
    }
    return -1;
}
esp_err_t zion_default_update(zion_prefs_t *p,int slot) {
    zion_prefs_t next=*p;
    const meta_carve_t *c=meta_carve_flash_carve();
    if(slot<0) next.flags=0;
    else {
        if(slot>=c->count || c->slot[slot].kind!=META_CARVE_KIND_APP || c->slot[slot].state!=META_SLOT_VALID) return ESP_ERR_INVALID_ARG;
        next.flags=ZION_PREFS_ENABLED;next.play_id=c->slot[slot].play_id;next.offset=c->slot[slot].offset;
        memcpy(next.sha256,c->slot[slot].image_sha256,32);
    }
    esp_err_t err=zion_prefs_save(&next);if(err==ESP_OK) *p=next;return err;
}
esp_err_t zion_delete(zion_prefs_t *p,int slot,bool *cleanup_pending) {
    *cleanup_pending=false;
    const meta_carve_t *c=meta_carve_flash_carve();
    if(slot<0 || slot>=c->count) return ESP_ERR_INVALID_ARG;
    const meta_carve_slot_t app=c->slot[slot];
    meta_carve_data_t owned[META_DATA_MAX];unsigned count=0;
    for(unsigned i=0;i<c->data_count;i++) if(app.play_id && c->data[i].play_id==app.play_id) owned[count++]=c->data[i];
    if((p->flags&ZION_PREFS_ENABLED) && p->play_id==app.play_id && p->offset==app.offset) {
        esp_err_t err=zion_default_update(p,-1);if(err!=ESP_OK) return err;
    }
    esp_err_t err=meta_carve_flash_remove_app_and_data(slot);
    if(err!=ESP_OK) return err;
    /* Once removal is durable it must never be rolled back to erased bytes. */
    err=esp_flash_erase_region(NULL,app.offset,app.size);
    for(unsigned i=0;i<count;i++) {
        esp_err_t e=esp_flash_erase_region(NULL,owned[i].offset,owned[i].size);
        if(err==ESP_OK) err=e;
    }
    *cleanup_pending=err!=ESP_OK;return err;
}
