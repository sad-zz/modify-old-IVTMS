/**
 * @file w25q80.c
 * @brief W25Q80 8Mbit SPI NOR Flash – full driver implementation
 *
 * از SPI1 (PA4-PA7) استفاده می‌کند که قبلاً به W5500 اختصاص داشت.
 * حالا این پین‌ها به W25Q80 اختصاص دارند.
 */

#include "w25q80.h"
#include "stm32f1xx_hal.h"
#include <string.h>

/* ─── Extern HAL handle (defined in main.c) ──────────────────────────── */
extern SPI_HandleTypeDef hspi1;

/* ─── GPIO ──────────────────────────────────────────────────────────── */
#define FLASH_CS_PORT  GPIOA
#define FLASH_CS_PIN   GPIO_PIN_4

/* ─── W25Q80 command bytes ───────────────────────────────────────────── */
#define CMD_WREN     0x06U   /* Write Enable                              */
#define CMD_WRDI     0x04U   /* Write Disable                             */
#define CMD_RDSR1    0x05U   /* Read Status Register-1                    */
#define CMD_READ     0x03U   /* Read Data                                 */
#define CMD_PP       0x02U   /* Page Program (256-byte page)              */
#define CMD_SE       0x20U   /* Sector Erase (4KB)                        */
#define CMD_JEDECID  0x9FU   /* Read JEDEC ID                             */
#define CMD_RELPD    0xABU   /* Release Power-down / Read Device ID       */
#define STAT_BUSY    0x01U   /* Status bit 0: Write-In-Progress           */

/* Expected JEDEC ID for W25Q80DV: 0xEF4014 */
#define W25Q80_JEDEC_MFR  0xEFU
#define W25Q80_JEDEC_TYPE 0x40U
#define W25Q80_JEDEC_CAP  0x14U

/* ─── SPI transaction helpers ───────────────────────────────────────── */
static inline void cs_low(void)
{
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_RESET);
}
static inline void cs_high(void)
{
    HAL_GPIO_WritePin(FLASH_CS_PORT, FLASH_CS_PIN, GPIO_PIN_SET);
}

