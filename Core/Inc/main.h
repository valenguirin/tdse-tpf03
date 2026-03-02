/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f1xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define B1_Pin GPIO_PIN_13
#define B1_GPIO_Port GPIOC
#define B1_EXTI_IRQn EXTI15_10_IRQn
#define SENS_LDR_Pin GPIO_PIN_1
#define SENS_LDR_GPIO_Port GPIOA
#define USART_TX_Pin GPIO_PIN_2
#define USART_TX_GPIO_Port GPIOA
#define USART_RX_Pin GPIO_PIN_3
#define USART_RX_GPIO_Port GPIOA
#define LED_STATUS_Pin GPIO_PIN_4
#define LED_STATUS_GPIO_Port GPIOA
#define LED_SYS_ARMED_Pin GPIO_PIN_5
#define LED_SYS_ARMED_GPIO_Port GPIOA
#define ACT_SIRENA_Pin GPIO_PIN_0
#define ACT_SIRENA_GPIO_Port GPIOB
#define ACT_ESTROBO_Pin GPIO_PIN_1
#define ACT_ESTROBO_GPIO_Port GPIOB
#define SENS_BOTON_PANICO_Pin GPIO_PIN_2
#define SENS_BOTON_PANICO_GPIO_Port GPIOB
#define TO_GSM_RX_Pin GPIO_PIN_10
#define TO_GSM_RX_GPIO_Port GPIOB
#define TO_GSM_TX_Pin GPIO_PIN_11
#define TO_GSM_TX_GPIO_Port GPIOB
#define TO_BLE_RX_Pin GPIO_PIN_9
#define TO_BLE_RX_GPIO_Port GPIOA
#define TO_BLE_TX_Pin GPIO_PIN_10
#define TO_BLE_TX_GPIO_Port GPIOA
#define TMS_Pin GPIO_PIN_13
#define TMS_GPIO_Port GPIOA
#define TCK_Pin GPIO_PIN_14
#define TCK_GPIO_Port GPIOA
#define SWO_Pin GPIO_PIN_3
#define SWO_GPIO_Port GPIOB
#define BLE_STATE_Pin_Pin GPIO_PIN_4
#define BLE_STATE_Pin_GPIO_Port GPIOB
#define RING_GSM_Pin GPIO_PIN_5
#define RING_GSM_GPIO_Port GPIOB
#define RING_GSM_EXTI_IRQn EXTI9_5_IRQn

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
