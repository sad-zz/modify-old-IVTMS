# SD/MMC Card – راهنمای اتصال و استفاده

## مرور سریع

این درایور یک کارت SD/SDHC/MMC را در حالت SPI به STM32F103C8T6 وصل می‌کند. در پروژه‌ی RATCX1-STM32 به‌عنوان **storage backend ثانویه** در کنار W25Q80 SPI flash استفاده می‌شود (auto-detect در زمان boot).

| ویژگی | مقدار |
|-------|-------|
| پشتیبانی | SDv1، SDv2 (SDSC)، SDHC، SDXC، MMCv3 |
| رابط | SPI mode 0 |
| ولتاژ | 3.3V |
| سرعت init | ~140 kHz (prescaler 256) |
| سرعت کار | ~9 MHz (prescaler 4) |
| اندازه بلاک | 512 بایت |

---

## اتصال سخت‌افزاری به STM32F103C8T6

```
STM32F103       SD card (microSD socket پیشنهادی: HC-SDC رایج)
─────────────────────────────────────────────────────────────
PB12 (GPIO Out) → /CS    (پایه ۱ SD)         + 10kΩ pull-up به VCC
PB13 (SPI2_SCK) → SCLK   (پایه ۵)
PB14 (SPI2_MISO) ← DO    (پایه ۷، DAT0)      + 10kΩ pull-up به VCC
PB15 (SPI2_MOSI) → DI    (پایه ۲، CMD)       + 10kΩ pull-up به VCC
3.3V            → VCC    (پایه ۴)
GND             → VSS    (پایه ۳ و ۶)
```

> ⚠️ **نکته مهم تغذیه:** مصرف لحظه‌ای کارت SD می‌تواند به ~۱۰۰mA برسد (به‌خصوص هنگام write). یک خازن **10µF + 100nF** نزدیک پایه‌ی VCC کارت اضافه کنید.

> ⚠️ **سطح ولتاژ:** STM32F103 و کارت SD هر دو ۳.۳V هستند – نیازی به level shifter نیست.

---

## دو حالت ذخیره‌سازی

### حالت ۱ – Raw Block (پیش‌فرض)

`#define SD_USE_FATFS  0` در `Core/Inc/config.h`

داده‌ها مستقیماً به سکتورهای SD نوشته می‌شوند، **بدون فایل سیستم**. این حالت:
- ✅ بدون نیاز به library خارجی
- ✅ مصرف RAM ≈ 0
- ❌ کارت روی PC قابل خواندن نیست (باید با ابزار hex-editor خواند)

ساختار: ۲۰۴۸ اسلات، هر اسلات یک سکتور ۵۱۲ بایتی، آدرس‌دهی بر اساس timestamp:

```
slot = (mo*31 + dd) * 24 * 12 + hh*12 + mm/5  mod 2048
LBA  = SD_RAW_BASE_LBA + slot
```

پوشش زمانی: ۲۰۴۸ × ۵ دقیقه = **~۷ روز** ذخیره آفلاین.

### حالت ۲ – FAT32 با FatFS

`#define SD_USE_FATFS  1` در `Core/Inc/config.h`

داده‌ها به‌صورت فایل‌های منفرد در پوشه‌ی `intervals/` ذخیره می‌شوند:

```
/intervals/
    YYMMDDHHmm.dat    ← هر فایل ۲۶۴ بایت (payload کامل 8821)
```

این حالت:
- ✅ کارت روی PC قابل خواندن
- ✅ تعداد فایل عملاً نامحدود
- ❌ نیازمند ChaN FatFS library (راهنمای نصب: `Drivers/FatFS/README.md`)

---

## تست سریع

بعد از flash کردن firmware و قرار دادن کارت SD، روی USART1 (PA9) با ۱۱۵۲۰۰ baud یکی از این پیغام‌ها را خواهید دید:

```
Storage: W25Q80 ready          ← فقط فلش
Storage: SD card ready         ← فقط SD
Storage: W25Q80 ready
Storage: SD card ready         ← هر دو فعال (redundancy)
```

اگر کارت کار نکرد، علت‌های رایج:
| مشکل | راه‌حل |
|------|------|
| init شکست (`sd_init` خطا) | اتصالات SPI، تغذیه ۳.۳V، pull-up روی DAT0 |
| MMC قدیمی (نه SD) | احتمالاً نسخه‌ی قدیمی است؛ کارت SDHC مدرن استفاده کنید |
| sector_count = 0 | CSD ناقص خوانده می‌شود – سرعت SCK را در init کم کنید |
| FatFS mount خطا (FR_NO_FILESYSTEM) | کارت با FAT32 فرمت نشده |

---

## مراجع

- [SD Physical Layer Simplified Specification v3.01](https://www.sdcard.org/downloads/pls/)
- [Elm-Chan SPI SD interface notes](http://elm-chan.org/docs/mmc/mmc_e.html)
