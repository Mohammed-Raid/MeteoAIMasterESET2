/**
  ******************************************************************************
  * @file    stm32n6xx_hal_msp.c
  * @brief   Initialisation "bas niveau" des peripheriques : horloges, broches,
  *          interruptions. Appelees automatiquement par HAL_xxx_Init().
  ******************************************************************************
  */
#include "main.h"

void HAL_MspInit(void)
{
}

/**
  * @brief  USART1 : PE5 = TX, PE6 = RX (Virtual COM Port du ST-LINK)
  */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  if (huart->Instance == USART1)
  {
    /* Horloge USART1 = IC9 = PLL2 / 16 = 100 MHz */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_USART1;
    PeriphClkInitStruct.Usart1ClockSelection = RCC_USART1CLKSOURCE_IC9;
    PeriphClkInitStruct.ICSelection[RCC_IC9].ClockSelection = RCC_ICCLKSOURCE_PLL2;
    PeriphClkInitStruct.ICSelection[RCC_IC9].ClockDivider = 16;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_5 | GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);
  }
}

/**
  * @brief  I2C1 sur le connecteur Arduino : PH9 = SCL (D15), PC1 = SDA (D14)
  *         Sorties en drain ouvert (obligatoire en I2C), fonction alternative AF4.
  */
void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  if (hi2c->Instance == I2C1)
  {
    /* Horloge I2C1 = PCLK1 = 200 MHz (la valeur Timing en depend) */
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
    PeriphClkInitStruct.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitStruct.Pin = CAPTEURS_I2C_SCL_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(CAPTEURS_I2C_SCL_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = CAPTEURS_I2C_SDA_Pin;
    HAL_GPIO_Init(CAPTEURS_I2C_SDA_GPIO_Port, &GPIO_InitStruct);

    __HAL_RCC_I2C1_CLK_ENABLE();
  }
}

/**
  * @brief  ETH1 en RMII avec le PHY LAN8742 de la carte (exemple ST)
  *         PF4 MDIO, PG11 MDC, PF5 ETH1_CLK, PF7 REF_CLK, PF10 CRS_DV,
  *         PF11 TX_EN, PF12 TXD0, PF13 TXD1, PF14 RXD0, PF15 RXD1
  */
void HAL_ETH_MspInit(ETH_HandleTypeDef *heth)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  if (heth->Instance == ETH1)
  {
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ETH1;
    PeriphClkInitStruct.Eth1ClockSelection = RCC_ETH1CLKSOURCE_HCLK;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_RCC_ETH1_CLK_ENABLE();
    __HAL_RCC_ETH1MAC_CLK_ENABLE();
    __HAL_RCC_ETH1TX_CLK_ENABLE();
    __HAL_RCC_ETH1RX_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_7 | GPIO_PIN_10
                        | GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14
                        | GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF11_ETH1;
    HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_11;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    /* Priorite 7 : >= configMAX_SYSCALL_INTERRUPT_PRIORITY (5) car
       l'interruption utilise des fonctions FreeRTOS "FromISR" */
    HAL_NVIC_SetPriority(ETH1_IRQn, 7, 0);
    HAL_NVIC_EnableIRQ(ETH1_IRQn);
  }
}
