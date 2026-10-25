/* AP-only DNS transport. Packet parsing and lifecycle policy are host-tested. */
#include "zion_portal_net.h"
#include "zion_portal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include <errno.h>
#include <stdatomic.h>
#include <string.h>
static atomic_bool running;
static SemaphoreHandle_t stopped;
static int dns_socket=-1;
static uint8_t dns_ip[4];
static void dns_worker(void *arg) {
    (void)arg;
    uint8_t query[ZION_PORTAL_DNS_MAX],reply[ZION_PORTAL_DNS_MAX];
    while(atomic_load(&running)) {
        struct sockaddr_in peer;socklen_t size=sizeof(peer);
        int count=recvfrom(dns_socket,query,sizeof(query),0,(struct sockaddr *)&peer,&size);
        if(count<0) {
            if(errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR) continue;
            ESP_LOGW("zion_portal","DNS receive errno=%d",errno);break;
        }
        size_t length=zion_portal_dns_reply(query,(size_t)count,reply,sizeof(reply),dns_ip);
        if(length) (void)sendto(dns_socket,reply,length,0,(struct sockaddr *)&peer,size);
    }
    close(dns_socket);
    atomic_store(&running,false);xSemaphoreGive(stopped);vTaskDelete(NULL);
}
esp_err_t zion_portal_dns_start(uint32_t address) {
    if(dns_socket>=0) return ESP_ERR_INVALID_STATE;
    if(!stopped) stopped=xSemaphoreCreateBinary();
    if(!stopped) return ESP_ERR_NO_MEM;
    (void)xSemaphoreTake(stopped,0);
    int socket_fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(socket_fd<0) return ESP_ERR_NO_MEM;
    struct timeval timeout={.tv_sec=0,.tv_usec=200000};
    struct sockaddr_in local={.sin_family=AF_INET,.sin_port=htons(53),.sin_addr.s_addr=address};
    if(setsockopt(socket_fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)) ||
       bind(socket_fd,(struct sockaddr *)&local,sizeof(local))) {
        ESP_LOGE("zion_portal","DNS bind/setup errno=%d",errno);close(socket_fd);return ESP_FAIL;
    }
    memcpy(dns_ip,&address,sizeof(dns_ip));dns_socket=socket_fd;atomic_store(&running,true);
    if(xTaskCreate(dns_worker,"zion-dns",3072,NULL,3,NULL)!=pdPASS) {
        atomic_store(&running,false);dns_socket=-1;close(socket_fd);return ESP_ERR_NO_MEM;
    }
    ESP_LOGI("zion_portal","ZION_PORTAL DNS started scope=AP port=53");return ESP_OK;
}
void zion_portal_dns_stop(void) {
    if(dns_socket<0) return;
    atomic_store(&running,false);
    /* The socket timeout bounds shutdown. Never delete a task holding a socket. */
    if(xSemaphoreTake(stopped,pdMS_TO_TICKS(1500))!=pdTRUE)
        ESP_LOGE("zion_portal","DNS shutdown timeout");
    else {dns_socket=-1;ESP_LOGI("zion_portal","ZION_PORTAL DNS stopped");}
}
