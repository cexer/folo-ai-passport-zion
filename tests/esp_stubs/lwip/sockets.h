// tests/esp_stubs/lwip/sockets.h —— host 语法检查桩:把 lwip BSD socket API
// 映射到宿主系统头(macOS/Linux 均提供同一套 POSIX socket 接口)。
// 仅用于 -fsyntax-only;目标机编译走 IDF lwip 组件的同名头。
#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
typedef intptr_t ssize_t;
static inline int host_recvfrom(SOCKET s, void *buf, int len, int flags, struct sockaddr *addr, int *alen)
{ return recvfrom(s, (char *)buf, len, flags, addr, alen); }
static inline int host_sendto(SOCKET s, const void *buf, int len, int flags, const struct sockaddr *addr, int alen)
{ return sendto(s, (const char *)buf, len, flags, addr, alen); }
#define recvfrom host_recvfrom
#define sendto host_sendto
#define close closesocket
#define SHUT_RDWR SD_BOTH
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
