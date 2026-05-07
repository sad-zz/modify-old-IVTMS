/**
 * @file storage.c
 * @brief Runtime auto-detect storage backend (W25Q80 + SD card)
 *
 * در زمان boot، این ماژول هر دو backend را امتحان می‌کند:
 *   - W25Q80 SPI NOR Flash روی SPI1
 *   - SD card روی SPI2 (در صورت موجود بودن)
 *
 * بعد از init هر کدام از backend ها که موفق بودند فعال باقی می‌مانند
 * و عملیات save/load روی backend فعال انجام می‌شوند. اگر هر دو فعال باشند:
 *   - save: روی هر دو نوشته می‌شود (redundancy)
 *   - load: ابتدا از SD سعی می‌کند، اگر نبود از W25Q80
 */

#include "storage.h"
#include "config.h"
#include "variables.h"
#include "w25q80.h"
#include "sd_spi.h"

#include <string.h>
#include <stdio.h>

/* ─── FatFS integration (optional) ───────────────────────────────────── */
#if defined(SD_USE_FATFS) && (SD_USE_FATFS == 1)
#  include "ff.h"
   static FATFS sd_fs;
   static int   sd_fatfs_mounted = 0;
#endif

/* ─── Module state ───────────────────────────────────────────────────── */
static uint8_t backends = STORAGE_BE_NONE;

/* ─── SD raw-block storage layout ────────────────────────────────────── *
 *
 * اگر FatFS فعال نباشد، رکوردهای بازه‌ای مستقیم به سکتورهای کارت SD
 * نوشته می‌شوند. ساختار راهبردی:
 *   - SD_RAW_BASE_LBA   : LBA شروع منطقه‌ی ذخیره (پیش‌فرض 1024 = پشت پارتیشن MBR)
 *   - SD_RAW_NUM_SLOTS  : تعداد اسلات
 *   - هر اسلات یک سکتور ۵۱۲ بایتی است (264 بایت داده + ۲۸۰ بایت پدینگ)
 *
 * ساختار سکتور (مشابه W25Q80):
 *   [0]       magic byte 0xA5
 *   [1..10]   timestamp "YYMMDDHHmm"
 *   [11..274] interval payload 264 bytes
 *   [275..511] padding 0xFF
 */
#ifndef SD_RAW_BASE_LBA
#  define SD_RAW_BASE_LBA   1024UL
#endif
#ifndef SD_RAW_NUM_SLOTS
#  define SD_RAW_NUM_SLOTS  2048UL    /* ~7 days @ 5min interval */
#endif
#define SD_RAW_MAGIC        0xA5U

/* ─── helpers ────────────────────────────────────────────────────────── */
static uint32_t ts_to_slot(const char *ts10, uint32_t num_slots)
{
    if (!ts10) return 0;
    /* Parse: YY[0-1] MM[2-3] DD[4-5] HH[6-7] mm[8-9] */
    uint32_t mo = (uint32_t)((ts10[2] - '0') * 10 + (ts10[3] - '0'));
    uint32_t dd = (uint32_t)((ts10[4] - '0') * 10 + (ts10[5] - '0'));
    uint32_t hh = (uint32_t)((ts10[6] - '0') * 10 + (ts10[7] - '0'));
    uint32_t mm = (uint32_t)((ts10[8] - '0') * 10 + (ts10[9] - '0'));
    /* index by month-day-hour-minute (cycles every ~31 days @ 5min) */
    uint32_t idx = (((mo * 31 + dd) * 24 + hh) * 12 + mm / 5);
    return idx % num_slots;
}

/* ─── SD raw save / load ─────────────────────────────────────────────── */
#if !defined(SD_USE_FATFS) || (SD_USE_FATFS == 0)
static int sd_save_interval_raw(const uint8_t *data264, const char *ts10)
{
    uint8_t  sect[512];
    uint32_t lba = SD_RAW_BASE_LBA + ts_to_slot(ts10, SD_RAW_NUM_SLOTS);

    memset(sect, 0xFF, sizeof(sect));
    sect[0] = SD_RAW_MAGIC;
    memcpy(sect + 1,  ts10,    10);
    memcpy(sect + 11, data264, 264);

    return sd_write(lba, sect, 1);
}

static int sd_load_interval_raw(const char *ts10_req, uint8_t *out264)
{
    uint8_t  sect[512];
    uint32_t lba = SD_RAW_BASE_LBA + ts_to_slot(ts10_req, SD_RAW_NUM_SLOTS);

    if (sd_read(lba, sect, 1) != 0) return 0;
    if (sect[0] != SD_RAW_MAGIC)    return 0;
    if (memcmp(sect + 1, ts10_req, 10) != 0) return 0;

    memcpy(out264, sect + 11, 264);
    return 1;
}
#endif /* !SD_USE_FATFS */

