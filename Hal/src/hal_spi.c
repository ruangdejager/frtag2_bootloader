/*
 * hal_spi.c
 *
 * SPI2 driver for the external NOR flash. See hal_spi.h. GPIO/clock setup
 * for SPI2 (HAL_SPI_MspInit) lives in Core/Src/stm32wlxx_hal_msp.c, per the
 * normal CubeMX split.
 */

#include "hal_spi.h"
#include <string.h>

SPI_HandleTypeDef hFlashSpi;

HAL_StatusTypeDef HAL_SPI_FLASH_vReadPacket(uint8_t *rx, uint16_t len)
{
    uint8_t au8Dummy[64];
    memset(au8Dummy, 0xFF, sizeof(au8Dummy));

    uint16_t u16Off = 0U;
    while (u16Off < len)
    {
        uint16_t u16N = (uint16_t)((len - u16Off) > sizeof(au8Dummy)
                                   ? sizeof(au8Dummy) : (len - u16Off));
        HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(&hFlashSpi, au8Dummy, rx + u16Off, u16N, SPI_TIMEOUT);
        if (status != HAL_OK)
            return status;
        u16Off = (uint16_t)(u16Off + u16N);
    }
    return HAL_OK;
}

void HAL_SPI_vInit(void)
{
    hFlashSpi.Instance               = FLASH_SPI;
    hFlashSpi.Init.Mode              = SPI_MODE_MASTER;
    hFlashSpi.Init.Direction         = SPI_DIRECTION_2LINES;
    hFlashSpi.Init.DataSize          = SPI_DATASIZE_8BIT;
    hFlashSpi.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hFlashSpi.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hFlashSpi.Init.NSS               = SPI_NSS_SOFT;
    hFlashSpi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_64;  /* ~0.5 MHz at 32 MHz PCLK */
    hFlashSpi.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hFlashSpi.Init.TIMode            = SPI_TIMODE_DISABLE;
    hFlashSpi.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hFlashSpi.Init.CRCPolynomial     = 7;
    hFlashSpi.Init.CRCLength         = SPI_CRC_LENGTH_DATASIZE;
    hFlashSpi.Init.NSSPMode          = SPI_NSS_PULSE_ENABLE;

    /* CS is a plain GPIO, initialised here since it's not part of the
     * SPI peripheral's AF pins (those are set up in HAL_SPI_MspInit). */
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin   = BSP_FLASH_CS_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BSP_FLASH_CS_PORT, &gpio);
    HAL_SPI_FLASH_vDeselect();

    HAL_SPI_Init(&hFlashSpi);
}
