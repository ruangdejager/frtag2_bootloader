/*
 * main.c  (fr_bootloader/src/main.c)
 *
 * Application entry point for frtag2_bootloader — a minimal bare-metal OTA
 * bootloader for frtag2. No RTOS, no coroutines: runs once per reset,
 * polling only.
 *
 * Reads the OTA image metadata from the external NOR flash (contract
 * defined in fr_bootloader/inc/OtaStore_Config.h, shared verbatim with the
 * frtag2 application), and:
 *
 *   - if the record is VALID and not yet CONSUMED, verifies its XOR-8 and
 *     programs internal flash 0x08005000..stopAddr from the scratchpad,
 *     then marks CONSUMED (a NOR 1->0 write, no erase needed);
 *   - on any mismatch, or once already CONSUMED, leaves internal flash
 *     alone.
 *
 * The scratchpad image is NEVER erased here — the primary re-reads it
 * later to distribute the same image to secondaries over LoRa.
 *
 * Bootloader version is written to TAMP->BKP2R on every boot (frtag2's own
 * OTA_BOOT_MAGIC/version handshake uses BKP0R/BKP1R — see
 * OtaStore_Config.h).
 *
 * CubeMX-generated boilerplate (SystemClock_Config, MX_GPIO_Init,
 * Error_Handler) is kept in this file rather than Core/Src/main.c —
 * Core/Src/main.c does not exist in this project; this is the sole main().
 */

#include <string.h>

#include "main.h"
#include "hal_bsp.h"
#include "hal_spi.h"
#include "hal_wdt.h"
#include "hal_rtc.h"
#include "Flash.h"
#include "OtaStore_Config.h"

#define BOOTLOADER_VERSION   1U

#define INTERNAL_FLASH_PAGE_SIZE   2048UL

typedef void (*pFunction)(void);

static uint8_t au8PageBuf[INTERNAL_FLASH_PAGE_SIZE];

/* --------------------------------------------------------------------------
 * SystemClock_Config — HSI + LSE, PLL -> 48 MHz. Same tree CubeMX generated
 * for this .ioc (identical to frtag2's own clock config).
 * -------------------------------------------------------------------------- */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType      = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSE;
    RCC_OscInitStruct.LSEState            = RCC_LSE_ON;
    RCC_OscInitStruct.HSIState            = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState        = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM            = RCC_PLLM_DIV1;
    RCC_OscInitStruct.PLL.PLLN            = 12;
    RCC_OscInitStruct.PLL.PLLP            = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLR            = RCC_PLLR_DIV4;
    RCC_OscInitStruct.PLL.PLLQ            = RCC_PLLQ_DIV2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        Error_Handler();

    RCC_ClkInitStruct.ClockType      = RCC_CLOCKTYPE_HCLK3 | RCC_CLOCKTYPE_HCLK
                                     | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1
                                     | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider  = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.AHBCLK3Divider = RCC_SYSCLK_DIV1;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
        Error_Handler();
}

static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Pin   = LED_RED_Pin | LED_YELLOW_Pin;
    HAL_GPIO_Init(GPIOB, &gpio);
    HAL_GPIO_WritePin(GPIOB, LED_RED_Pin | LED_YELLOW_Pin, GPIO_PIN_RESET);
}

static void LED_vFlash(uint16_t u16Pin, uint32_t u32Ms)
{
    HAL_GPIO_WritePin(GPIOB, u16Pin, GPIO_PIN_SET);
    HAL_Delay(u32Ms);
    HAL_GPIO_WritePin(GPIOB, u16Pin, GPIO_PIN_RESET);
}

void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

/* --------------------------------------------------------------------------
 * OTA metadata read/verify — direct external-flash reads at the offsets
 * defined in OtaStore_Config.h. No dependency on frtag2's OtaStore.c
 * abstraction; the bootloader only ever reads this record, it never writes
 * the VALID marker (only the application does) and never erases scratch.
 * -------------------------------------------------------------------------- */
typedef struct
{
    uint32_t u32Version;
    uint32_t u32StopAddr;
    uint32_t u32SizeBytes;
    uint8_t  u8Xor8;
    bool     bValid;
    bool     bConsumed;
} OtaMeta_t;

static uint32_t GetU32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool OTA_bGetMeta(OtaMeta_t *pt)
{
    uint8_t au8Rec[OTA_META_RECORD_LEN];
    memset(au8Rec, 0, sizeof(au8Rec));

    if (!FLASH_vRead(OTA_META_ADDR, au8Rec, sizeof(au8Rec)))
        return false;

    if (GetU32(&au8Rec[OTA_META_OFF_MAGIC]) != OTA_META_MAGIC)
        return false;

    pt->u32Version   = GetU32(&au8Rec[OTA_META_OFF_VERSION]);
    pt->u32StopAddr  = GetU32(&au8Rec[OTA_META_OFF_STOP_ADDR]);
    pt->u32SizeBytes = GetU32(&au8Rec[OTA_META_OFF_SIZE]);
    pt->u8Xor8       = au8Rec[OTA_META_OFF_XOR8];
    pt->bValid       = (au8Rec[OTA_META_OFF_VALID]    == OTA_META_MARKER);
    pt->bConsumed    = (au8Rec[OTA_META_OFF_CONSUMED] == OTA_META_MARKER);

    if (pt->u32SizeBytes == 0UL || pt->u32SizeBytes > OTA_APP_MAX_SIZE)
        return false;

    return pt->bValid;
}

