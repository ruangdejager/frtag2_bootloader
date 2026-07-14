/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file — trimmed for frtag2_bootloader.
  *                   Only the pins the bootloader actually touches (status
  *                   LEDs, external NOR flash SPI2 bus) are kept; the rest
  *                   of frtag2's board pinout (GNSS, ACC, ADC, debug UART)
  *                   is not needed here.
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

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32wlxx_hal.h"

void Error_Handler(void);

/* -----------------------------------------------------------------------
 * LEDs
 * ----------------------------------------------------------------------- */
#define LED_RED_Pin GPIO_PIN_5
#define LED_RED_GPIO_Port GPIOB
#define LED_YELLOW_Pin GPIO_PIN_8
#define LED_YELLOW_GPIO_Port GPIOB

/* -----------------------------------------------------------------------
 * External Flash SPI2 (labelled "SD_*" by the .ioc pinout, same physical
 * bus frtag2 calls its NOR flash bus)
 * ----------------------------------------------------------------------- */
#define SD_SO_Pin GPIO_PIN_5
#define SD_SO_GPIO_Port GPIOA
#define SD_SCLK_Pin GPIO_PIN_8
#define SD_SCLK_GPIO_Port GPIOA
#define SD_SI_Pin GPIO_PIN_10
#define SD_SI_GPIO_Port GPIOA
#define SD_CS_Pin GPIO_PIN_15
#define SD_CS_GPIO_Port GPIOA

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
