/**
  ******************************************************************************
  * @file    lwipopts.h
  * @brief   Configuration de lwIP 2.2.1 pour le TP (FreeRTOS + sockets BSD)
  ******************************************************************************
  */
#ifndef LWIPOPTS_H
#define LWIPOPTS_H

/* ---- Systeme : lwIP avec un OS (FreeRTOS) --------------------------------*/
#define NO_SYS                          0
#define SYS_LIGHTWEIGHT_PROT            1
#define LWIP_TCPIP_CORE_LOCKING         1
#define LWIP_TCPIP_CORE_LOCKING_INPUT   0

/* ---- Memoire ---------------------------------------------------------------
   Le sujet recommande MEM_SIZE = 5000 octets au depart. Avec DNS + sockets
   TCP/RAW ouvertes en meme temps, on prend 10 ko (a ajuster). */
#define MEM_ALIGNMENT                   4
#define MEM_SIZE                        (10 * 1024)
#define MEMP_NUM_PBUF                   16
#define MEMP_NUM_RAW_PCB                4
#define MEMP_NUM_UDP_PCB                6
#define MEMP_NUM_TCP_PCB                6
#define MEMP_NUM_TCP_PCB_LISTEN         4
#define MEMP_NUM_TCP_SEG                16
#define MEMP_NUM_NETBUF                 8
#define MEMP_NUM_NETCONN                8
#define MEMP_NUM_SYS_TIMEOUT            (LWIP_NUM_SYS_TIMEOUT_INTERNAL + 4)
#define PBUF_POOL_SIZE                  12

/* Tous les blocs memoire de lwIP alignes sur 32 octets (buffers DMA Ethernet) */
#define LWIP_DECLARE_MEMORY_ALIGNED(variable_name, size) \
        u8_t variable_name[LWIP_MEM_ALIGN_BUFFER(size)] __attribute__((aligned(32)))

/* pbuf "custom" : reception Ethernet sans recopie (zero-copy) */
#define LWIP_SUPPORT_CUSTOM_PBUF        1

/* ---- Protocoles ------------------------------------------------------------*/
#define LWIP_IPV4                       1
#define LWIP_IPV6                       0
#define LWIP_ETHERNET                   1
#define LWIP_ARP                        1
#define LWIP_ICMP                       1    /* repond aux ping */
#define LWIP_RAW                        1    /* socket brute pour envoyer des ping */
#define LWIP_DHCP                       1    /* adresse IP automatique */
#define LWIP_DNS                        1    /* resolution de google.com */
#define DNS_MAX_SERVERS                 2
#define LWIP_UDP                        1
#define LWIP_TCP                        1
#define TCP_MSS                         1460
#define TCP_SND_BUF                     (4 * TCP_MSS)
#define TCP_WND                         (4 * TCP_MSS)

/* Sommes de controle IP/TCP/UDP/ICMP calculees par logiciel. La CRC Ethernet
   (FCS) est toujours calculee/inseree par le MAC (CRCPadCtrl dans ethernetif.c) */
#define CHECKSUM_GEN_IP                 1
#define CHECKSUM_GEN_UDP                1
#define CHECKSUM_GEN_TCP                1
#define CHECKSUM_GEN_ICMP               1
#define CHECKSUM_CHECK_IP               1
#define CHECKSUM_CHECK_UDP              1
#define CHECKSUM_CHECK_TCP              1
#define CHECKSUM_CHECK_ICMP             1

/* ---- Interface reseau ------------------------------------------------------*/
#define LWIP_NETIF_HOSTNAME             1
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_LINK_CALLBACK        1
#define LWIP_NETIF_API                  1

/* ---- API sockets BSD -------------------------------------------------------*/
#define LWIP_NETCONN                    1
#define LWIP_SOCKET                     1
#define LWIP_COMPAT_SOCKETS             1    /* socket(), bind(), sendto()... */
#define LWIP_POSIX_SOCKETS_IO_NAMES     0    /* on utilise closesocket()      */
#define LWIP_SO_RCVTIMEO                1
#define LWIP_SO_SNDTIMEO                1
#define LWIP_TIMEVAL_PRIVATE            0    /* struct timeval de newlib      */
#define LWIP_ERRNO_STDINCLUDE           1    /* errno de newlib               */
#define LWIP_DNS_API_DECLARE_STRUCTS    1

/* ---- Taches lwIP (tailles de pile en OCTETS, cf. sys_arch.c) --------------*/
#define TCPIP_THREAD_NAME               "tcpip"
#define TCPIP_THREAD_STACKSIZE          4096
#define TCPIP_THREAD_PRIO               5
#define TCPIP_MBOX_SIZE                 16
#define DEFAULT_THREAD_STACKSIZE        2048
#define DEFAULT_THREAD_PRIO             3
#define DEFAULT_RAW_RECVMBOX_SIZE       8
#define DEFAULT_UDP_RECVMBOX_SIZE       8
#define DEFAULT_TCP_RECVMBOX_SIZE       8
#define DEFAULT_ACCEPTMBOX_SIZE         4

/* ---- Nombres aleatoires (ports, identifiants DNS) --------------------------*/
#define LWIP_RAND()                     lwip_port_rand()

/* ---- Statistiques ----------------------------------------------------------*/
#define LWIP_STATS                      0

/* ---- Logs de diagnostic (demande du sujet, etape 3) -------------------------
   LWIP_DEBUG active les messages lwIP sur la console (printf -> UART).
   Chaque module est desactive par defaut : passer a LWIP_DBG_ON celui a
   observer (attention : DHCP_DEBUG / ICMP_DEBUG sont assez bavards). */
/* USER CODE BEGIN 0 */
#define LWIP_DEBUG
/* USER CODE END 0 */
#define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_ALL
#define LWIP_DBG_TYPES_ON               LWIP_DBG_ON
#define ETHARP_DEBUG                    LWIP_DBG_OFF
#define NETIF_DEBUG                     LWIP_DBG_OFF
#define ICMP_DEBUG                      LWIP_DBG_OFF
#define DHCP_DEBUG                      LWIP_DBG_OFF
#define DNS_DEBUG                       LWIP_DBG_OFF
#define SOCKETS_DEBUG                   LWIP_DBG_OFF
#define TCPIP_DEBUG                     LWIP_DBG_OFF
#define MEM_DEBUG                       LWIP_DBG_OFF
#define MEMP_DEBUG                      LWIP_DBG_OFF

#endif /* LWIPOPTS_H */
