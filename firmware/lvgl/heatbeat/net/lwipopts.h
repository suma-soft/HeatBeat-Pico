// firmware/lvgl/heatbeat/net/lwipopts.h
#pragma once

/* System setup: bez systemu operacyjnego (arch poll) */
#define NO_SYS                         1
#define SYS_LIGHTWEIGHT_PROT           0

/* Pamięć – uproszczenie: użyj malloc() libc */
#define MEM_LIBC_MALLOC                1
#define MEMP_MEM_MALLOC                1
#define MEM_ALIGNMENT                  4

/* Protokoły i funkcje, które chcemy mieć */
#define LWIP_IPV4                      1
#define LWIP_IPV6                      0
#define LWIP_UDP                       1
#define LWIP_TCP                       1
#define LWIP_ICMP                      1
#define LWIP_DHCP                      1
#define LWIP_DNS                       1

/* API wysokiego poziomu – wyłączone przy NO_SYS=1 */
#define LWIP_NETCONN                   0
#define LWIP_SOCKET                    0

/* Parametry TCP (bez fajerwerków – wystarczą do DHCP/DNS i prostych rzeczy) */
#define TCP_MSS                        1460
#define TCP_WND                        (4 * TCP_MSS)
#define TCP_SND_BUF                    (4 * TCP_MSS)

/* Drobiazgi ułatwiające życie */
#define LWIP_NETIF_HOSTNAME            1
#define LWIP_NETIF_STATUS_CALLBACK     1
#define LWIP_NETIF_LINK_CALLBACK       1

#define LWIP_SOCKET                    1
#define LWIP_NETCONN                   0
#define LWIP_TIMEVAL_PRIVATE           0


/* Jeśli kompilator krzyczy o rand(), odkomentuj:
#include <stdlib.h>
#define LWIP_RAND() ((u32_t)rand())
*/
