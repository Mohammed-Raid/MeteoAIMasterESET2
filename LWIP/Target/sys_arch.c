/**
  ******************************************************************************
  * @file    sys_arch.c
  * @brief   Couche d'abstraction OS de lwIP pour FreeRTOS :
  *          semaphores, mutex, boites aux lettres (files), taches, temps.
  ******************************************************************************
  */
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/err.h"
#include "stm32n6xx_hal.h"

/* ---- Initialisation et temps ----------------------------------------------*/
void sys_init(void)
{
}

u32_t sys_now(void)
{
  return (u32_t)HAL_GetTick();
}

/* ---- Semaphores ------------------------------------------------------------*/
err_t sys_sem_new(sys_sem_t *sem, u8_t count)
{
  *sem = xSemaphoreCreateCounting(0xFFFF, count);
  if (*sem == NULL)
  {
    return ERR_MEM;
  }
  return ERR_OK;
}

void sys_sem_free(sys_sem_t *sem)
{
  vSemaphoreDelete(*sem);
  *sem = NULL;
}

void sys_sem_signal(sys_sem_t *sem)
{
  xSemaphoreGive(*sem);
}

/* Retourne le temps d'attente en ms, ou SYS_ARCH_TIMEOUT.
   timeout = 0 signifie "attendre indefiniment" (convention lwIP) */
u32_t sys_arch_sem_wait(sys_sem_t *sem, u32_t timeout)
{
  TickType_t debut = xTaskGetTickCount();
  TickType_t attente;

  if (timeout == 0)
  {
    attente = portMAX_DELAY;
  }
  else
  {
    attente = pdMS_TO_TICKS(timeout);
    if (attente == 0)
    {
      attente = 1;
    }
  }

  if (xSemaphoreTake(*sem, attente) == pdTRUE)
  {
    return (u32_t)((xTaskGetTickCount() - debut) * portTICK_PERIOD_MS);
  }
  return SYS_ARCH_TIMEOUT;
}

/* ---- Mutex -----------------------------------------------------------------*/
err_t sys_mutex_new(sys_mutex_t *mutex)
{
  *mutex = xSemaphoreCreateMutex();
  if (*mutex == NULL)
  {
    return ERR_MEM;
  }
  return ERR_OK;
}

void sys_mutex_free(sys_mutex_t *mutex)
{
  vSemaphoreDelete(*mutex);
  *mutex = NULL;
}

void sys_mutex_lock(sys_mutex_t *mutex)
{
  xSemaphoreTake(*mutex, portMAX_DELAY);
}

void sys_mutex_unlock(sys_mutex_t *mutex)
{
  xSemaphoreGive(*mutex);
}

/* ---- Boites aux lettres (files de pointeurs) -------------------------------*/
err_t sys_mbox_new(sys_mbox_t *mbox, int size)
{
  *mbox = xQueueCreate((UBaseType_t)size, sizeof(void *));
  if (*mbox == NULL)
  {
    return ERR_MEM;
  }
  return ERR_OK;
}

void sys_mbox_free(sys_mbox_t *mbox)
{
  vQueueDelete(*mbox);
  *mbox = NULL;
}

void sys_mbox_post(sys_mbox_t *mbox, void *msg)
{
  while (xQueueSendToBack(*mbox, &msg, portMAX_DELAY) != pdTRUE)
  {
  }
}

err_t sys_mbox_trypost(sys_mbox_t *mbox, void *msg)
{
  if (xQueueSendToBack(*mbox, &msg, 0) == pdTRUE)
  {
    return ERR_OK;
  }
  return ERR_MEM;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *mbox, void *msg)
{
  BaseType_t reveil = pdFALSE;

  if (xQueueSendToBackFromISR(*mbox, &msg, &reveil) == pdTRUE)
  {
    portYIELD_FROM_ISR(reveil);
    return ERR_OK;
  }
  return ERR_MEM;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *mbox, void **msg, u32_t timeout)
{
  void *poubelle;
  TickType_t debut = xTaskGetTickCount();
  TickType_t attente;

  if (msg == NULL)
  {
    msg = &poubelle;
  }

  if (timeout == 0)
  {
    attente = portMAX_DELAY;
  }
  else
  {
    attente = pdMS_TO_TICKS(timeout);
    if (attente == 0)
    {
      attente = 1;
    }
  }

  if (xQueueReceive(*mbox, msg, attente) == pdTRUE)
  {
    return (u32_t)((xTaskGetTickCount() - debut) * portTICK_PERIOD_MS);
  }
  *msg = NULL;
  return SYS_ARCH_TIMEOUT;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *mbox, void **msg)
{
  void *poubelle;

  if (msg == NULL)
  {
    msg = &poubelle;
  }
  if (xQueueReceive(*mbox, msg, 0) == pdTRUE)
  {
    return 0;
  }
  return SYS_MBOX_EMPTY;
}

/* ---- Taches ----------------------------------------------------------------
   stacksize est donne en OCTETS par lwIP, FreeRTOS attend des MOTS de 32 bits */
sys_thread_t sys_thread_new(const char *name, lwip_thread_fn thread, void *arg,
                            int stacksize, int prio)
{
  TaskHandle_t tache = NULL;

  if (xTaskCreate(thread, name, (configSTACK_DEPTH_TYPE)(stacksize / 4), arg,
                  (UBaseType_t)prio, &tache) != pdPASS)
  {
    printf("sys_thread_new : creation de la tache %s impossible\r\n", name);
  }
  return tache;
}

/* ---- Section critique ------------------------------------------------------*/
sys_prot_t sys_arch_protect(void)
{
  taskENTER_CRITICAL();
  return 1;
}

void sys_arch_unprotect(sys_prot_t pval)
{
  (void)pval;
  taskEXIT_CRITICAL();
}

/* ---- Generateur pseudo-aleatoire (congruence lineaire) ---------------------*/
unsigned int lwip_port_rand(void)
{
  static uint32_t graine = 0;

  if (graine == 0)
  {
    graine = HAL_GetTick() ^ 0x5A5A1234U;
  }
  graine = graine * 1103515245U + 12345U;
  return (unsigned int)(graine >> 1);
}
