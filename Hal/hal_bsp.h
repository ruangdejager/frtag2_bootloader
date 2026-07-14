/*
 * hal_bsp.h
 *
 * Board pin map for frtag2_bootloader — the subset of frtag2's board
 * pinout the bootloader actually touches: status LEDs and the external
 * NOR flash SPI2 bus. Must stay in sync with frtag2's own Hal/hal_bsp.h
 * if the board pinout ever changes.
 */

#ifndef HAL_BSP_H_
#define HAL_BSP_H_

#include "stm32wlxx_hal.h"
#include "main.h"

/* -----------------------------------------------------------------------
 * LEDs
 * ----------------------------------------------------------------------- */
#define BSP_LED_RED_PORT        LED_RED_GPIO_Port
#define BSP_LED_RED_PIN         LED_RED_Pin
#define BSP_LED_YELLOW_PORT     LED_YELLOW_GPIO_Port
#define BSP_LED_YELLOW_PIN      LED_YELLOW_Pin

/* -----------------------------------------------------------------------
 * External Flash SPI (SPI2) — AT25EU0041A-SSHN-T, 512 KB NOR flash
 * ----------------------------------------------------------------------- */
#define BSP_FLASH_MISO_PORT         SD_SO_GPIO_Port
#define BSP_FLASH_MISO_PIN          SD_SO_Pin
#define BSP_FLASH_SCK_PORT          SD_SCLK_GPIO_Port
#define BSP_FLASH_SCK_PIN           SD_SCLK_Pin
#define BSP_FLASH_MOSI_PORT         SD_SI_GPIO_Port
#define BSP_FLASH_MOSI_PIN          SD_SI_Pin
#define BSP_FLASH_CS_PORT           SD_CS_GPIO_Port
#define BSP_FLASH_CS_PIN            SD_CS_Pin

#endif /* HAL_BSP_H_ */
