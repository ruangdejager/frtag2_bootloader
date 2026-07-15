/*
 * bl_dbg.h
 *
 * Minimal polling-only debug UART for frtag2_bootloader — USART2 (the same
 * debug port frtag2's own app uses, PA2 TX), bare-register, no HAL_UART
 * driver and no libc stdio (both would eat into the 20 KB budget). Just
 * enough to see which branch the OTA logic took on a given boot: metadata
 * valid/consumed, computed vs. stored XOR-8, program result.
 */

#ifndef FR_BOOTLOADER_BL_DBG_H_
#define FR_BOOTLOADER_BL_DBG_H_

#include <stdint.h>

void BL_DBG_vInit(void);
void BL_DBG_vPuts(const char *s);
void BL_DBG_vPutHex32(uint32_t v);
void BL_DBG_vPutHex8(uint8_t v);
void BL_DBG_vPutDec32(uint32_t v);

#endif /* FR_BOOTLOADER_BL_DBG_H_ */
