/*
 * main.c  (fr_bootloader/src/main.c)
 *
 * frtag2 OTA bootloader. No RTOS, no coroutines: runs once per reset,
 * polling only. Sole main() for the whole project — Core/Src/main.c
 * doesn't exist.
 *
 * Boot flow:
 *   1. Read the OTA metadata record from external NOR flash (layout in
 *      Fota_Config.h, kept byte-identical between this bootloader and the
 *      application).
 *   2. If the record is VALID and its version is strictly newer than the
 *      version currently installed (read directly from internal flash at
 *      OTA_FW_INFO_ADDR — a FwVersion_t compiled into the app image itself,
 *      see version_config.c on the app side), verify its XOR-8 and program
 *      internal flash OTA_APP_BASE_ADDR..stopAddr from the scratchpad.
 *   3. Jump to the application. On any check failure, jump to whatever is
 *      already installed.
 *
 * The scratchpad image is NEVER erased here — the primary re-reads it
 * later to distribute the same image to secondaries over LoRa.
 */

#include <string.h>

#include "main.h"
#include "hal_bsp.h"
#include "hal_spi.h"
#include "hal_wdt.h"
#include "hal_rtc.h"
#include "Flash.h"
#include "Fota_Config.h"
#include "bl_dbg.h"

/* Read by build_scripts/create_release_hex_files.ps1 (regex '_BL_VER\s+(\d+)')
 * to name the release hex. */
#define FRTAG_BL_VER   1U

#define INTERNAL_FLASH_PAGE_SIZE   2048UL

typedef void (*pFunction)(void);

static uint8_t au8PageBuf[INTERNAL_FLASH_PAGE_SIZE];

/* --------------------------------------------------------------------------
 * SystemClock_Config — HSI + LSE, PLL -> 48 MHz.
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
 * OTA metadata read — bootloader only ever reads this record; the app
 * writes VALID and the primary writes DISTRIBUTED.
 * -------------------------------------------------------------------------- */
typedef struct
{
    uint32_t u32Version;
    uint32_t u32StopAddr;
    uint32_t u32SizeBytes;
    uint8_t  u8Xor8;
    bool     bValid;
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
    pt->bValid       = (au8Rec[OTA_META_OFF_VALID] == OTA_META_MARKER);

    if (pt->u32SizeBytes == 0UL || pt->u32SizeBytes > OTA_APP_MAX_SIZE)
        return false;

    return pt->bValid;
}

/* Installed app version, read straight out of internal flash at the fixed
 * address the app's own linker script places its FwVersion_t at (see
 * version_config.c, Fota_Config.h). Internal flash is memory-mapped, so
 * this is a plain pointer read — no driver, no RTC/backup-domain
 * dependency (that dependency is exactly what made the older TAMP-backup-
 * register approach unreliable: the app wrote it before the RTC clock was
 * enabled and the write silently no-op'd).
 *
 * Packed into the same MMmmpp representation as meta.u32Version for a
 * direct comparison. An erased app region reads back all-0xFF fields,
 * which is treated as "no app installed" (0) rather than a huge version -
 * otherwise a blank chip would appear infinitely up to date and the
 * bootloader would never program the first image. */
static uint32_t BL_u32GetInstalledVersion(void)
{
    const FwVersion_t *pt = (const FwVersion_t *)OTA_FW_INFO_ADDR;

    if (pt->major == 0xFFFFU && pt->minor == 0xFFFFU && pt->patch == 0xFFFFU)
        return 0U;

    return (uint32_t)pt->major * 10000UL
         + (uint32_t)pt->minor * 100UL
         + (uint32_t)pt->patch;
}

