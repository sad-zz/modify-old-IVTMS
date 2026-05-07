/**
 * @file diskio.c
 * @brief FatFS disk-io glue layer
 *
 * این فایل توابعی را که FatFS (نوشته‌ی ChaN) برای دسترسی به سخت‌افزار
 * نیاز دارد به درایور پایین‌سطح کارت SD ما (sd_spi.c) متصل می‌کند.
 *
 * فقط زمانی کامپایل می‌شود که در config.h:
 *     #define SD_USE_FATFS  1
 * تنظیم شده باشد و فایل‌های ff.c / ff.h / ffconf.h در درخت
 * پروژه قرار گرفته باشند (طبق README این پوشه).
 *
 * نکته: FatFS برای STM32 درایو با شماره ۰ (`"0:"`) را به SD کارت اختصاص
 * می‌دهد. اگر در آینده درایو دیگری اضافه شود، توابع زیر باید روی pdrv
 * dispatch کنند.
 */

#include "../../Core/Inc/sd_spi.h"

/* FatFS headers – دانلود جداگانه. اگر هنوز در درخت پروژه نیستند،
 * این فایل کامپایل نمی‌شود (به دلیل شکست include). */
#if defined(SD_USE_FATFS) && (SD_USE_FATFS == 1)

#include "ff.h"
#include "diskio.h"

#define DRIVE_SDCARD  0

/* ─── disk_status ─────────────────────────────────────────────────── */
DSTATUS disk_status(BYTE pdrv)
{
    if (pdrv != DRIVE_SDCARD) return STA_NOINIT;
    return (sd_get_type() == 0) ? STA_NOINIT : 0;
}

/* ─── disk_initialize ─────────────────────────────────────────────── */
DSTATUS disk_initialize(BYTE pdrv)
{
    if (pdrv != DRIVE_SDCARD) return STA_NOINIT;
    if (sd_init() != 0) return STA_NOINIT;
    return 0;
}

/* ─── disk_read ───────────────────────────────────────────────────── */
DRESULT disk_read(BYTE pdrv, BYTE *buf, LBA_t sector, UINT count)
{
    if (pdrv != DRIVE_SDCARD)        return RES_PARERR;
    if (sd_get_type() == 0)          return RES_NOTRDY;
    if (sd_read(sector, buf, count)) return RES_ERROR;
    return RES_OK;
}

/* ─── disk_write ──────────────────────────────────────────────────── */
#if FF_FS_READONLY == 0
DRESULT disk_write(BYTE pdrv, const BYTE *buf, LBA_t sector, UINT count)
{
    if (pdrv != DRIVE_SDCARD)         return RES_PARERR;
    if (sd_get_type() == 0)           return RES_NOTRDY;
    if (sd_write(sector, buf, count)) return RES_ERROR;
    return RES_OK;
}
#endif

/* ─── disk_ioctl ──────────────────────────────────────────────────── */
DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    if (pdrv != DRIVE_SDCARD) return RES_PARERR;
    if (sd_get_type() == 0)   return RES_NOTRDY;

    switch (cmd) {
    case CTRL_SYNC:
        return (sd_sync() == 0) ? RES_OK : RES_ERROR;

    case GET_SECTOR_COUNT:
        *(LBA_t *)buff = sd_get_sector_count();
        return RES_OK;

    case GET_SECTOR_SIZE:
        *(WORD *)buff = 512;
        return RES_OK;

    case GET_BLOCK_SIZE:
        /* Smallest erase block in 512-byte sectors. SD cards usually
         * report this in CSD; we use 1 (unknown) so FatFS treats every
         * sector as independently writable. */
        *(DWORD *)buff = 1;
        return RES_OK;

    default:
        return RES_PARERR;
    }
}

/* ─── get_fattime ─────────────────────────────────────────────────── */
/* FatFS calls this to timestamp files. Map to the software RTC kept in
 * variables.h so files on the SD card show the correct creation time. */
#include "../../Core/Inc/variables.h"

DWORD get_fattime(void)
{
    /* FAT timestamp format:
     *   bit 31..25  Year from 1980 (0..127)
     *   bit 24..21  Month  (1..12)
     *   bit 20..16  Day    (1..31)
     *   bit 15..11  Hour   (0..23)
     *   bit 10..5   Minute (0..59)
     *   bit  4..0   Second / 2 (0..29)  */
    uint16_t y = 2000 + current_time.year - 1980;
    return ((DWORD)y                      << 25) |
           ((DWORD)current_time.month     << 21) |
           ((DWORD)current_time.day       << 16) |
           ((DWORD)current_time.hour      << 11) |
           ((DWORD)current_time.minute    <<  5) |
           ((DWORD)(current_time.second / 2));
}

#endif /* SD_USE_FATFS */
