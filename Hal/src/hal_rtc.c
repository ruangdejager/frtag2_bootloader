/*
 * hal_rtc.c
 *
 * RTC backup-domain access. HAL_RTC_Init() is what actually triggers
 * HAL_RTC_MspInit() (Core/Src/stm32wlxx_hal_msp.c), which is what unlocks
 * the backup domain and enables the RTC/RTCAPB clocks that TAMP->BKPxR
 * needs — the calendar itself is otherwise unused by the bootloader.
 */

#include "hal_rtc.h"
#include "main.h"

RTC_HandleTypeDef hrtc;

void HAL_RTC_vInitBackupDomain(void)
{
    hrtc.Instance            = RTC;
    hrtc.Init.HourFormat     = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv   = 127;
    hrtc.Init.SynchPrediv    = 255;
    hrtc.Init.OutPut         = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutRemap    = RTC_OUTPUT_REMAP_NONE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType     = RTC_OUTPUT_TYPE_OPENDRAIN;

    if (HAL_RTC_Init(&hrtc) != HAL_OK)
        Error_Handler();

    HAL_PWR_EnableBkUpAccess();
}
