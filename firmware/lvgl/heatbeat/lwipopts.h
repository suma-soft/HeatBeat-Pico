#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// NO_SYS (poll mode)
#define NO_SYS                      1
#define LWIP_NETCONN                0
#define LWIP_SOCKET                 0

// Memory
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

#endif