#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// lwipopts.h dla pico_cyw43_arch_lwip_poll (NO_SYS)
// Bazuje na przykładach z Pico SDK

// NO_SYS=1: lwIP działa w trybie poll (bez wątków/RTOS)
#define NO_SYS                      1

// ❌ API netconn i socket wyłączone (wymagają wątków)
#define LWIP_NETCONN                0
#define LWIP_SOCKET                 0

// Memory pools
#define MEM_SIZE                    (8*1024)
#define MEMP_NUM_TCP_PCB            8
#define MEMP_NUM_TCP_PCB_LISTEN     4
#define MEMP_NUM_TCP_SEG            16
#define MEMP_NUM_PBUF               24
#define PBUF_POOL_SIZE              24

// TCP
#define LWIP_TCP                    1
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (4*TCP_MSS)
#define TCP_WND                     (4*TCP_MSS)
#define TCP_LISTEN_BACKLOG          1

// DHCP
#define LWIP_DHCP                   1
#define DHCP_DOES_ARP_CHECK         0

// DNS (opcjonalnie - można wyłączyć jeśli używasz IP)
#define LWIP_DNS                    0

// ICMP (ping)
#define LWIP_ICMP                   1

// Statisticts & Debug (wyłącz dla produkcji)
#define LWIP_STATS                  0
#define LWIP_DEBUG                  0

// Checksum offload (dla CYW43)
#define CHECKSUM_GEN_IP             0
#define CHECKSUM_GEN_UDP            0
#define CHECKSUM_GEN_TCP            0
#define CHECKSUM_GEN_ICMP           0
#define CHECKSUM_CHECK_IP           0
#define CHECKSUM_CHECK_UDP          0
#define CHECKSUM_CHECK_TCP          0
#define CHECKSUM_CHECK_ICMP         0

#endif /* LWIPOPTS_H */