/**
  ******************************************************************************
  * @file    main.c
  * @brief   TP2 ETRS923 - Interface capteurs & STM32 sur NUCLEO-N657X0-Q
  *
  *  Point d'entree commun aux 3 parties du TP. La partie executee est choisie
  *  par PARTIE_TP dans main.h (ou "make PARTIE=x").
  *
  *  Base de code : configuration d'horloge, USART1 et ETH1 reprises de
  *  l'exemple ST "Nx_TCP_Echo_Client" (STM32CubeN6) pour cette carte.
  ******************************************************************************
  */
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

/* Handles des peripheriques -------------------------------------------------*/
UART_HandleTypeDef huart1;   /* USART1 = console (Virtual COM Port ST-LINK)  */
I2C_HandleTypeDef  hi2c1;    /* I2C1   = connecteur Arduino -> IKS01A3       */

extern TIM_HandleTypeDef htim6;  /* base de temps HAL (stm32n6xx_hal_timebase_tim.c) */

/* Prototypes ----------------------------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
static void SystemIsolation_Config(void);

/**
  * @brief  Point d'entree du programme.
  */
int main(void)
{
  /* Cache instructions active. Le cache de donnees (D-cache) est volontairement
     laisse DESACTIVE : le DMA Ethernet lit/ecrit directement la RAM, sans cache
     on n'a aucune operation de nettoyage/invalidation a gerer. */
  SCB_EnableICache();

  /* Initialisation de la HAL (la base de temps 1 ms est faite par TIM6, pour
     laisser le SysTick a FreeRTOS dans la partie 3). */
  HAL_Init();

  /* Horloges : CPU 600 MHz, bus 400 MHz, AHB/APB 200 MHz */
  SystemClock_Config();

  /* Peripheriques */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  SystemIsolation_Config();

  /* printf sans tampon : chaque caractere part directement sur l'UART */
  setvbuf(stdout, NULL, _IONBF, 0);

  printf("\r\n\r\n=== TP2 ETRS923 - NUCLEO-N657X0-Q - partie %d ===\r\n", PARTIE_TP);

  if (PARTIE_TP == 1)
  {
    partie1_lancer();
  }
  else if (PARTIE_TP == 2)
  {
    partie2_lancer();
  }
  else if (PARTIE_TP == 4)
  {
    partie4_lancer();   /* TP3 : inference du reseau de neurones */
  }
  else if (PARTIE_TP == 5)
  {
    partie5_lancer();   /* TP3 : Cloud contre Edge (ne revient jamais) */
  }
  else
  {
    partie3_lancer();   /* ne revient jamais : demarre FreeRTOS */
  }

  while (1)
  {
  }
}

/**
  * @brief  Attente en millisecondes, utilisable avec ou sans FreeRTOS.
  *         Si l'ordonnanceur tourne, on rend la main aux autres taches.
  */
void attendre_ms(uint32_t ms)
{
  if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
  {
    vTaskDelay(pdMS_TO_TICKS(ms));
  }
  else
  {
    HAL_Delay(ms);
  }
}

/**
  * @brief  Redirection de printf vers USART1 (appelee par _write de syscalls.c)
  */
int __io_putchar(int ch)
{
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
  return ch;
}

/**
  * @brief  Configuration des horloges (identique a l'exemple ST).
  *         CPU = 600 MHz, SYSB (AXI) = 400 MHz, HCLK = PCLKx = 200 MHz
  *         PLL2 = 1600 MHz -> IC9 / 16 = 100 MHz pour l'USART1
  */
void SystemClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  if (HAL_PWREx_ConfigSupply(PWR_EXTERNAL_SOURCE_SUPPLY) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /* HSI 64 MHz */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSIDiv = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /* Prescaler des timers (TIM6 = base de temps HAL) */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_TIM;
  PeriphClkInitStruct.TIMPresSelection = RCC_TIMPRES_DIV1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /* Si le CPU tourne deja sur les PLL (redemarrage a chaud), on repasse
     temporairement sur HSI avant de reconfigurer les PLL */
  HAL_RCC_GetClockConfig(&RCC_ClkInitStruct);
  if ((RCC_ClkInitStruct.CPUCLKSource == RCC_CPUCLKSOURCE_IC1) ||
      (RCC_ClkInitStruct.SYSCLKSource == RCC_SYSCLKSOURCE_IC2_IC6_IC11))
  {
    RCC_ClkInitStruct.ClockType = (RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_SYSCLK);
    RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_HSI;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }
  }

  /* PLL1 = 64 / 4 * 75 = 1200 MHz ; PLL2 = 64 / 1 * 25 = 1600 MHz */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_NONE;
  RCC_OscInitStruct.PLL1.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL1.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL1.PLLM = 4;
  RCC_OscInitStruct.PLL1.PLLN = 75;
  RCC_OscInitStruct.PLL1.PLLFractional = 0;
  RCC_OscInitStruct.PLL1.PLLP1 = 1;
  RCC_OscInitStruct.PLL1.PLLP2 = 1;
  RCC_OscInitStruct.PLL2.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL2.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL2.PLLM = 1;
  RCC_OscInitStruct.PLL2.PLLN = 25;
  RCC_OscInitStruct.PLL2.PLLFractional = 0;
  RCC_OscInitStruct.PLL2.PLLP1 = 1;
  RCC_OscInitStruct.PLL2.PLLP2 = 1;
  RCC_OscInitStruct.PLL3.PLLState = RCC_PLL_NONE;
  RCC_OscInitStruct.PLL4.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /* CPU = IC1 = 1200/2 = 600 MHz ; SYSB = IC2 = 1200/3 = 400 MHz ; HCLK = 200 MHz */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_CPUCLK | RCC_CLOCKTYPE_HCLK
                              | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1
                              | RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK5
                              | RCC_CLOCKTYPE_PCLK4;
  RCC_ClkInitStruct.CPUCLKSource = RCC_CPUCLKSOURCE_IC1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_IC2_IC6_IC11;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;
  RCC_ClkInitStruct.APB5CLKDivider = RCC_APB5_DIV1;
  RCC_ClkInitStruct.IC1Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC1Selection.ClockDivider = 2;
  RCC_ClkInitStruct.IC2Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC2Selection.ClockDivider = 3;
  RCC_ClkInitStruct.IC6Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC6Selection.ClockDivider = 4;
  RCC_ClkInitStruct.IC11Selection.ClockSelection = RCC_ICCLKSOURCE_PLL1;
  RCC_ClkInitStruct.IC11Selection.ClockDivider = 3;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  USART1 : 115200 bauds, 8 bits, pas de parite, 1 stop (8N1)
  */
static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  I2C1 en mode standard 100 kHz.
  *         Horloge I2C1 = PCLK1 = 200 MHz.
  *         Valeur Timing calculee avec l'algorithme du BSP ST
  *         (stm32n6xx_nucleo_bus.c, I2C_GetTiming(200 MHz, 100 kHz)).
  *         Pour 400 kHz (fast mode) : 0x60911523
  */
static void MX_I2C1_Init(void)
{
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x40D2B6B9;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief  LEDs en sortie push-pull, eteintes au demarrage (niveau haut).
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOG_CLK_ENABLE();

  /* LEDs actives a l'etat bas : SET = eteinte */
  HAL_GPIO_WritePin(GPIOG, LED_BLEUE_Pin | LED_ROUGE_Pin | LED_VERTE_Pin, GPIO_PIN_SET);

  GPIO_InitStruct.Pin = LED_BLEUE_Pin | LED_ROUGE_Pin | LED_VERTE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
}

/**
  * @brief  Isolation (RIF) : le DMA Ethernet (maitre) doit avoir les memes
  *         attributs (securise, privilegie) que le code FSBL pour acceder a la
  *         RAM. Repris de l'exemple ST Nx_TCP_Echo_Client.
  */
static void SystemIsolation_Config(void)
{
  RIMC_MasterConfig_t RIMC_master = {0};

  __HAL_RCC_RIFSC_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();

  RIMC_master.MasterCID = RIF_CID_1;
  RIMC_master.SecPriv = RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV;
  HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_ETH1, &RIMC_master);
  HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_ETH1, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);

  /* Broches : USART1, ETH1, LEDs, I2C1 */
  HAL_GPIO_ConfigPinAttributes(GPIOE, GPIO_PIN_5, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOE, GPIO_PIN_6, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF, GPIO_PIN_4, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF, GPIO_PIN_5, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF, GPIO_PIN_7, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF, GPIO_PIN_10, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF, GPIO_PIN_11, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF, GPIO_PIN_12, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF, GPIO_PIN_13, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF, GPIO_PIN_14, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOF, GPIO_PIN_15, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG, GPIO_PIN_0, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG, GPIO_PIN_8, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG, GPIO_PIN_10, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOG, GPIO_PIN_11, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOH, GPIO_PIN_9, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
  HAL_GPIO_ConfigPinAttributes(GPIOC, GPIO_PIN_1, GPIO_PIN_SEC | GPIO_PIN_NPRIV);
}

/**
  * @brief  Interruption de TIM6 toutes les 1 ms : avance le compteur HAL
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
}

/**
  * @brief  Erreur fatale : LED rouge qui clignote vite, programme bloque.
  */
void Error_Handler(void)
{
  volatile uint32_t i;

  __disable_irq();
  __HAL_RCC_GPIOG_CLK_ENABLE();
  while (1)
  {
    HAL_GPIO_TogglePin(LED_ROUGE_GPIO_Port, LED_ROUGE_Pin);
    for (i = 0; i < 20000000; i++)
    {
    }
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  printf("assert_failed : %s ligne %lu\r\n", file, (unsigned long)line);
  Error_Handler();
}
#endif