static uint8_t OTA_u8CalcXor(uint32_t u32SizeBytes)
{
    uint8_t  au8Buf[64];
    uint8_t  u8Xor    = 0U;
    uint32_t u32Addr  = OTA_SCRATCH_START_ADDR;
    uint32_t u32Remain = u32SizeBytes;

    while (u32Remain > 0U)
    {
        uint16_t u16Chunk = (u32Remain > sizeof(au8Buf)) ? (uint16_t)sizeof(au8Buf) : (uint16_t)u32Remain;
        if (!FLASH_vRead(u32Addr, au8Buf, u16Chunk))
            return (uint8_t)~u8Xor;   /* force a mismatch on read failure */
        for (uint16_t i = 0; i < u16Chunk; i++)
            u8Xor ^= au8Buf[i];
        u32Addr   += u16Chunk;
        u32Remain -= u16Chunk;
        HAL_WDT_vReset();
    }
    return u8Xor;
}

static bool OTA_bMarkConsumed(void)
{
    uint8_t u8Marker = OTA_META_MARKER;
    return FLASH_vPageWrite(OTA_META_ADDR + OTA_META_OFF_CONSUMED, &u8Marker, 1U);
}

/* --------------------------------------------------------------------------
 * Internal flash programming — erase 2 KB page(s), program 8 bytes at a
 * time (STM32WL double-word program), from the scratchpad image.
 * -------------------------------------------------------------------------- */
static bool BL_bProgramApp(const OtaMeta_t *pt)
{
    uint32_t u32DstAddr = OTA_APP_BASE_ADDR;
    uint32_t u32SrcAddr = OTA_SCRATCH_START_ADDR;
    uint32_t u32Remain  = pt->u32SizeBytes;

    if (HAL_FLASH_Unlock() != HAL_OK)
        return false;

    bool bOk = true;

    while (u32Remain > 0U && bOk)
    {
        uint16_t u16Chunk = (u32Remain > INTERNAL_FLASH_PAGE_SIZE)
                           ? (uint16_t)INTERNAL_FLASH_PAGE_SIZE
                           : (uint16_t)u32Remain;

        memset(au8PageBuf, 0xFF, sizeof(au8PageBuf));

        if (!FLASH_vRead(u32SrcAddr, au8PageBuf, u16Chunk))
        {
            bOk = false;
            break;
        }

        FLASH_EraseInitTypeDef erase = {0};
        uint32_t u32PageError = 0U;
        erase.TypeErase = FLASH_TYPEERASE_PAGES;
        erase.Page      = (u32DstAddr - FLASH_BASE) / INTERNAL_FLASH_PAGE_SIZE;
        erase.NbPages   = 1U;
        if (HAL_FLASHEx_Erase(&erase, &u32PageError) != HAL_OK)
        {
            bOk = false;
            break;
        }

        for (uint32_t off = 0; off < INTERNAL_FLASH_PAGE_SIZE; off += 8U)
        {
            uint64_t u64Data;
            memcpy(&u64Data, &au8PageBuf[off], 8U);
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, u32DstAddr + off, u64Data) != HAL_OK)
            {
                bOk = false;
                break;
            }
        }

        u32DstAddr += INTERNAL_FLASH_PAGE_SIZE;
        u32SrcAddr += u16Chunk;
        u32Remain  -= u16Chunk;
        HAL_WDT_vReset();
    }

    HAL_FLASH_Lock();
    return bOk;
}

/* --------------------------------------------------------------------------
 * Jump to the application at OTA_APP_BASE_ADDR.
 * -------------------------------------------------------------------------- */
static void BL_vJumpToApp(void)
{
    uint32_t *app = (uint32_t *)OTA_APP_BASE_ADDR;
    uint32_t app_estack       = app[0];
    uint32_t app_ResetHandler = app[1];

    HAL_RCC_DeInit();
    HAL_DeInit();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL  = 0U;

    __set_MSP(app_estack);
    SCB->VTOR = OTA_APP_BASE_ADDR;

    pFunction JumpToApp = (pFunction)app_ResetHandler;
    JumpToApp();
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    HAL_WDT_vInit();
    HAL_RTC_vInitBackupDomain();

    /* Record the bootloader's own version on every boot (own register —
     * distinct from BKP0R/BKP1R, which are the app<->bootloader OTA
     * handoff flag/version per OtaStore_Config.h). */
    TAMP->BKP2R = BOOTLOADER_VERSION;

    HAL_SPI_vInit();
    FLASH_vInit();

    OtaMeta_t meta;
    if (OTA_bGetMeta(&meta) && !meta.bConsumed)
    {
        uint8_t u8XorCalc = OTA_u8CalcXor(meta.u32SizeBytes);
        if (u8XorCalc == meta.u8Xor8)
        {
            LED_vFlash(LED_RED_Pin, 50);
            if (BL_bProgramApp(&meta))
            {
                (void)OTA_bMarkConsumed();
            }
            /* On a programming failure, fall through and jump to whatever
             * is (possibly partially) in internal flash — nothing safer
             * to do without a golden/second bank to fall back to. */
        }
        else
        {
            LED_vFlash(LED_YELLOW_Pin, 50);   /* image present but XOR mismatch — skip */
        }
    }

    /* Clear the app<->bootloader handoff flag either way. */
    TAMP->BKP0R = 0U;

    BL_vJumpToApp();

    while (1) { }
}
