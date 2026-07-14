/*
 * stm32wlxx_it.c
 *
 * Interrupt service routines for frtag2_bootloader. The bootloader runs
 * entirely polled (no peripheral interrupts enabled), so only the Cortex-M4
 * fault handlers and the SysTick handler HAL_Init() relies on are provided
 * (the TIM16-based HAL timebase this .ioc generates for RTOS builds is not
 * used here — no RTOS).
 */

#include "main.h"
#include "stm32wlxx_it.h"

void NMI_Handler(void)
{
    while (1) { }
}

void HardFault_Handler(void)
{
    while (1) { }
}

void MemManage_Handler(void)
{
    while (1) { }
}

void BusFault_Handler(void)
{
    while (1) { }
}

void UsageFault_Handler(void)
{
    while (1) { }
}

void SVC_Handler(void) { }

void DebugMon_Handler(void) { }

void PendSV_Handler(void) { }

void SysTick_Handler(void)
{
    HAL_IncTick();
}
