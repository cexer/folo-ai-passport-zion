#include "zion_wifi.h"
#include "zion_form.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_netif.h"
#include "esp_random.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static httpd_handle_t server;
static esp_netif_t *ap;
static char csrf[17];
static char pending_ssid[33],pending_password[65];
static bool pending;
static esp_err_t get(httpd_req_t *req) {
    char html[1100];
    snprintf(html,sizeof(html),"<!doctype html><meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'>"
        "<title>Zion 配网</title><style>body{max-width:26em;margin:2em auto;padding:1em;font:18px sans-serif;background:#101912;color:#d6e2b6}input,button{box-sizing:border-box;width:100%%;padding:12px;margin:8px 0;font:inherit}</style>"
        "<h1>Zion 配网</h1><form method=post action=/wifi><input type=hidden name=token value='%s'>"
        "<label>Wi-Fi 名称<input name=ssid maxlength=32 required></label><label>密码<input name=password type=password maxlength=64></label>"
        "<button>保存并连接</button></form><p>提交后请查看设备上的连接状态，再切回手机原网络。</p>",csrf);
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    httpd_resp_set_hdr(req,"Content-Security-Policy","default-src 'none'; style-src 'unsafe-inline'; form-action 'self'; frame-ancestors 'none'");
    return httpd_resp_sendstr(req,html);
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
    }
    memset(body,0,sizeof(body));memset(pass,0,sizeof(pass));
    if(!ok) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"设置已失效或设备忙，请刷新页面");
    httpd_resp_set_type(req,"text/html; charset=utf-8");
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    char policy[180];char *html=malloc(1100);
    if(!html) {pending=false;memset(pending_password,0,sizeof(pending_password));return httpd_resp_send_err(req,HTTPD_500_INTERNAL_SERVER_ERROR,"内存不足，请稍后重试");}
    snprintf(policy,sizeof(policy),"default-src 'none'; script-src 'nonce-%s'; connect-src 'self'; form-action 'self'; frame-ancestors 'none'",csrf);
    httpd_resp_set_hdr(req,"Content-Security-Policy",policy);
    snprintf(html,1100,"<meta charset=utf-8><meta name=viewport content='width=device-width,initial-scale=1'><h2>已收到网络设置</h2><p id=status>正在连接，请查看设备状态，成功后再切回原网络。</p>"
        "<script nonce='%s'>fetch('/apply',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'token=%s'}).then(r=>{if(!r.ok)document.getElementById('status').textContent='设备忙，请返回重试';}).catch(()=>{});</script>"
        "<noscript><form method=post action=/apply><input type=hidden name=token value='%s'><button>开始连接</button></form></noscript>",csrf,csrf,csrf);
    esp_err_t result=httpd_resp_sendstr(req,html);free(html);return result;
}
static esp_err_t apply(httpd_req_t *req) {
    char body[23]={0};size_t at=0;
    if(req->content_len!=22) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"连接请求无效，请重新配网");
    while(at<22) {int n=httpd_req_recv(req,body+at,22-at);if(n<=0) return ESP_FAIL;at+=(size_t)n;}
    if(!pending || memcmp(body,"token=",6) || memcmp(body+6,csrf,16)) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"没有匹配的待连接设置");
    if(!zion_service_wifi(pending_ssid,pending_password)) return httpd_resp_send_err(req,HTTPD_400_BAD_REQUEST,"设备忙，请稍后重新提交");
    pending=false;memset(pending_password,0,sizeof(pending_password));
    return httpd_resp_sendstr(req,"正在连接，请查看设备状态");
}
esp_err_t zion_wifi_ap_start(char name[33],char password[9]) {
    if(server) return ESP_OK;
    uint32_t random[3];esp_fill_random(random,sizeof(random));
    snprintf(name,33,"Zion-%04lx",(unsigned long)(random[0]&0xffff));
    snprintf(password,9,"%08lu",(unsigned long)(random[1]%100000000));
    snprintf(csrf,sizeof(csrf),"%08lx%08lx",(unsigned long)random[0],(unsigned long)random[2]);
    ap=esp_netif_create_default_wifi_ap();if(!ap) return ESP_ERR_NO_MEM;
    wifi_config_t cfg={0};memcpy(cfg.ap.ssid,name,strlen(name));cfg.ap.ssid_len=strlen(name);
    memcpy(cfg.ap.password,password,8);cfg.ap.max_connection=1;cfg.ap.authmode=WIFI_AUTH_WPA2_PSK;
    cfg.ap.channel=1;cfg.ap.beacon_interval=100;
    esp_err_t err=esp_wifi_set_mode(WIFI_MODE_APSTA);
    if(err==ESP_OK) err=esp_wifi_set_config(WIFI_IF_AP,&cfg);
    memset(&cfg,0,sizeof(cfg));
    if(err==ESP_OK) {
        httpd_config_t h=HTTPD_DEFAULT_CONFIG();h.stack_size=4096;h.max_open_sockets=2;h.lru_purge_enable=true;h.recv_wait_timeout=5;h.send_wait_timeout=5;
        err=httpd_start(&server,&h);
    }
    if(err==ESP_OK) {
        httpd_uri_t a={.uri="/",.method=HTTP_GET,.handler=get},b={.uri="/wifi",.method=HTTP_POST,.handler=post},c={.uri="/apply",.method=HTTP_POST,.handler=apply};
        err=httpd_register_uri_handler(server,&a);if(err==ESP_OK) err=httpd_register_uri_handler(server,&b);
        if(err==ESP_OK) err=httpd_register_uri_handler(server,&c);
    }
    if(err!=ESP_OK) zion_wifi_ap_stop();
    return err;
}
void zion_wifi_ap_stop(void) {
    if(server) {httpd_stop(server);server=NULL;}
    if(ap) {(void)esp_wifi_set_mode(WIFI_MODE_STA);esp_netif_destroy_default_wifi(ap);ap=NULL;}
    memset(csrf,0,sizeof(csrf));
    pending=false;memset(pending_ssid,0,sizeof(pending_ssid));memset(pending_password,0,sizeof(pending_password));
}
