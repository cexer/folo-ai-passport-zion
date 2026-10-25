#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ZION_PORTAL_URL "http://192.168.4.1/"
/* Public entry used by the official website's mini-program-DP_ytNE7.js.
 * No device identity or credential is included. */
#define ZION_MINI_PROGRAM_URL "weixin://dl/business/?appid=wxd2a90fe8e2accc23&path=pages/index/index&env_version=release"
#define ZION_PORTAL_DNS_MAX 512u
#define ZION_PORTAL_RESULT_GRACE_MS 90000u
#define ZION_PORTAL_EXIT_GRACE_MS 10000u
typedef enum { ZION_PORTAL_IDLE, ZION_PORTAL_CONNECTING, ZION_PORTAL_READY, ZION_PORTAL_FAILED } zion_portal_phase_t;
typedef struct { zion_portal_phase_t phase; int error; uint64_t close_at; } zion_portal_t;
void zion_portal_begin(zion_portal_t *p);
void zion_portal_result(zion_portal_t *p,int error,uint64_t now);
void zion_portal_finish(zion_portal_t *p,uint64_t now);
bool zion_portal_expired(const zion_portal_t *p,uint64_t now);
/* One bounded standard DNS question. A/IN -> AP IPv4; AAAA/other -> NODATA.
 * No upstream forwarding, compression traversal, or persistence beyond TTL 0. */
size_t zion_portal_dns_reply(const uint8_t *query,size_t size,uint8_t *reply,size_t capacity,const uint8_t ip[4]);