static uint8_t spi_xfer(uint8_t byte)
{
    uint8_t rx = 0;
    HAL_SPI_TransmitReceive(&hspi1, &byte, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

static void spi_write_buf(const uint8_t *buf, uint16_t len)
{
    HAL_SPI_Transmit(&hspi1, (uint8_t *)buf, len, HAL_MAX_DELAY);
}

static void spi_read_buf(uint8_t *buf, uint16_t len)
{
    memset(buf, 0xFF, len);
    HAL_SPI_Receive(&hspi1, buf, len, HAL_MAX_DELAY);
}

/* ─── w25q80_read_status ─────────────────────────────────────────────── */
uint8_t w25q80_read_status(void)
{
    cs_low();
    spi_xfer(CMD_RDSR1);
    uint8_t s = spi_xfer(0xFF);
    cs_high();
    return s;
}

/* ─── w25q80_wait_busy ───────────────────────────────────────────────── */
void w25q80_wait_busy(void)
{
    uint32_t start = HAL_GetTick();
    while (w25q80_read_status() & STAT_BUSY) {
        if ((HAL_GetTick() - start) > 5000) break;  /* safety timeout 5s */
        HAL_Delay(1);
    }
}

/* ─── Write Enable ───────────────────────────────────────────────────── */
static void write_enable(void)
{
    cs_low();
    spi_xfer(CMD_WREN);
    cs_high();
}

/* ─── w25q80_init ────────────────────────────────────────────────────── */
int w25q80_init(void)
{
    /* CS idle high */
    cs_high();

    /* Release power-down */
    cs_low();
    spi_xfer(CMD_RELPD);
    cs_high();
    HAL_Delay(3);   /* tRES1 = 3µs, use 3ms to be safe */

    /* Read JEDEC ID */
    cs_low();
    spi_xfer(CMD_JEDECID);
    uint8_t mfr  = spi_xfer(0xFF);
    uint8_t type = spi_xfer(0xFF);
    uint8_t cap  = spi_xfer(0xFF);
    cs_high();

    if (mfr != W25Q80_JEDEC_MFR || type != W25Q80_JEDEC_TYPE || cap != W25Q80_JEDEC_CAP)
        return -1;   /* chip not found or wrong variant */

    return 0;
}

/* ─── w25q80_read ────────────────────────────────────────────────────── */
void w25q80_read(uint32_t addr, uint8_t *buf, uint16_t len)
{
    uint8_t cmd[4];
    cmd[0] = CMD_READ;
    cmd[1] = (uint8_t)((addr >> 16) & 0xFF);
    cmd[2] = (uint8_t)((addr >>  8) & 0xFF);
    cmd[3] = (uint8_t)( addr        & 0xFF);

    cs_low();
    spi_write_buf(cmd, 4);
    spi_read_buf(buf, len);
    cs_high();
}

/* ─── w25q80_page_program ────────────────────────────────────────────── */
int w25q80_page_program(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    if (len == 0 || len > 256) return -1;

    write_enable();
    w25q80_wait_busy();

    uint8_t cmd[4];
    cmd[0] = CMD_PP;
    cmd[1] = (uint8_t)((addr >> 16) & 0xFF);
    cmd[2] = (uint8_t)((addr >>  8) & 0xFF);
    cmd[3] = (uint8_t)( addr        & 0xFF);

    cs_low();
    spi_write_buf(cmd, 4);
    spi_write_buf(buf, len);
    cs_high();

    w25q80_wait_busy();
    return 0;
}

/* ─── w25q80_sector_erase ────────────────────────────────────────────── */
int w25q80_sector_erase(uint32_t addr)
{
    write_enable();
    w25q80_wait_busy();

    addr &= ~0xFFFU;   /* align to 4KB sector boundary */

    uint8_t cmd[4];
    cmd[0] = CMD_SE;
    cmd[1] = (uint8_t)((addr >> 16) & 0xFF);
    cmd[2] = (uint8_t)((addr >>  8) & 0xFF);
    cmd[3] = (uint8_t)( addr        & 0xFF);

    cs_low();
    spi_write_buf(cmd, 4);
    cs_high();

    w25q80_wait_busy();   /* sector erase: typ. 60ms, max 400ms */
    return 0;
}

/* ─── Slot address helper ─────────────────────────────────────────────── */
/**
 * Convert a "YYMMDDHHmm" timestamp string to a flash sector address.
 * Slot index = (DD*24*12 + HH*12 + mm/5) % NUM_SLOTS
 * Address    = slot_index * SLOT_SIZE (4KB)
 */
static uint32_t ts_to_addr(const char *ts10)
{
    if (!ts10 || strlen(ts10) < 10) return 0;

    /* Parse: YY=ts[0-1], MM=ts[2-3], DD=ts[4-5], HH=ts[6-7], mm=ts[8-9] */
    uint32_t dd = (uint32_t)((ts10[4] - '0') * 10 + (ts10[5] - '0'));
    uint32_t hh = (uint32_t)((ts10[6] - '0') * 10 + (ts10[7] - '0'));
    uint32_t mm = (uint32_t)((ts10[8] - '0') * 10 + (ts10[9] - '0'));

    uint32_t idx = (dd * 24 * 12 + hh * 12 + mm / 5) % W25Q80_NUM_SLOTS;
    return idx * W25Q80_SLOT_SIZE;
}

/* ─── w25q80_save_interval ───────────────────────────────────────────── */
/**
 * Record layout in flash (275 bytes total):
 *   [0]       magic byte (0xA5) – marks valid record
 *   [1..10]   timestamp "YYMMDDHHmm" (ts10, 10 chars)
 *   [11..274] interval_data payload (W25Q80_DATA_SIZE = 264 bytes, incl. CRLF)
 *
 * Total = 1 + 10 + 264 = 275 bytes, fits in 2 pages (512 bytes).
 * The 4KB sector is erased first (sector erase is atomic at 4KB).
 */
void w25q80_save_interval(const uint8_t *data264, const char *ts10)
{
    if (!data264 || !ts10) return;

    uint32_t addr = ts_to_addr(ts10);

    /* Build the on-flash record (≤274 bytes → fits in 2 × 256-byte pages) */
    uint8_t record[280];
    memset(record, 0xFF, sizeof(record));
    record[0] = W25Q80_MAGIC;
    memcpy(record + 1, ts10, 10);
    memcpy(record + 11, data264, W25Q80_DATA_SIZE);

    /* Erase sector, then program in two 256-byte chunks */
    w25q80_sector_erase(addr);
    w25q80_page_program(addr,       record,       256);
    w25q80_page_program(addr + 256, record + 256, (uint16_t)(sizeof(record) - 256));
}

/* ─── w25q80_load_interval ───────────────────────────────────────────── */
int w25q80_load_interval(const char *ts10_req, uint8_t *out264)
{
    if (!ts10_req || !out264) return 0;

    uint32_t addr = ts_to_addr(ts10_req);

    uint8_t record[280];
    w25q80_read(addr, record, sizeof(record));

    /* Validate magic byte */
    if (record[0] != W25Q80_MAGIC) return 0;

    /* Validate timestamp match */
    if (memcmp(record + 1, ts10_req, 10) != 0) return 0;

    memcpy(out264, record + 11, W25Q80_DATA_SIZE);
    return 1;
}