/* ─── SD FatFS save / load ───────────────────────────────────────────── */
#if defined(SD_USE_FATFS) && (SD_USE_FATFS == 1)
static int sd_save_interval_fatfs(const uint8_t *data264, const char *ts10)
{
    if (!sd_fatfs_mounted) return -1;

    /* Make sure the directory exists (ignore "already exists" error) */
    f_mkdir("0:/intervals");

    char path[40];
    snprintf(path, sizeof(path), "0:/intervals/%c%c%c%c%c%c%c%c%c%c.dat",
             ts10[0], ts10[1], ts10[2], ts10[3], ts10[4],
             ts10[5], ts10[6], ts10[7], ts10[8], ts10[9]);

    FIL  fp;
    UINT bw;
    if (f_open(&fp, path, FA_CREATE_ALWAYS | FA_WRITE) != FR_OK) return -1;
    FRESULT r = f_write(&fp, data264, 264, &bw);
    f_close(&fp);
    return (r == FR_OK && bw == 264) ? 0 : -1;
}

static int sd_load_interval_fatfs(const char *ts10_req, uint8_t *out264)
{
    if (!sd_fatfs_mounted) return 0;

    char path[40];
    snprintf(path, sizeof(path), "0:/intervals/%c%c%c%c%c%c%c%c%c%c.dat",
             ts10_req[0], ts10_req[1], ts10_req[2], ts10_req[3], ts10_req[4],
             ts10_req[5], ts10_req[6], ts10_req[7], ts10_req[8], ts10_req[9]);

    FIL  fp;
    UINT br;
    if (f_open(&fp, path, FA_READ) != FR_OK) return 0;
    FRESULT r = f_read(&fp, out264, 264, &br);
    f_close(&fp);
    return (r == FR_OK && br == 264) ? 1 : 0;
}
#endif /* SD_USE_FATFS */

/* ─── Public API ─────────────────────────────────────────────────────── */

uint8_t storage_init(void)
{
    backends = STORAGE_BE_NONE;

    /* ── W25Q80 ─────────────────────────────────────────────────────── */
    if (w25q80_init() == 0) {
        backends |= STORAGE_BE_W25Q80;
    }

    /* ── SD card ────────────────────────────────────────────────────── */
    if (sd_init() == 0) {
        /* Boost SPI2 to operating speed now that init handshake done */
        sd_spi_set_high_speed();
#       if defined(SD_USE_FATFS) && (SD_USE_FATFS == 1)
        if (f_mount(&sd_fs, "0:", 1) == FR_OK) {
            sd_fatfs_mounted = 1;
            backends |= STORAGE_BE_SDCARD;
        } else {
            sd_fatfs_mounted = 0;
            sd_deinit();
        }
#       else
        backends |= STORAGE_BE_SDCARD;
#       endif
    }

    /* If neither backend came up, set the MMC error bit so the device
     * still reports the failure to the server. */
    if (backends == STORAGE_BE_NONE) {
        set_error(MMC_ERR);
    } else {
        reset_error(MMC_ERR);
    }
    return backends;
}

uint8_t storage_available(void) { return backends; }

void storage_save_interval(const uint8_t *data264, const char *ts10)
{
    if (!data264 || !ts10) return;

    /* Write to every available backend for redundancy */
    if (backends & STORAGE_BE_W25Q80) {
        w25q80_save_interval(data264, ts10);
    }
    if (backends & STORAGE_BE_SDCARD) {
#       if defined(SD_USE_FATFS) && (SD_USE_FATFS == 1)
        sd_save_interval_fatfs(data264, ts10);
#       else
        sd_save_interval_raw(data264, ts10);
#       endif
    }
}

int storage_load_interval(const char *ts10_req, uint8_t *out264)
{
    if (!ts10_req || !out264) return 0;

    /* Prefer SD (larger capacity, longer history) */
    if (backends & STORAGE_BE_SDCARD) {
#       if defined(SD_USE_FATFS) && (SD_USE_FATFS == 1)
        if (sd_load_interval_fatfs(ts10_req, out264)) return 1;
#       else
        if (sd_load_interval_raw   (ts10_req, out264)) return 1;
#       endif
    }
    if (backends & STORAGE_BE_W25Q80) {
        if (w25q80_load_interval(ts10_req, out264)) return 1;
    }
    return 0;
}
