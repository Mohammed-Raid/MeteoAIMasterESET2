/**
  ******************************************************************************
  * @file    FreeRTOSConfig.h
  * @brief   Configuration de FreeRTOS V11.1.0 pour STM32N657 (Cortex-M55)
  *
  *  Portage utilise : GCC/ARM_CM55_NTZ (sans TrustZone). Le code FSBL tourne en
  *  etat SECURISE uniquement -> configRUN_FREERTOS_SECURE_ONLY = 1.
  *
  *  API : API native FreeRTOS (xTaskCreate, vTaskDelay, ...). Le paquet ST
  *  STM32CubeN6 ne fournit pas de couche CMSIS-RTOS v1/v2 pour FreeRTOS ;
  *  l'API native evite une couche supplementaire (et sa consommation de pile).
  ******************************************************************************
  */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#if defined(__ICCARM__) || defined(__CC_ARM) || defined(__GNUC__)
#include <stdint.h>
extern uint32_t SystemCoreClock;
void vAssertCalled(const char *fichier, int ligne);
#endif

/* Cortex-M55 / ARMv8.1-M */
#define configENABLE_FPU                        1
#define configENABLE_MVE                        1    /* GCC peut generer des instructions Helium (MVE) pour le M55 */
#define configENABLE_MPU                        0
#define configENABLE_TRUSTZONE                  0
#define configRUN_FREERTOS_SECURE_ONLY          1

/* Ordonnanceur */
#define configUSE_PREEMPTION                    1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0
#define configCPU_CLOCK_HZ                      (SystemCoreClock)
#define configTICK_RATE_HZ                      ((TickType_t)1000)
#define configMAX_PRIORITIES                    (7)
#define configMINIMAL_STACK_SIZE                ((uint16_t)256)
#define configMAX_TASK_NAME_LEN                 (16)
#define configTICK_TYPE_WIDTH_IN_BITS           TICK_TYPE_WIDTH_32_BITS
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1

/* Memoire : allocation dynamique dans un tas de configTOTAL_HEAP_SIZE octets
   (heap_4.c) */
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configSUPPORT_STATIC_ALLOCATION         0
#define configTOTAL_HEAP_SIZE                   ((size_t)(64 * 1024))
#define configAPPLICATION_ALLOCATED_HEAP        0

/* Synchronisation */
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configQUEUE_REGISTRY_SIZE               8

/* Hooks et detection d'erreurs */
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_MALLOC_FAILED_HOOK            1
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_NEWLIB_REENTRANT              0

/* Timers logiciels */
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (2)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            (configMINIMAL_STACK_SIZE * 2)

/* Priorites d'interruption (4 bits de priorite sur le STM32N6)
   Les interruptions de priorite numerique >= 5 peuvent appeler les fonctions
   FreeRTOS "...FromISR" (ex. : Ethernet en priorite 7). */
#define configPRIO_BITS                               4
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY       15
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  5
#define configKERNEL_INTERRUPT_PRIORITY      (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))

/* Fonctions incluses */
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xSemaphoreGetMutexHolder        1

/* Assertion : affiche le fichier et la ligne puis bloque */
#define configASSERT(x)  if ((x) == 0) { vAssertCalled(__FILE__, __LINE__); }

#endif /* FREERTOS_CONFIG_H */
