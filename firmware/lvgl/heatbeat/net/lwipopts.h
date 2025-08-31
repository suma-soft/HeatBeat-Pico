#pragma once

/* Bez RTOS (cyw43_arch_poll) */
#define NO_SYS                         1
#define SYS_LIGHTWEIGHT_PROT           0

/* Pamięć */
#define MEM_LIBC_MALLOC                1
#define MEMP_MEM_MALLOC                1
#define MEM_ALIGNMENT                  4

/* Protokoły */
#define LWIP_IPV4                      1
#define LWIP_IPV6                      0
#define LWIP_TCP                       1
#define LWIP_UDP                       1
#define LWIP_ICMP                      1
#define LWIP_DHCP                      1
#define LWIP_DNS                       1

/* Wysokopoziomowe API – MUSI być OFF przy NO_SYS=1 */
#define LWIP_NETCONN                   0
#define LWIP_SOCKET                    0

/* Dodatki */
#define LWIP_TIMEVAL_PRIVATE           0
#define LWIP_NETIF_HOSTNAME            1
#define LWIP_NETIF_STATUS_CALLBACK     1
#define LWIP_NETIF_LINK_CALLBACK       1

/* TCP spokojne ustawienia */
#define TCP_MSS                        1460
#define TCP_WND                        (4 * TCP_MSS)
#define TCP_SND_BUF                    (4 * TCP_MSS)

/* // Jeśli kompilator krzyczy o rand():
// #include <stdlib.h>
// #define LWIP_RAND() ((u32_t)rand())
*/
