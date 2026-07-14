/*
 * stm32wlxx_hal_msp.c
 *
 * MSP init for frtag2_bootloader — trimmed to the two peripherals the
 * bootloader actually uses (RTC, for the TAMP backup registers; SPI2, for
 * the external NOR flash). The ADC/UART/SPI1(ACC) Msp blocks CubeMX
 * generates from frtag2's full board pinout are not needed here.
 */

#include "main.h"

void HAL_MspInit(void)
{
    HAL_NVIC_SetPriority(PendSV_IRQn, 15, 0);
}

/**
 * @brief RTC MSP Initialization
 */
void HAL_RTC_MspInit(RTC_HandleTypeDef *hrtc)
{
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    if (hrtc->Instance != RTC)
        return;

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInitStruct.RTCClockSelection    = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
        Error_Handler();

    __HAL_RCC_RTC_ENABLE();
    __HAL_RCC_RTCAPB_CLK_ENABLE();
}

void HAL_RTC_MspDeInit(RTC_HandleTypeDef *hrtc)
{
    if (hrtc->Instance != RTC)
        return;

    __HAL_RCC_RTC_DISABLE();
    __HAL_RCC_RTCAPB_CLK_DISABLE();
}

/**
 * @brief SPI MSP Initialization — SPI2 only (external NOR flash bus,
 * labelled "SD_*" in the .ioc pinout).
 */
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (hspi->Instance != SPI2)
        return;

    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* PA5 (MISO) — AF3 */
    GPIO_InitStruct.Pin       = SD_SO_Pin;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF3_SPI2;
    HAL_GPIO_Init(SD_SO_GPIO_Port, &GPIO_InitStruct);

    /* PA8 (SCK) + PA10 (MOSI) — AF5 */
    GPIO_InitStruct.Pin       = SD_SCLK_Pin | SD_SI_Pin;
    GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void HAL_SPI_MspDeInit(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance != SPI2)
        return;

    __HAL_RCC_SPI2_CLK_DISABLE();
    HAL_GPIO_DeInit(GPIOA, SD_SO_Pin | SD_SCLK_Pin | SD_SI_Pin);
}
