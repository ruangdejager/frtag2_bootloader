/*
 * hal_rtc.h
 *
 * RTC backup-domain access for frtag2_bootloader — trimmed to just what
 * the bootloader needs: unlocking TAMP->BKPxR for the app<->bootloader OTA
 * handoff (OtaStore_Config.h) and the bootloader's own version register.
 * No calendar, no wakeup timer, no cmsis_os2 dependency (unlike frtag2's
 * own Hal/inc/hal_rtc.h, which owns the 1 Hz heartbeat wakeup for the RTOS
 * build).
 */

#ifndef INC_HAL_RTC_H_
#define INC_HAL_RTC_H_

#include "stm32wlxx_hal.h"

extern RTC_HandleTypeDef hrtc;

void HAL_RTC_vInitBackupDomain(void);

#endif /* INC_HAL_RTC_H_ */
