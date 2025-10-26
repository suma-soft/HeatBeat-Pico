#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// ============================================================================
// lwipopts.h dla pico_cyw43_arch_lwip_threadsafe_background
// Oparte na pico-sdk/lib/lwip/src/apps/http/lwipopts.h
// ============================================================================

// Tryb z wątkami (SYS, nie NO_SYS)
#define NO_SYS                          0

// Pamięć
#define MEM_LIBC_MALLOC                 1
#define MEMP_MEM_MALLOC                 1
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        4000
#define MEMP_NUM_TCP_SEG                32

// Protokoły
#define LWIP_IPV4                       1
#define LWIP_IPV6                       0
#define LWIP_ICMP                       1
#define LWIP_RAW                        1
#define LWIP_UDP                        1
#define LWIP_TCP                        1
#define LWIP_DHCP                       1
#define LWIP_DNS                        0  // Wyłączone - używamy IP literalnych

// API wysokiego poziomu (netconn, socket)
#define LWIP_NETCONN                    1  // ✅ KLUCZOWE!
#define LWIP_SOCKET                     0  // Nie używamy BSD sockets

// TCP opcje
#define TCP_MSS                         1460
#define TCP_WND                         (8 * TCP_MSS)
#define TCP_SND_BUF                     (8 * TCP_MSS)
#define TCP_SND_QUEUELEN                ((4 * (TCP_SND_BUF) + (TCP_MSS - 1)) / (TCP_MSS))
#define LWIP_TCP_KEEPALIVE              1

// PBUF
#define PBUF_POOL_SIZE                  24
#define LWIP_WND_SCALE                  1
#define TCP_RCV_SCALE                   0x7

// Wątki/mailboxy dla netconn (threadsafe_background)
#define TCPIP_THREAD_STACKSIZE          1024
#define DEFAULT_THREAD_STACKSIZE        1024
#define DEFAULT_RAW_RECVMBOX_SIZE       8
#define TCPIP_MBOX_SIZE                 8
#define LWIP_TIMEVAL_PRIVATE            0
#define LWIP_NETCONN_SEM_PER_THREAD     1

// Netconn/socket limity
#define MEMP_NUM_NETCONN                8
#define DEFAULT_TCP_RECVMBOX_SIZE       8
#define DEFAULT_UDP_RECVMBOX_SIZE       8
#define DEFAULT_ACCEPTMBOX_SIZE         8

// Callbacki i hostname
#define LWIP_NETIF_HOSTNAME             1
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_LINK_CALLBACK        1

// Statystyki i debugging (wyłączone dla produkcji)
#define LWIP_STATS                      0
#define LWIP_STATS_DISPLAY              0

// Randomizacja (wymagane dla niektórych funkcji)
#define LWIP_RAND()                     ((u32_t)rand())

#endif /* LWIPOPTS_H */