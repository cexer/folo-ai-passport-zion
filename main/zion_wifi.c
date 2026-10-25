#include "zion_wifi.h"
#include "zion_form.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "zion_wifi_page.h"
#include "zion_wifi_result.h"
#include "zion_portal_net.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static httpd_handle_t server;
static esp_netif_t *ap;
static char csrf[17];
static char pending_ssid[33],pending_password[65];
static bool pending;
static portMUX_TYPE scan_lock=portMUX_INITIALIZER_UNLOCKED;
static bool scanning;
static esp_err_t scan_error;
static unsigned scan_count;
static wifi_ap_record_t scan_records[ZION_WIFI_SCAN_MAX];
static zion_portal_t portal;
extern const uint8_t mini_png_start[] asm("_binary_zion_mini_png_start");
extern const uint8_t mini_png_end[] asm("_binary_zion_mini_png_end");
static uint64_t now_ms(void) {return (uint64_t)esp_timer_get_time()/1000;}
static void page_headers(httpd_req_t *req) {
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"Content-Security-Policy","default-src 'none'; style-src 'unsafe-inline'; script-src 'self'; img-src 'self'; connect-src 'self'; form-action 'self'; frame-ancestors 'none'");
}
static esp_err_t get(httpd_req_t *req) {
    page_headers(req);
    return httpd_resp_sendstr(req,wifi_page);
}
static esp_err_t script(httpd_req_t *req) {
    httpd_resp_set_type(req,"application/javascript; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_sendstr(req,wifi_script);
}
static esp_err_t result_script(httpd_req_t *req) {
    httpd_resp_set_type(req,"application/javascript; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_sendstr(req,wifi_result_script);
}
static esp_err_t mini_code(httpd_req_t *req) {
    httpd_resp_set_type(req,"image/png");
    return httpd_resp_send(req,(const char *)mini_png_start,mini_png_end-mini_png_start);
}
static esp_err_t redirect(httpd_req_t *req,httpd_err_code_t error) {
    (void)error;
    httpd_resp_set_status(req,"303 See Other");
    httpd_resp_set_hdr(req,"Location",ZION_PORTAL_URL);
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"Connection","close");
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    /* A nonempty body is required by iOS portal detection. */
    esp_err_t result=httpd_resp_sendstr(req,"<a href='" ZION_PORTAL_URL "'>打开 ZION 配网页</a>");
    (void)httpd_sess_trigger_close(server,httpd_req_to_sockfd(req));
    return result;
}
static esp_err_t status_get(httpd_req_t *req) {
    portENTER_CRITICAL(&scan_lock);zion_portal_t copy=portal;portEXIT_CRITICAL(&scan_lock);
    char json[128];
    snprintf(json,sizeof(json),"{\"state\":%u,\"error\":%d,\"pending\":%s}",(unsigned)copy.phase,copy.error,pending?"true":"false");
    httpd_resp_set_type(req,"application/json");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_sendstr(req,json);
}
void zion_wifi_connect_result(esp_err_t error) {
    portENTER_CRITICAL(&scan_lock);zion_portal_result(&portal,error,now_ms());portEXIT_CRITICAL(&scan_lock);
    ESP_LOGI("zion_wifi","ZION_PORTAL result=%s success_grace_ms=%u",esp_err_to_name(error),error?0:ZION_PORTAL_RESULT_GRACE_MS);
}
bool zion_wifi_ap_expired(void) {
    portENTER_CRITICAL(&scan_lock);bool expired=zion_portal_expired(&portal,now_ms());portEXIT_CRITICAL(&scan_lock);
    return server && expired;
}
static esp_err_t token_get(httpd_req_t *req) {
    httpd_resp_set_type(req,"text/plain");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    return httpd_resp_sendstr(req,csrf);
}
void zion_wifi_scan_complete(const wifi_ap_record_t *records,unsigned count,esp_err_t error) {
    if(count>ZION_WIFI_SCAN_MAX) count=ZION_WIFI_SCAN_MAX;
    portENTER_CRITICAL(&scan_lock);
    scan_count=records?count:0;scan_error=error;
    if(scan_count) memcpy(scan_records,records,scan_count*sizeof(*records));
    scanning=false;
    portEXIT_CRITICAL(&scan_lock);
}
static esp_err_t scan_post(httpd_req_t *req) {
    char token[17];
    if(req->content_len || httpd_req_get_hdr_value_str(req,"X-Zion-Token",token,sizeof(token))!=ESP_OK || strcmp(token,csrf))
        return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"设置已失效，请刷新页面");
    portENTER_CRITICAL(&scan_lock);bool busy=scanning;
    if(!busy) {scanning=true;scan_error=ESP_OK;scan_count=0;}
    portEXIT_CRITICAL(&scan_lock);
    if(!busy && !zion_service_scan()) {
        zion_wifi_scan_complete(NULL,0,ESP_ERR_INVALID_STATE);
        httpd_resp_set_status(req,"409 Conflict");return httpd_resp_sendstr(req,"设备忙，请稍后重新扫描");
    }
    return httpd_resp_sendstr(req,"OK");
}
static esp_err_t scan_get(httpd_req_t *req) {
    /* Copy under the lock, format/send outside it. Hex preserves SSID bytes and
     * makes quotes, HTML and control characters inert in both JSON and JS. */
    wifi_ap_record_t *records=malloc(sizeof(scan_records));
    if(!records) return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"扫描结果内存不足");
    portENTER_CRITICAL(&scan_lock);
    bool busy=scanning;unsigned count=scan_count;esp_err_t error=scan_error;
    memcpy(records,scan_records,count*sizeof(*records));portEXIT_CRITICAL(&scan_lock);
    httpd_resp_set_type(req,"application/json");httpd_resp_set_hdr(req,"Cache-Control","no-store");
    char out[180];snprintf(out,sizeof(out),"{\"pending\":%s,\"error\":%d,\"networks\":[",busy?"true":"false",error);
    esp_err_t result=httpd_resp_sendstr_chunk(req,out);
    for(unsigned i=0;i<count && result==ESP_OK;i++) {
        char hex[65];unsigned n=0;
        while(n<32 && records[i].ssid[n]) {snprintf(hex+n*2,3,"%02x",records[i].ssid[n]);n++;}
        hex[n*2]=0;
        snprintf(out,sizeof(out),"%s{\"hex\":\"%s\",\"rssi\":%d,\"secure\":%s}",i?",":"",hex,records[i].rssi,records[i].authmode==WIFI_AUTH_OPEN?"false":"true");
        result=httpd_resp_sendstr_chunk(req,out);
    }
    free(records);
    if(result==ESP_OK) result=httpd_resp_sendstr_chunk(req,"]}");
    if(result==ESP_OK) result=httpd_resp_send_chunk(req,NULL,0);
    return result;
}
static esp_err_t post(httpd_req_t *req) {
    char body[512],ssid[33],pass[65],token[17];size_t at=0;
    if(req->content_len<=0 || req->content_len>=(int)sizeof(body)) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"提交内容无效");
    while(at<(size_t)req->content_len) {
        int n=httpd_req_recv(req,body+at,req->content_len-at);
        if(n<=0) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"提交不完整，请重试");
        at+=(size_t)n;
    }
    bool ok=zion_wifi_form(body,at,ssid,pass,token) && !memcmp(token,csrf,16);
    if(ok) {
        memcpy(pending_ssid,ssid,sizeof(pending_ssid));memcpy(pending_password,pass,sizeof(pending_password));pending=true;
        portENTER_CRITICAL(&scan_lock);portal=(zion_portal_t){0};portEXIT_CRITICAL(&scan_lock);
    }
    memset(body,0,sizeof(body));memset(pass,0,sizeof(pass));
    if(!ok) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"设置已失效或设备忙，请刷新页面");
    page_headers(req);return httpd_resp_sendstr(req,wifi_result_page);
}
static bool session_token(httpd_req_t *req) {
    char body[23]={0};size_t at=0;
    if(req->content_len!=22) return false;
    while(at<22) {int n=httpd_req_recv(req,body+at,22-at);if(n<=0) return false;at+=(size_t)n;}
    return !memcmp(body,"token=",6) && !memcmp(body+6,csrf,16);
}
static esp_err_t finish(httpd_req_t *req) {
    if(!session_token(req)) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"配网会话已失效");
    portENTER_CRITICAL(&scan_lock);bool ready=portal.phase==ZION_PORTAL_READY;
    if(ready) zion_portal_finish(&portal,now_ms());
    portEXIT_CRITICAL(&scan_lock);
    if(!ready) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"设备尚未完成配网");
    ESP_LOGI("zion_wifi","ZION_PORTAL handoff exit_grace_ms=%u",ZION_PORTAL_EXIT_GRACE_MS);
    return httpd_resp_sendstr(req,"OK");
}
static esp_err_t apply(httpd_req_t *req) {
    if(!session_token(req) || !pending) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"没有匹配的待连接设置");
    if(!zion_service_wifi(pending_ssid,pending_password)) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"设备忙，请稍后重新提交");
    portENTER_CRITICAL(&scan_lock);zion_portal_begin(&portal);portEXIT_CRITICAL(&scan_lock);
    pending=false;memset(pending_password,0,sizeof(pending_password));
    return httpd_resp_sendstr(req,"正在连接，请查看设备状态");
}
esp_err_t zion_wifi_ap_start(char name[33],char password[9]) {
    if(server) return ESP_OK;
    uint32_t random[3];esp_fill_random(random,sizeof(random));
    snprintf(name,33,"Zion-%04lx",(unsigned long)(random[0]&0xffff));
    password[0]=0;
    snprintf(csrf,sizeof(csrf),"%08lx%08lx",(unsigned long)random[0],(unsigned long)random[2]);
    ap=esp_netif_create_default_wifi_ap();if(!ap) return ESP_ERR_NO_MEM;
    wifi_config_t cfg={0};memcpy(cfg.ap.ssid,name,strlen(name));cfg.ap.ssid_len=strlen(name);
    cfg.ap.max_connection=1;cfg.ap.authmode=WIFI_AUTH_OPEN;
    cfg.ap.channel=1;cfg.ap.beacon_interval=100;
    esp_err_t err=esp_wifi_set_mode(WIFI_MODE_APSTA);
    if(err==ESP_OK) err=esp_wifi_set_config(WIFI_IF_AP,&cfg);
    if(err==ESP_OK) ESP_LOGI("zion_wifi","ZION_AP auth=%u password_bytes=%u",(unsigned)cfg.ap.authmode,(unsigned)strlen((const char *)cfg.ap.password));
    memset(&cfg,0,sizeof(cfg));
    if(err==ESP_OK) {
        esp_netif_ip_info_t ip;err=esp_netif_get_ip_info(ap,&ip);
        if(err==ESP_OK) {
            /* The URI storage remains valid until the DHCP server is destroyed. */
            static const char portal_uri[]=ZION_PORTAL_URL;
            uint8_t offer_dns=1;
            esp_netif_dns_info_t dns={.ip.type=ESP_IPADDR_TYPE_V4,.ip.u_addr.ip4=ip.ip};
            esp_err_t stopped_result=esp_netif_dhcps_stop(ap);
            if(stopped_result!=ESP_OK && stopped_result!=ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED) err=stopped_result;
            if(err==ESP_OK) err=esp_netif_dhcps_option(ap,ESP_NETIF_OP_SET,ESP_NETIF_DOMAIN_NAME_SERVER,&offer_dns,sizeof(offer_dns));
            if(err==ESP_OK) err=esp_netif_set_dns_info(ap,ESP_NETIF_DNS_MAIN,&dns);
            if(err==ESP_OK) err=esp_netif_dhcps_option(ap,ESP_NETIF_OP_SET,ESP_NETIF_CAPTIVEPORTAL_URI,(void *)portal_uri,strlen(portal_uri));
            if(err==ESP_OK) err=esp_netif_dhcps_start(ap);
            if(err==ESP_OK) err=zion_portal_dns_start(ip.ip.addr);
        }
    }
    if(err==ESP_OK) {
        httpd_config_t h=HTTPD_DEFAULT_CONFIG();h.stack_size=4096;h.max_uri_handlers=16;h.max_open_sockets=3;h.lru_purge_enable=true;h.recv_wait_timeout=5;h.send_wait_timeout=5;
        err=httpd_start(&server,&h);
    }
    if(err==ESP_OK) {
        const httpd_uri_t routes[]={
            {.uri="/",.method=HTTP_GET,.handler=get},
            {.uri="/wifi",.method=HTTP_POST,.handler=post},
            {.uri="/apply",.method=HTTP_POST,.handler=apply},
            {.uri="/wifi.js",.method=HTTP_GET,.handler=script},
            {.uri="/token",.method=HTTP_GET,.handler=token_get},
            {.uri="/scan",.method=HTTP_GET,.handler=scan_get},
            {.uri="/scan",.method=HTTP_POST,.handler=scan_post},
            {.uri="/status",.method=HTTP_GET,.handler=status_get},
            {.uri="/finish",.method=HTTP_POST,.handler=finish},
            {.uri="/result.js",.method=HTTP_GET,.handler=result_script},
            {.uri="/mini-code.png",.method=HTTP_GET,.handler=mini_code}};
        for(unsigned i=0;i<sizeof(routes)/sizeof(routes[0]) && err==ESP_OK;i++) err=httpd_register_uri_handler(server,&routes[i]);
        if(err==ESP_OK) err=httpd_register_err_handler(server,HTTPD_404_NOT_FOUND,redirect);
    }
    if(err!=ESP_OK) zion_wifi_ap_stop();
    return err;
}
void zion_wifi_ap_stop(void) {
    zion_portal_dns_stop();
    if(server) {httpd_stop(server);server=NULL;}
    if(ap) {(void)esp_wifi_set_mode(WIFI_MODE_STA);esp_netif_destroy_default_wifi(ap);ap=NULL;}
    memset(csrf,0,sizeof(csrf));
    pending=false;memset(pending_ssid,0,sizeof(pending_ssid));memset(pending_password,0,sizeof(pending_password));
    portENTER_CRITICAL(&scan_lock);portal=(zion_portal_t){0};scanning=false;scan_count=0;scan_error=ESP_OK;portEXIT_CRITICAL(&scan_lock);
}
