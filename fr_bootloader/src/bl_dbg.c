/*
 * bl_dbg.c
 *
 * See bl_dbg.h. USART2, PA2 (TX only — nothing needs to talk back to the
 * bootloader), 115200 8N1, polled TXE. BRR computed for 48 MHz PCLK1
 * (matches SystemClock_Config's PLL output, same tree as frtag2's app).
 */

#include "bl_dbg.h"
#include "stm32wlxx_hal.h"

void BL_DBG_vInit(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_USART2_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin       = GPIO_PIN_2;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &gpio);

    USART2->BRR = 417U;   /* 48 MHz / 115200, OVER8=0 */
    USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void BL_DBG_vPutChar(char c)
{
    uint32_t u32Start = HAL_GetTick();
    while (!(USART2->ISR & USART_ISR_TXE_TXFNF))
    {
        if ((HAL_GetTick() - u32Start) > 100U)
            return;   /* never hang the bootloader on a disconnected debug port */
    }
    USART2->TDR = (uint8_t)c;
}

void BL_DBG_vPuts(const char *s)
{
    while (*s)
        BL_DBG_vPutChar(*s++);
}

void BL_DBG_vPutHex32(uint32_t v)
{
    char buf[9];
    for (int8_t i = 7; i >= 0; i--)
    {
        uint8_t u8Nib = (uint8_t)((v >> (i * 4)) & 0xFU);
        buf[7 - i] = (char)((u8Nib < 10U) ? ('0' + u8Nib) : ('A' + u8Nib - 10U));
    }
    buf[8] = 0;
    BL_DBG_vPuts(buf);
}

void BL_DBG_vPutHex8(uint8_t v)
{
    char buf[3];
    buf[0] = (char)(((v >> 4) < 10U) ? ('0' + (v >> 4)) : ('A' + (v >> 4) - 10U));
    buf[1] = (char)(((v & 0xFU) < 10U) ? ('0' + (v & 0xFU)) : ('A' + (v & 0xFU) - 10U));
    buf[2] = 0;
    BL_DBG_vPuts(buf);
}

void BL_DBG_vPutDec32(uint32_t v)
{
    char buf[11];   /* max 10 digits for a uint32_t + NUL */
    uint8_t u8Idx = sizeof(buf) - 1U;
    buf[u8Idx] = 0;
    do {
        buf[--u8Idx] = (char)('0' + (v % 10U));
        v /= 10U;
    } while (v > 0U);
    BL_DBG_vPuts(&buf[u8Idx]);
}