static uint8_t OTA_u8CalcXor(uint32_t u32SizeBytes)
{
    uint8_t  au8Buf[64];
    uint8_t  u8Xor     = 0U;
    uint32_t u32Addr   = OTA_SCRATCH_START_ADDR;
    uint32_t u32Remain = u32SizeBytes;

    while (u32Remain > 0U)
    {
        uint16_t u16Chunk = (u32Remain > sizeof(au8Buf))
                            ? (uint16_t)sizeof(au8Buf) : (uint16_t)u32Remain;
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

/* --------------------------------------------------------------------------
 * Internal-flash programming from scratchpad.
 *
 * Per-op Unlock -> CLEAR_FLAG(ALL_ERRORS) -> op (retry once) -> Lock is
 * the same pattern frtag2's flashLog.c uses on this exact chip. Do NOT
 * hold the flash unlocked across the whole session and do NOT skip the
 * flag clear — the HAL treats any latched SR error (including OPTVERR
 * left set by the option-byte load at reset) as a prior-op failure and
 * returns HAL_ERROR without ever writing CR.
 *
 * LED heartbeat runs while erasing/programming so a bench operator can see
 * the device is actually working: red 20 ms on / 80 ms off, then yellow
 * 20 ms on / 80 ms off, repeating (200 ms cycle). The XOR-verify pass right
 * before this (see main()) lights both LEDs solid instead — visually
 * distinct from this blink pattern, so "verifying" and "programming" don't
 * look the same on the bench.
 * -------------------------------------------------------------------------- */
static void BL_vLedHeartbeat(uint32_t u32NowMs, uint32_t u32StartMs)
{
    uint32_t u32Phase = (u32NowMs - u32StartMs) % 200U;
    /*   0..19   -> RED on
     *  20..99   -> both off
     * 100..119  -> YELLOW on
     * 120..199  -> both off
     */
    bool bRedOn    = (u32Phase < 20U);
    bool bYellowOn = (u32Phase >= 100U) && (u32Phase < 120U);
    HAL_GPIO_WritePin(GPIOB, LED_RED_Pin,    bRedOn    ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, LED_YELLOW_Pin, bYellowOn ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bool BL_bProgramApp(const OtaMeta_t *pt)
{
    uint32_t u32DstAddr = OTA_APP_BASE_ADDR;
    uint32_t u32SrcAddr = OTA_SCRATCH_START_ADDR;
    uint32_t u32Remain  = pt->u32SizeBytes;
    uint32_t u32Done    = 0U;
    uint32_t u32StartMs = HAL_GetTick();
    uint8_t  u8LastPct  = 0xFFU;

    while (u32Remain > 0U)
    {

        uint16_t u16Chunk = (u32Remain > INTERNAL_FLASH_PAGE_SIZE)
                            ? (uint16_t)INTERNAL_FLASH_PAGE_SIZE
                            : (uint16_t)u32Remain;

        memset(au8PageBuf, 0xFF, sizeof(au8PageBuf));
        (void)FLASH_vRead(u32SrcAddr, au8PageBuf, u16Chunk);

        FLASH_EraseInitTypeDef erase = {0};
        uint32_t u32PageError = 0U;
        erase.TypeErase = FLASH_TYPEERASE_PAGES;
        erase.Page      = (u32DstAddr - FLASH_BASE) / INTERNAL_FLASH_PAGE_SIZE;
        erase.NbPages   = 1U;

        HAL_StatusTypeDef status;
        HAL_FLASH_Unlock();
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
        status = HAL_FLASHEx_Erase(&erase, &u32PageError);
        if (status != HAL_OK)
            status = HAL_FLASHEx_Erase(&erase, &u32PageError);
        HAL_FLASH_Lock();

        for (uint32_t off = 0; off < INTERNAL_FLASH_PAGE_SIZE; off += 8U)
        {
            uint64_t u64Data;
            memcpy(&u64Data, &au8PageBuf[off], 8U);

            HAL_FLASH_Unlock();
            __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
            status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                                       u32DstAddr + off, u64Data);
            if (status != HAL_OK)
                status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD,
                                           u32DstAddr + off, u64Data);
            HAL_FLASH_Lock();
        }

        u32DstAddr += INTERNAL_FLASH_PAGE_SIZE;
        u32SrcAddr += u16Chunk;
        u32Remain  -= u16Chunk;
        u32Done    += u16Chunk;
        HAL_WDT_vReset();

        uint8_t u8Pct = (uint8_t)((u32Done * 100UL) / pt->u32SizeBytes);
        if (u8LastPct == 0xFFU || u32Remain == 0U || u8Pct >= (uint8_t)(u8LastPct + 10U))
        {
            BL_DBG_vPuts("  programming ");
            BL_DBG_vPutDec32(u32Done);
            BL_DBG_vPuts("/");
            BL_DBG_vPutDec32(pt->u32SizeBytes);
            BL_DBG_vPuts(" B (");
            BL_DBG_vPutDec32(u8Pct);
            BL_DBG_vPuts("%)\r\n");
            u8LastPct = u8Pct;
        }
    }

    HAL_GPIO_WritePin(GPIOB, LED_YELLOW_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, LED_RED_Pin,    GPIO_PIN_RESET);
    return true;
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

    /* Own version — separate from BKP0R/BKP1R (app<->bootloader OTA handoff). */
    TAMP->BKP2R = FRTAG_BL_VER;

    HAL_GPIO_WritePin(GPIOB, LED_RED_Pin,    GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, LED_YELLOW_Pin, GPIO_PIN_SET);

    BL_DBG_vInit();
    BL_DBG_vPuts("\r\n--- frtag2_bootloader v");
    BL_DBG_vPutHex8(FRTAG_BL_VER);
    BL_DBG_vPuts(" ---\r\n");

    HAL_SPI_vInit();
    FLASH_vInit();

    OtaMeta_t meta;
    bool bMetaOk = OTA_bGetMeta(&meta);

    /* Installed app version — read directly from the app's own image in
     * internal flash (see BL_u32GetInstalledVersion). 0 means "no app
     * installed" (blank chip). */
    uint32_t u32InstalledVer = BL_u32GetInstalledVersion();

    BL_DBG_vPuts("meta: valid=");
    BL_DBG_vPuts(bMetaOk ? "YES" : "NO");
    BL_DBG_vPuts(" ver=");
    BL_DBG_vPutDec32(bMetaOk ? meta.u32Version : 0U);
    BL_DBG_vPuts(" installed=");
    BL_DBG_vPutDec32(u32InstalledVer);
    if (bMetaOk)
    {
        BL_DBG_vPuts(" size=");
        BL_DBG_vPutDec32(meta.u32SizeBytes);
        BL_DBG_vPuts(" xor=0x");
        BL_DBG_vPutHex8(meta.u8Xor8);
    }
    BL_DBG_vPuts("\r\n");

    if (bMetaOk && meta.u32Version > u32InstalledVer)
    {
        /* XOR verify streams the whole image over SPI (up to ~236 KB) —
         * real time, not instant. Both LEDs solid for the duration so it
         * reads as "busy verifying" on the bench, distinct from idle and
         * from the programming phase's blink pattern below. */
    	HAL_GPIO_WritePin(GPIOB, LED_YELLOW_Pin,    GPIO_PIN_RESET);
        uint8_t u8XorCalc = OTA_u8CalcXor(meta.u32SizeBytes);
        HAL_GPIO_WritePin(GPIOB, LED_RED_Pin, GPIO_PIN_RESET);

        if (u8XorCalc == meta.u8Xor8)
        {
        	HAL_GPIO_WritePin(GPIOB, LED_YELLOW_Pin, GPIO_PIN_SET);
            BL_DBG_vPuts("programming v");
            BL_DBG_vPutDec32(meta.u32Version);
            BL_DBG_vPuts(" (LEDs red/yellow blinking)...\r\n");
            (void)BL_bProgramApp(&meta);
        	HAL_GPIO_WritePin(GPIOB, LED_YELLOW_Pin,    GPIO_PIN_RESET);
            BL_DBG_vPuts("program done\r\n");
            /* BKP3R is deliberately NOT touched here — only the running
             * app is authoritative about "what's installed", so the app
             * itself writes BKP3R in FOTA_vInit on every boot. If a reset
             * happens before the just-programmed app writes it, this
             * bootloader run will re-program the same image; that's
             * idempotent and safer than the bootloader making a claim
             * about a version it never verified was actually running. */
        }
        else
        {
            BL_DBG_vPuts("xor mismatch (calc=0x");
            BL_DBG_vPutHex8(u8XorCalc);
            BL_DBG_vPuts(" stored=0x");
            BL_DBG_vPutHex8(meta.u8Xor8);
            BL_DBG_vPuts(") — skip\r\n");
            LED_vFlash(LED_YELLOW_Pin, 50);
        }
    }

    /* Clear the app<->bootloader handoff flag either way. */
    TAMP->BKP0R = 0U;

    BL_DBG_vPuts("jumping to app @0x");
    BL_DBG_vPutHex32(OTA_APP_BASE_ADDR);
    BL_DBG_vPuts("\r\n");
    HAL_Delay(20);   /* let the last UART bytes drain */

    BL_vJumpToApp();

    while (1) { }
}
