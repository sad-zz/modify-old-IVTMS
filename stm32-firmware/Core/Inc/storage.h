/**
 * @file storage.h
 * @brief Storage abstraction layer – W25Q80 SPI flash و/یا SD card
 *
 * این ماژول یک API یکپارچه برای ذخیره و بازیابی رکوردهای بازه‌ای
 * (interval data) ارائه می‌دهد. زیر این لایه از یکی یا هر دو backend
 * زیر استفاده می‌شود (runtime auto-detect):
 *
 *   1) W25Q80 SPI NOR Flash (SPI1 + PA4 /CS)
 *   2) SD/MMC card via SPI2 (PB12 /CS, PB13/14/15 SCK/MISO/MOSI)
 *
 * در زمان boot، storage_init() هر دو backend را امتحان می‌کند و هر کدام
 * که در دسترس بود را فعال می‌کند. اگر هر دو موجود باشند، SD به‌عنوان
 * backend اصلی انتخاب می‌شود (ظرفیت بزرگ‌تر، قابل خواندن روی PC) و
 * W25Q80 به‌عنوان پشتیبان عمل می‌کند.
 *
 * SD backend می‌تواند یا از FatFS (FAT32) و یا از سکتورهای raw استفاده
 * کند. انتخاب از طریق ماکروی SD_USE_FATFS در config.h انجام می‌شود:
 *   #define SD_USE_FATFS  1   // فایل‌های قابل خواندن روی PC (نیازمند FatFS library)
 *   #define SD_USE_FATFS  0   // raw block storage (پیش‌فرض، بدون وابستگی)
 */

#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

/* ─── Backend availability bitmask ──────────────────────────────────── */
#define STORAGE_BE_NONE     0x00U
#define STORAGE_BE_W25Q80   0x01U
#define STORAGE_BE_SDCARD   0x02U

/* ─── Public API ─────────────────────────────────────────────────────── */

/**
 * @brief Probe available backends and initialise them.
 *        Tries W25Q80 first (SPI1), then SD card (SPI2).
 *        After this call, storage_available() returns the bitmask of
 *        successfully initialised backends.
 * @return bitmask of available backends (STORAGE_BE_*).
 */
uint8_t storage_init(void);

/**
 * @brief Return bitmask of currently available backends.
 */
uint8_t storage_available(void);

/**
 * @brief Save an interval record to all available backends.
 *        If both W25Q80 and SD are present, writes to both for redundancy.
 *        Silently ignored if no backend is available (logged via error_byte).
 *
 * @param data264  264-byte interval payload (the 8821 body, including CRLF)
 * @param ts10     10-char timestamp string "YYMMDDHHmm" extracted from
 *                 interval_data[8..17]
 */
void storage_save_interval(const uint8_t *data264, const char *ts10);

/**
 * @brief Try to load an interval record matching the requested timestamp.
 *        Searches SD first (if available), then W25Q80.
 *
 * @param ts10_req 10-char timestamp string from a 0197 server request
 * @param out264   caller-supplied 264-byte buffer, filled on success
 * @return 1 if a valid matching record was found, 0 otherwise.
 */
int storage_load_interval(const char *ts10_req, uint8_t *out264);

#endif /* STORAGE_H */
