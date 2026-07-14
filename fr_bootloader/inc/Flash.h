/*
 * Flash.h
 *
 * AT25EU0041A SPI NOR flash device driver — bootloader-side port of
 * frtag2's fr_app/Device/Flash/Flash.c, minus its RTOS delay/dbg_log
 * dependencies (HAL_Delay/HAL_GetTick in place of osDelay/osKernelGetTickCount;
 * no logging).
 */

#ifndef FR_BOOTLOADER_FLASH_H_
#define FR_BOOTLOADER_FLASH_H_

#include <stdint.h>
#include <stdbool.h>

void    FLASH_vInit(void);
bool    FLASH_bDeviceBusy(void);
uint8_t FLASH_u8ReadStatusReg(void);
bool    FLASH_vRead(uint32_t addr, uint8_t *buf, uint16_t len);
bool    FLASH_vPageWrite(uint32_t addr, const uint8_t *buf, uint16_t len);
bool    FLASH_vSectorErase(uint32_t addr);

#endif /* FR_BOOTLOADER_FLASH_H_ */
