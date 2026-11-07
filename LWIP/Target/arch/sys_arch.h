/**
  ******************************************************************************
  * @file    arch/sys_arch.h
  * @brief   Couche d'abstraction OS de lwIP pour FreeRTOS (types)
  ******************************************************************************
  */
#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

typedef SemaphoreHandle_t sys_sem_t;
typedef SemaphoreHandle_t sys_mutex_t;
typedef QueueHandle_t     sys_mbox_t;
typedef TaskHandle_t      sys_thread_t;
typedef uint32_t          sys_prot_t;

#define SYS_MBOX_NULL  ((QueueHandle_t)NULL)
#define SYS_SEM_NULL   ((SemaphoreHandle_t)NULL)

#define sys_sem_valid(sem)           (((sem) != NULL) && (*(sem) != NULL))
#define sys_sem_set_invalid(sem)     do { if ((sem) != NULL) { *(sem) = NULL; } } while (0)
#define sys_mutex_valid(mutex)       (((mutex) != NULL) && (*(mutex) != NULL))
#define sys_mutex_set_invalid(mutex) do { if ((mutex) != NULL) { *(mutex) = NULL; } } while (0)
#define sys_mbox_valid(mbox)         (((mbox) != NULL) && (*(mbox) != NULL))
#define sys_mbox_set_invalid(mbox)   do { if ((mbox) != NULL) { *(mbox) = NULL; } } while (0)

#endif /* LWIP_ARCH_SYS_ARCH_H */
