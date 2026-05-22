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
#include "stm32f4xx_hal.h"

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
#define PIN_CAMARAS_Pin GPIO_PIN_1
#define PIN_CAMARAS_GPIO_Port GPIOA
#define LORA_RST_Pin GPIO_PIN_3
#define LORA_RST_GPIO_Port GPIOA
#define LORA_NSS_Pin GPIO_PIN_4
#define LORA_NSS_GPIO_Port GPIOA
#define LORA_DIO0_Pin GPIO_PIN_1
#define LORA_DIO0_GPIO_Port GPIOB
#define LED_ROJO_Pin GPIO_PIN_15
#define LED_ROJO_GPIO_Port GPIOA
#define LED_VERDE_Pin GPIO_PIN_3
#define LED_VERDE_GPIO_Port GPIOB
#define LED_AZUL_Pin GPIO_PIN_4
#define LED_AZUL_GPIO_Port GPIOB
#define PIN_LIBERACION_Pin GPIO_PIN_5
#define PIN_LIBERACION_GPIO_Port GPIOB
#define BNO_INT_Pin GPIO_PIN_8
#define BNO_INT_GPIO_Port GPIOB
#define BNO_RST_Pin GPIO_PIN_9
#define BNO_RST_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
