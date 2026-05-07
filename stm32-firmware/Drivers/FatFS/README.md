# FatFS Integration – راهنمای فعال‌سازی FAT32 روی SD کارت

## مرور سریع

این پوشه شامل فقط فایل **diskio.c** (لایه‌ی gluse بین FatFS و درایور SD ما) است.
هسته‌ی FatFS (نوشته‌ی ChaN) به دلیل اندازه‌اش (~۲۰۰KB با LFN) و license، **به‌صورت پیش‌فرض همراه پروژه نیست** و باید کاربر دانلود کند.

اگر FatFS نصب نشود، پروژه همچنان کامپایل می‌شود و SD به‌صورت **raw block** استفاده می‌شود (ساختار توضیح داده شده در `Core/Src/storage.c`). برای داشتن فایل‌های قابل خواندن روی PC (FAT32) مراحل زیر را دنبال کنید.

---

## مرحله ۱ – دانلود FatFS

از سایت رسمی ChaN دانلود کنید (نسخه پیشنهادی R0.15a یا بالاتر):

> http://elm-chan.org/fsw/ff/00index_e.html

فایل‌های زیر را از پکیج به مسیر `stm32-firmware/Drivers/FatFS/` کپی کنید:

| فایل | توضیح |
|------|-------|
| `ff.c`         | هسته‌ی FatFS |
| `ff.h`         | header اصلی FatFS |
| `ffconf.h`     | پیکربندی (پایین این فایل را با تنظیمات پیشنهادی جایگزین کنید) |
| `ffsystem.c`   | توابع heap/mutex (می‌توانید stub خالی استفاده کنید) |
| `ffunicode.c`  | جدول Unicode (فقط اگر FF_USE_LFN > 0) |
| `diskio.h`     | header برای glue (همراه FatFS) |

`diskio.c` در همین پوشه است و **نباید جایگزین شود** – این پیاده‌سازی ما برای SPI SD است.

---

## مرحله ۲ – تنظیم ffconf.h

تنظیمات پیشنهادی برای این پروژه (RAM کم، فقط ASCII، FAT32):

```c
#define FFCONF_DEF      80286
#define FF_FS_READONLY  0
#define FF_FS_MINIMIZE  0      /* همه‌ی APIs لازم - f_open, f_write, f_read, f_close */
#define FF_USE_FIND     0
#define FF_USE_MKFS     0      /* mkfs لازم نیست (کارت روی PC فرمت می‌شود) */
#define FF_USE_FASTSEEK 0
#define FF_USE_EXPAND   0
#define FF_USE_CHMOD    0
#define FF_USE_LABEL    0
#define FF_USE_FORWARD  0
#define FF_USE_STRFUNC  0
#define FF_PRINT_LLI    0
#define FF_PRINT_FLOAT  0
#define FF_STRF_ENCODE  0
#define FF_CODE_PAGE    437    /* US */
#define FF_USE_LFN      0      /* بدون Long-File-Names – نام‌ها 8.3 */
#define FF_MAX_LFN      0
#define FF_LFN_UNICODE  0
#define FF_LFN_BUF      0
#define FF_SFN_BUF      11
#define FF_STRF_ENCODE  0
#define FF_FS_RPATH     0
#define FF_VOLUMES      1      /* فقط یک درایو SD */
#define FF_STR_VOLUME_ID 0
#define FF_MULTI_PARTITION 0
#define FF_MIN_SS       512
#define FF_MAX_SS       512
#define FF_LBA64        0
#define FF_MIN_GPT      0x10000000
#define FF_USE_TRIM     0
#define FF_FS_TINY      1      /* مهم: مصرف RAM را به یک sector buffer برای کل volume کاهش می‌دهد */
#define FF_FS_EXFAT     0
#define FF_FS_NORTC     0      /* از get_fattime استفاده می‌شود (در diskio.c پیاده‌شده) */
#define FF_NORTC_MON    1
#define FF_NORTC_MDAY   1
#define FF_NORTC_YEAR   2025
#define FF_FS_NOFSINFO  0
#define FF_FS_LOCK      0
#define FF_FS_REENTRANT 0
```

با این پیکربندی، حافظه‌ی RAM مصرفی FatFS **~۶۰۰ بایت** (یک FATFS struct + sector buffer) خواهد بود.

---

## مرحله ۳ – فعال کردن SD_USE_FATFS

در `Core/Inc/config.h`:

```c
#define SD_USE_FATFS    1
```

و سپس پروژه را rebuild کنید. حالا داده‌های بازه‌ای به‌صورت فایل‌های `.dat` در پوشه‌ی `intervals/` روی کارت SD ذخیره می‌شوند:

```
/intervals/
    2503241105.dat       ← بازه‌ی ساعت 11:05 روز 24 مارس 2025
    2503241110.dat
    2503241115.dat
    ...
```

هر فایل دقیقاً ۲۶۴ بایت است (همان payload فرمت ۸۸۲۱).

---

## مرحله ۴ – فرمت‌دهی کارت SD

قبل از اولین استفاده:

1. کارت را در PC قرار دهید (با reader یا adapter)
2. در ویندوز: `Format... → File system: FAT32 → Allocation unit size: Default`
3. در لینوکس: `sudo mkfs.vfat -F 32 /dev/sdX1`

**کارت‌های ≤ 32GB** را می‌توان با FAT32 فرمت کرد. کارت‌های ۶۴GB و بالاتر معمولاً exFAT دارند که FatFS با FF_FS_EXFAT=0 پشتیبانی نمی‌کند – آنها را با ابزار **FAT32 Format** اجباراً به FAT32 فرمت کنید.

---

## عیب‌یابی

| مشکل | علت احتمالی |
|------|-------------|
| `f_mount` خطای FR_NO_FILESYSTEM | کارت با FAT32 فرمت نشده |
| فایل‌ها روی PC خوانده نمی‌شوند | کارت با raw blocks نوشته شده (SD_USE_FATFS=0 بود) – باید reformat کنید |
| `f_write` خطای FR_DISK_ERR | اتصالات SPI ناپایدار، یا تغذیه‌ی کارت ناکافی |
| init `sd_init` ناموفق | کارت معیوب یا SCK سرعت بالا (init باید ~400kHz باشد) |

---

## مراجع

- [ChaN FatFS Documentation](http://elm-chan.org/fsw/ff/00index_e.html)
- [SD Physical Layer Simplified Specification](https://www.sdcard.org/downloads/pls/)
