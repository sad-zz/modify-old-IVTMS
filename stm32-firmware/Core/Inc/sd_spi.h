/**
 * @file sd_spi.h
 * @brief SD/MMC card driver in SPI mode – low-level block I/O
 *
 * درایور سطح پایین کارت SD/SDHC/MMC در حالت SPI. فقط دستورات لازم
 * برای راه‌اندازی، خواندن و نوشتن یک بلاک ۵۱۲ بایتی پیاده‌سازی شده‌اند.
 *
 * ─── اتصال سخت‌افزاری (SPI2 پیشنهادی) ─────────────────────────────────
 *  STM32F103   SD card (در soldered/socket)
 *  PB12 (GPIO) → /CS    (chip select – active low)
 *  PB13 (SPI2_SCK) → SCLK  (CLK)
 *  PB14 (SPI2_MISO) ← DO   (DAT0 – output from card)
 *  PB15 (SPI2_MOSI) → DI   (CMD – input to card)
 *  3.3V → VCC
 *  GND  → VSS1, VSS2
 *
 *  Pull-up پیشنهادی: 10kΩ روی DI، DO، /CS به VCC (طبق spec SDA).
 *
 * ─── ترتیب راه‌اندازی استاندارد SPI ─────────────────────────────────
 *   1) باس SPI روی ~400kHz تنظیم شود (سرعت پایین برای init)
 *   2) /CS=HIGH، حداقل ۸۰ کلاک ارسال برای wake-up
 *   3) CMD0 (GO_IDLE_STATE) → پاسخ 0x01 (idle)
 *   4) CMD8 (SEND_IF_COND, 0x1AA) → اگر SDv2 پاسخ معتبر، اگر SDv1 پاسخ illegal
 *   5) ACMD41 (با HCS=1 برای SDv2) – تکرار تا کارت آماده شود
 *   6) CMD58 (READ_OCR) → بیت CCS مشخص می‌کند SDHC (block addr) یا SDSC (byte addr)
 *   7) CMD16 (SET_BLOCKLEN=512) فقط برای SDSC
 *   8) سرعت SPI به سرعت بالا (تا ~25MHz) ارتقا داده شود
 */

#ifndef SD_SPI_H
#define SD_SPI_H

#include <stdint.h>

/* ─── Card type flags ───────────────────────────────────────────────── */
#define SD_TYPE_NONE     0x00U
#define SD_TYPE_MMC      0x01U   /* MMCv3                              */
#define SD_TYPE_SDV1     0x02U   /* SD v1 (byte address)               */
#define SD_TYPE_SDV2     0x04U   /* SD v2 (byte address)               */
#define SD_TYPE_BLOCK    0x08U   /* SDHC/SDXC – block (LBA) address    */

/* ─── Public API ─────────────────────────────────────────────────────── */

/**
 * @brief Initialise the SD card via SPI2.
 *        Performs full power-up + identification sequence.
 *        Caller must have already configured SPI2 in MX_SPI2_Init() and
 *        the /CS pin (PB12) as a push-pull output.
 * @return 0 on success, non-zero on error (no card / unsupported / timeout).
 */
int sd_init(void);

/**
 * @brief De-initialise (deselect, idle).
 */
void sd_deinit(void);

/**
 * @brief Return card type bitmask (SD_TYPE_*).
 */
uint8_t sd_get_type(void);

/**
 * @brief Total number of 512-byte sectors on the card (from CSD).
 *        Returns 0 if card is uninitialised.
 */
uint32_t sd_get_sector_count(void);

/**
 * @brief Read one or more 512-byte sectors.
 * @param lba    starting sector number (LBA)
 * @param buf    destination buffer (count * 512 bytes)
 * @param count  number of sectors to read
 * @return 0 on success, non-zero on error.
 */
int sd_read(uint32_t lba, uint8_t *buf, uint32_t count);

/**
 * @brief Write one or more 512-byte sectors.
 * @param lba    starting sector number (LBA)
 * @param buf    source buffer (count * 512 bytes)
 * @param count  number of sectors to write
 * @return 0 on success, non-zero on error.
 */
int sd_write(uint32_t lba, const uint8_t *buf, uint32_t count);

/**
 * @brief Sync (wait for write to complete) – called by FatFS sync.
 */
int sd_sync(void);

/**
 * @brief Re-init SPI2 to high-speed prescaler (after sd_init() succeeds).
 *        پیاده‌سازی در main.c (نیاز به دسترسی به hspi2 handle).
 */
void sd_spi_set_high_speed(void);

#endif /* SD_SPI_H */
