/*
 * hal_wdt.c
 *
 * Independent Watchdog (IWDG) driver — ported verbatim from frtag2's
 * Hal/src/hal_wdt.c, minus the sleep-current-test prescaler variants
 * (the bootloader never sleeps).
 *
 * ~1.6 s timeout (prescaler 64, reload 4095). Kicked once per internal
 * flash page erase/program during a firmware update — that loop is the
 * only place a >1.6 s stall could otherwise occur.
 */

#include "hal_wdt.h"

static IWDG_HandleTypeDef hiwdg;

void HAL_WDT_vInit(void)
{
    hiwdg.Instance       = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_64;
    hiwdg.Init.Window    = 4095;
    hiwdg.Init.Reload    = 4095;
    HAL_IWDG_Init(&hiwdg);
    HAL_IWDG_Refresh(&hiwdg);
}

void HAL_WDT_vReset(void)
{
    HAL_IWDG_Refresh(&hiwdg);
}
