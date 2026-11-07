/**
  ******************************************************************************
  * @file    arch/cc.h
  * @brief   Adaptation de lwIP au compilateur GCC ARM + newlib
  ******************************************************************************
  */
#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>     /* struct timeval (LWIP_TIMEVAL_PRIVATE = 0) */

#define LWIP_NO_UNISTD_H  1

#ifndef BYTE_ORDER
#define BYTE_ORDER  LITTLE_ENDIAN
#endif

/* Messages de diagnostic et assertions -> console UART */
#define LWIP_PLATFORM_DIAG(x)    do { printf x; } while (0)
#define LWIP_PLATFORM_ASSERT(x)  do { printf("lwIP assertion \"%s\" : %s ligne %d\r\n", \
                                     x, __FILE__, __LINE__); while (1) { } } while (0)

unsigned int lwip_port_rand(void);

#endif /* LWIP_ARCH_CC_H */
