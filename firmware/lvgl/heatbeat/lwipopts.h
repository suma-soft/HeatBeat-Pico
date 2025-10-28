#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// Wymuś NO_SYS dla lwIP (brak wątków)
#define NO_SYS 1

// Wyłącz API netconn (wymaga SYS)
#define LWIP_NETCONN 0

// Wyłącz API socket (wymaga SYS)
#define LWIP_SOCKET 0

// Podstawowa konfiguracja lwIP dla NO_SYS
#define LWIP_TIMERS 1
#define LWIP_ARP 1
#define LWIP_ETHERNET 1
#define LWIP_ICMP 1
#define LWIP_RAW 1
#define LWIP_UDP 1
#define LWIP_TCP 1
#define LWIP_DNS 1
#define LWIP_DHCP 1

// Bufory i pamięć
#define MEM_SIZE (8*1024)
#define PBUF_POOL_SIZE 24
#define MEMP_NUM_TCP_PCB 8
#define MEMP_NUM_TCP_PCB_LISTEN 4
#define MEMP_NUM_UDP_PCB 8
#define MEMP_NUM_NETBUF 0
#define MEMP_NUM_NETCONN 0
#define MEMP_NUM_TCPIP_MSG_API 0

// TCP options
#define TCP_MSS 536
#define TCP_SND_BUF (2 * TCP_MSS)
#define TCP_WND (2 * TCP_MSS)

// Debugging (wyłącz dla release)
#define LWIP_DEBUG 0

#endif /* LWIPOPTS_H */