#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// NO_SYS=0: lwIP działa w trybie SYS (z wątkami) - wymagane przez threadsafe_background
#define NO_SYS                      0

// API netconn i socket
#define LWIP_NETCONN                1
#define LWIP_SOCKET                 0

// Thread-safe (tcpip_core_locking)
#define LWIP_TCPIP_CORE_LOCKING     1
#define LWIP_COMPAT_MUTEX_ALLOWED   1

// Memory pools
#define MEM_SIZE                    (16*1024)
#define MEMP_NUM_TCP_PCB            8
#define MEMP_NUM_NETCONN            8

// TCP
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (8*TCP_MSS)
#define TCP_WND                     (8*TCP_MSS)

// DHCP
#define LWIP_DHCP                   1
#define DHCP_DOES_ARP_CHECK         0

// DNS
#define LWIP_DNS                    0

// ICMP
#define LWIP_ICMP                   1

// Stats & Debug
#define LWIP_STATS                  0
#define LWIP_DEBUG                  0

// Checksum offload
#define CHECKSUM_GEN_IP             0
#define CHECKSUM_GEN_UDP            0
#define CHECKSUM_GEN_TCP            0
#define CHECKSUM_GEN_ICMP           0
#define CHECKSUM_CHECK_IP           0
#define CHECKSUM_CHECK_UDP          0
#define CHECKSUM_CHECK_TCP          0
#define CHECKSUM_CHECK_ICMP         0

#endif /* LWIPOPTS_H */