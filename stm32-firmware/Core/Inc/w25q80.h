/**
 * @file w25q80.h
 * @brief W25Q80 8Mbit SPI NOR Flash – driver header
 *
 * ماژول Flash برای ذخیره‌ی پایدار (non-volatile) داده‌های بازه‌ای RATCX1.
 * در موازات RAM دستگاه کار می‌کند و داده‌ها را هنگام قطع برق نگه می‌دارد.
 *
 * ─── مشخصات W25Q80DV ──────────────────────────────────────────────────
 *  ظرفیت   : 8 Mbit = 1 MB
 *  صفحه    : 256 بایت
 *  سکتور   : 4 KB  (16 صفحه)  – کوچک‌ترین واحد پاکسازی
 *  بلوک 32K: 32 KB (8 سکتور)
 *  بلوک 64K: 64 KB (16 سکتور)
 *  ولتاژ   : 2.7V – 3.6V
 *  SPI حداکثر: 80 MHz
 *
 * ─── اتصال سخت‌افزاری ─────────────────────────────────────────────────
 *  STM32F103  W25Q80
 *  PA4  → /CS  (chip select, active-low)
 *  PA5  → CLK  (SPI1_SCK)
 *  PA6  ← DO   (SPI1_MISO, pin 2 of W25Q80)
 *  PA7  → DI   (SPI1_MOSI, pin 5 of W25Q80)
 *  PB2  → /WP  (write protect – tie HIGH to disable, or GPIO for control)
 *  PB3  → /HOLD (tie HIGH normally)
 *  3.3V → VCC
 *  GND  → GND
 *
 * ─── طرح ذخیره داده‌های بازه‌ای ──────────────────────────────────────
 *  هر بازه ۵ دقیقه‌ای یک رشته ۲۶۴ بایتی تولید می‌کند (فرمت ۸۸۲۱).
 *  هر اسلات در Flash یک سکتور ۴KB را اشغال می‌کند (ساده‌ترین روش).
 *  - تعداد اسلات   : 128  (نیمه اول Flash = 512KB)
 *  - پوشش زمانی    : 128 × 5min = 10.7 ساعت
 *  - بقیه Flash    : 512KB – رزرو برای config آینده
 *
 *  آدرس اسلات i = i × 0x1000  (i = 0..127)
 *  شاخص اسلات از timestamp استخراج می‌شود:
 *    slot_index = (day*24*12 + hour*12 + minute/5) % NUM_SLOTS
 */

#ifndef W25Q80_H
#define W25Q80_H

#include <stdint.h>

/* ─── Storage layout ─────────────────────────────────────────────────── */
#define W25Q80_NUM_SLOTS     128      /* number of interval storage slots  */
#define W25Q80_SLOT_SIZE     0x1000U  /* 4KB per slot (one sector)         */
#define W25Q80_DATA_SIZE     264U     /* bytes per interval record         */
#define W25Q80_MAGIC         0xA5U    /* first byte of a valid record       */

/* ─── Low-level flash operations ─────────────────────────────────────── */

/**
 * @brief Initialise SPI1 (if not already done by main.c), verify JEDEC ID.
 * @return 0 on success (JEDEC = 0xEF4014 for W25Q80DV), non-zero on error.
 */
int  w25q80_init(void);

/**
 * @brief Read bytes from the flash address space.
 * @param addr   24-bit byte address (0x000000 … 0x0FFFFF for W25Q80)
 * @param buf    destination buffer
 * @param len    byte count
 */
void w25q80_read(uint32_t addr, uint8_t *buf, uint16_t len);

/**
 * @brief Program up to 256 bytes within one 256-byte page.
 *        Caller must erase the sector first (0xFF required for 0→1 bits).
 * @param addr   must be page-aligned if len==256; partial-page writes allowed
 * @param buf    source buffer
 * @param len    byte count (1..256)
 * @return 0 on success.
 */
int  w25q80_page_program(uint32_t addr, const uint8_t *buf, uint16_t len);

/**
 * @brief Erase one 4KB sector (sets all bytes to 0xFF).
 * @param addr   any address within the sector (lower 12 bits ignored)
 * @return 0 on success.
 */
int  w25q80_sector_erase(uint32_t addr);

/** Read status register 1. Bit 0 = BUSY. */
uint8_t w25q80_read_status(void);

/** Block until WIP (write-in-progress) bit clears. */
void w25q80_wait_busy(void);

/* ─── High-level interval storage ────────────────────────────────────── */

/**
 * @brief Save an interval record to the slot determined by its timestamp.
 * @param data264  264-byte interval payload (the 8821 body)
 * @param ts10     10-char timestamp string "YYMMDDHHmm" from interval_data[8..17]
 */
void w25q80_save_interval(const uint8_t *data264, const char *ts10);

/**
 * @brief Try to load an interval record matching the requested timestamp.
 * @param ts10_req 10-char timestamp string from 0197 request
 * @param out264   caller-supplied 264-byte buffer, filled on success
 * @return 1 if a valid matching record was found, 0 otherwise.
 */
int  w25q80_load_interval(const char *ts10_req, uint8_t *out264);

#endif /* W25Q80_H */
