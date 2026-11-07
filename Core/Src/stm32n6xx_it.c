/**
  ******************************************************************************
  * @file    stm32n6xx_it.c
  * @brief   Gestionnaires d'interruption et d'exceptions.
  ******************************************************************************
  */
#include "main.h"
#include "stm32n6xx_it.h"

extern TIM_HandleTypeDef htim6;
extern ETH_HandleTypeDef heth1;

/* ---- Exceptions du Cortex-M55 : en cas de faute, on bloque le programme ---*/
void NMI_Handler(void)
{
  while (1)
  {
  }
}

void HardFault_Handler(void)
{
  Error_Handler();
}

void MemManage_Handler(void)
{
  Error_Handler();
}

void BusFault_Handler(void)
{
  Error_Handler();
}

void UsageFault_Handler(void)
{
  Error_Handler();
}

void SecureFault_Handler(void)
{
  Error_Handler();
}

void DebugMon_Handler(void)
{
}

/* ---- Peripheriques --------------------------------------------------------*/

/* TIM6 : base de temps 1 ms de la HAL (HAL_GetTick, HAL_Delay) */
void TIM6_IRQHandler(void)
{
  HAL_TIM_IRQHandler(&htim6);
}

/* Ethernet : reception / emission de trames (partie 3) */
void ETH1_IRQHandler(void)
{
  HAL_ETH_IRQHandler(&heth1);
}
