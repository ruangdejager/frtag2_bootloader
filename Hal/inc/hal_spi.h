/*
 * hal_spi.h
 *
 * SPI2 driver for the external NOR flash (AT25EU0041A). Ported from
 * frtag2's Hal/inc/hal_spi.h with the accelerometer bus (SPI1), the
 * deep-power-down "ensure awake" tracking and the MicroSD alt-population
 * dropped — none apply to a bootloader that runs once, does one job and
 * resets.
 */

#ifndef INC_HAL_SPI_H_
#define INC_HAL_SPI_H_

#include <stdint.h>
#include "stm32wlxx_hal.h"
#include "hal_bsp.h"

#define SPI_TIMEOUT 1000U

extern SPI_HandleTypeDef hFlashSpi;

#define FLASH_SPI               SPI2
#define FLASH_SPI_CLK_ENABLE()  __HAL_RCC_SPI2_CLK_ENABLE()
#define FLASH_PORT_CLK_ENABLE() __HAL_RCC_GPIOA_CLK_ENABLE()

void HAL_SPI_vInit(void);

/* Full-duplex read: clocks out 0xFF dummies to clock in 'len' bytes.
 * Required because HAL_SPI_Receive() does NOT generate clock in 2-line
 * master mode. */
HAL_StatusTypeDef HAL_SPI_FLASH_vReadPacket(uint8_t *rx, uint16_t len);

#define HAL_SPI_FLASH_vSelect()                     HAL_GPIO_WritePin(BSP_FLASH_CS_PORT, BSP_FLASH_CS_PIN, GPIO_PIN_RESET)
#define HAL_SPI_FLASH_vDeselect()                   HAL_GPIO_WritePin(BSP_FLASH_CS_PORT, BSP_FLASH_CS_PIN, GPIO_PIN_SET)
#define HAL_SPI_FLASH_vSpiWritePacket(tx, len)      HAL_SPI_Transmit(&hFlashSpi, (uint8_t *)(tx), (len), SPI_TIMEOUT)
#define HAL_SPI_FLASH_vSpiReadPacket(rx, len)       HAL_SPI_FLASH_vReadPacket((rx), (len))

#endif /* INC_HAL_SPI_H_ */
