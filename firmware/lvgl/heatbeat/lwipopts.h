#ifndef LWIPOPTS_H
#define LWIPOPTS_H

// Bazuj na domyślnych ustawieniach Pico SDK
#include "lwipopts_examples_common.h"

// SYS (wątki/RTOS mode)
#define NO_SYS                      0
#define LWIP_NETCONN                1
#define LWIP_SOCKET                 0

// Thread-safe
#define LWIP_TCPIP_CORE_LOCKING     1
#define LWIP_COMPAT_MUTEX_ALLOWED   1

// Memory
#define MEM_SIZE                    (16*1024)
#define MEMP_NUM_TCP_PCB            8
#define MEMP_NUM_NETCONN            8

// TCP
#define TCP_MSS                     1460
#define TCP_SND_BUF                 (8*TCP_MSS)
#define TCP_WND                     (8*TCP_MSS)

#endif