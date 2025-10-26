#pragma once

/* === tryb bez RTOS / arch_poll === */
#define NO_SYS                         1
#define SYS_LIGHTWEIGHT_PROT           0

/* Pamięć – prosto, przez malloc() */
#define MEM_LIBC_MALLOC                1
#define MEMP_MEM_MALLOC                1
#define MEM_ALIGNMENT                  4

/* Protokoły/funkcje */
#define LWIP_IPV4                      1
#define LWIP_IPV6                      0
#define LWIP_ICMP                      1
#define LWIP_UDP                       1
#define LWIP_TCP                       1
#define LWIP_DHCP                      1
#define LWIP_DNS                       1

/* API wysokiego poziomu – WYŁĄCZONE przy NO_SYS=1 */
#define LWIP_NETCONN                   0
#define LWIP_SOCKET                    0

/* TCP – skromne bufory wystarczą do DHCP/DNS */
#define TCP_MSS                        1460
#define TCP_WND                        (4 * TCP_MSS)
#define TCP_SND_BUF                    (4 * TCP_MSS)

/* Przydatne callbacki i hostname */
#define LWIP_NETIF_HOSTNAME            1
#define LWIP_NETIF_STATUS_CALLBACK     1
#define LWIP_NETIF_LINK_CALLBACK       1

/* timeval od systemu */
#define LWIP_TIMEVAL_PRIVATE           0

/* (opcjonalnie) losowanie dla lwIP
#include <stdlib.h>
#define LWIP_RAND() ((u32_t)rand())
*/
