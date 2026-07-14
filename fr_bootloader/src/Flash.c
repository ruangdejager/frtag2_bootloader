/*
 * Flash.c
 *
 * AT25EU0041A SPI NOR flash device driver — bootloader port.
 * Follows the fr9/frtag2 flash.c command pattern (JEDEC-compatible command
 * set). Only the operations the bootloader needs are ported: init, read,
 * page write and sector erase (write is needed only for the CONSUMED
 * marker byte; the scratch image itself is only ever read here — the
 * bootloader must never erase it, since it is re-read later to distribute
 * to secondaries).
 */

#include "Flash.h"
#include "Flash_Config.h"
#include "hal_spi.h"
#include "hal_wdt.h"

#define FLASH_WAIT_READY_TIMEOUT_MS   3000U

static bool bDevicePresent = false;

static bool FLASH_bWaitReady(void)
{
    uint32_t u32Start = HAL_GetTick();
    while (FLASH_bDeviceBusy())
    {
        if ((HAL_GetTick() - u32Start) >= FLASH_WAIT_READY_TIMEOUT_MS)
            return false;
        HAL_WDT_vReset();
    }
    return true;
}

static void FLASH_vWriteEnable(void)
{
    uint8_t cmd = FLASH_CMD_WRITE_ENABLE;
    HAL_SPI_FLASH_vSelect();
    HAL_SPI_FLASH_vSpiWritePacket(&cmd, 1);
    HAL_SPI_FLASH_vDeselect();
}

static void FLASH_vWriteDisable(void)
{
    uint8_t cmd = FLASH_CMD_WRITE_DISABLE;
    HAL_SPI_FLASH_vSelect();
    HAL_SPI_FLASH_vSpiWritePacket(&cmd, 1);
    HAL_SPI_FLASH_vDeselect();
}

void FLASH_vInit(void)
{
    uint8_t cmd = FLASH_CMD_RESUME;
    HAL_SPI_FLASH_vSelect();
    HAL_SPI_FLASH_vSpiWritePacket(&cmd, 1);
    HAL_SPI_FLASH_vDeselect();
    HAL_Delay(1);

    uint8_t id[3] = {0};
    cmd = FLASH_CMD_JEDEC_ID;
    HAL_SPI_FLASH_vSelect();
    HAL_SPI_FLASH_vSpiWritePacket(&cmd, 1);
    HAL_SPI_FLASH_vSpiReadPacket(id, 3);
    HAL_SPI_FLASH_vDeselect();

    bDevicePresent = (id[0] == FLASH_MANUFACTURER_ID);
    if (!bDevicePresent)
        return;

    FLASH_vWriteDisable();   /* start disarmed */
}

bool FLASH_bDeviceBusy(void)
{
    return (FLASH_u8ReadStatusReg() & FLASH_STATUS_WIP) != 0U;
}

uint8_t FLASH_u8ReadStatusReg(void)
{
    uint8_t cmd = FLASH_CMD_READ_STATUS;
    uint8_t status = FLASH_STATUS_WIP;
    HAL_SPI_FLASH_vSelect();
    HAL_SPI_FLASH_vSpiWritePacket(&cmd, 1);
    if (HAL_SPI_FLASH_vSpiReadPacket(&status, 1) != HAL_OK)
        status = FLASH_STATUS_WIP;
    HAL_SPI_FLASH_vDeselect();
    return status;
}

bool FLASH_vRead(uint32_t addr, uint8_t *buf, uint16_t len)
{
    if (!bDevicePresent)
        return false;

    uint8_t cmd[4] = {
        FLASH_CMD_READ,
        (uint8_t)((addr >> 16) & 0xFFU),
        (uint8_t)((addr >>  8) & 0xFFU),
        (uint8_t)((addr      ) & 0xFFU),
    };
    if (!FLASH_bWaitReady())
        return false;

    HAL_SPI_FLASH_vSelect();
    bool bOk = (HAL_SPI_FLASH_vSpiWritePacket(cmd, 4) == HAL_OK) &&
               (HAL_SPI_FLASH_vSpiReadPacket(buf, len) == HAL_OK);
    HAL_SPI_FLASH_vDeselect();
    return bOk;
}

bool FLASH_vPageWrite(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    if (!bDevicePresent)
        return false;

    uint8_t cmd[4] = {
        FLASH_CMD_PAGE_PROGRAM,
        (uint8_t)((addr >> 16) & 0xFFU),
        (uint8_t)((addr >>  8) & 0xFFU),
        (uint8_t)((addr      ) & 0xFFU),
    };
    if (!FLASH_bWaitReady())
        return false;

    FLASH_vWriteEnable();
    HAL_SPI_FLASH_vSelect();
    bool bOk = (HAL_SPI_FLASH_vSpiWritePacket(cmd, 4) == HAL_OK) &&
               (HAL_SPI_FLASH_vSpiWritePacket((uint8_t *)buf, len) == HAL_OK);
    HAL_SPI_FLASH_vDeselect();
    if (!bOk)
        FLASH_vWriteDisable();
    return bOk;
}

bool FLASH_vSectorErase(uint32_t addr)
{
    if (!bDevicePresent)
        return false;

    uint8_t cmd[4] = {
        FLASH_CMD_SECTOR_ERASE,
        (uint8_t)((addr >> 16) & 0xFFU),
        (uint8_t)((addr >>  8) & 0xFFU),
        (uint8_t)((addr      ) & 0xFFU),
    };
    if (!FLASH_bWaitReady())
        return false;

    FLASH_vWriteEnable();
    HAL_SPI_FLASH_vSelect();
    bool bOk = (HAL_SPI_FLASH_vSpiWritePacket(cmd, 4) == HAL_OK);
    HAL_SPI_FLASH_vDeselect();
    if (!bOk)
        FLASH_vWriteDisable();
    return bOk;
}
