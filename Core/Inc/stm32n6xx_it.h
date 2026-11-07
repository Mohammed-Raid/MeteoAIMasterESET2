/**
  ******************************************************************************
  * @file    stm32n6xx_it.h
  * @brief   Prototypes des gestionnaires d'interruption.
  ******************************************************************************
  */
#ifndef __STM32N6xx_IT_H
#define __STM32N6xx_IT_H

void NMI_Handler(void);
void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);
void SecureFault_Handler(void);
void DebugMon_Handler(void);
void TIM6_IRQHandler(void);
void ETH1_IRQHandler(void);

/* SVC_Handler, PendSV_Handler et SysTick_Handler sont fournis par le portage
   FreeRTOS (Middlewares/Third_Party/FreeRTOS/.../ARM_CM55_NTZ) */

#endif /* __STM32N6xx_IT_H */
