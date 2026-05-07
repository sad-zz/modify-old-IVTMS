# RATCX1-STM32 — راهنمای راه‌اندازی یک‌کلیکی

این روش جایگزین مرحله‌ی دستی پیکربندی CubeMX است (مشکلات TIM2/TIM4، RCC، …).
فقط یک بار اجرا می‌کنید و پروژه آماده Build می‌شود.

---

## پیش‌نیاز

- **STM32CubeIDE** نصب شده باشد (نسخه ≥ 1.13)
- **ST-Link V2** برای فلش
- **Bash shell** (لینوکس/مک/Git Bash برای ویندوز) **یا** خط فرمان ویندوز
- این پوشه‌ی `stm32-firmware/` (همانطور که از repo دانلود شده)

---

## مراحل (۵ دقیقه)

### ۱. باز کردن پروژه در CubeIDE

```
File → Open Projects from File System
Directory → روی Browse کلیک کنید → پوشه‌ی stm32-firmware را انتخاب کنید
Folder list باید RATCX1-STM32 را نشان دهد ✓
Finish
```

اگر CubeIDE نگفت "import as STM32 project"، نگران نباشید — بعد از Generate Code تشخیص می‌دهد.

### ۲. تولید کد CubeMX

دوبار کلیک روی **`RATCX1-STM32.ioc`** در Project Explorer.

اگر پرسید "Migrate to a newer firmware version?" → **Yes** بزنید.

روی دکمه‌ی نارنجی **`GENERATE CODE`** بالا-راست بزنید (یا `Alt+K`).

CubeIDE حدود ۲۰ ثانیه طول می‌کشد و یک پروژه‌ی کامل می‌سازد. اگر پرسید "Open Associated Perspective?" → Yes.

> ⚠️ بعد از این مرحله، فایل‌های `Core/Src/main.c` و `Core/Src/stm32f1xx_it.c` ما توسط CubeMX overwrite شدند. مرحله‌ی ۳ آن‌ها را برمی‌گرداند.

### ۳. اجرای setup script

**لینوکس / مک / WSL:**
```bash
cd stm32-firmware
bash setup_after_cubemx.sh
```

**ویندوز (cmd یا PowerShell):**
```powershell
cd stm32-firmware
.\setup_after_cubemx.bat
```

**ویندوز (دو-کلیک):** فایل `setup_after_cubemx.bat` را در File Explorer دو-کلیک کنید.

خروجی موفق:
```
✅ Core/Src/main.c
✅ Core/Src/stm32f1xx_it.c
✅ all application sources present
✅ Setup complete!
```

### ۴. تنظیم config.h

فایل `Core/Inc/config.h` را در CubeIDE باز کنید. این سه فیلد را تنظیم کنید:

```c
#define SYSTEM_ID       "10001704"            // شناسه ثبت‌شده روی سرور
#define SERVER_IP       {192, 168, 1, 100}    // IP سرور TC Manager
#define AIR780_APN      "mtnirancell"         // APN سیم‌کارت
```

| اپراتور ایران | APN |
|---------------|-----|
| ایران‌سل | `mtnirancell` |
| همراه اول | `mcinet` |
| رایتل | `internet` |

### ۵. Refresh و Build

در Project Explorer روی **RATCX1-STM32** راست‌کلیک → **Refresh** (یا F5).

سپس `Ctrl+B` (یا منوی Project → Build All).

خروجی موفق در پنل Console:
```
arm-none-eabi-size  RATCX1-STM32.elf
   text    data     bss     dec     hex
  29384      96   12288   41768    a328
Build Finished. 0 errors, 0 warnings.
```

### ۶. فلش روی برد

ST-Link را به PC و Blue Pill وصل کنید (4 سیم: 3V3، GND، SWDIO، SWCLK).

در CubeIDE: `Run → Run` (یا Ctrl+F11).

پنجره‌ی Run Configuration باز می‌شود → OK بزنید.

اولین بار: STM32CubeProgrammer ممکن است درخواست authentication بدهد → password سیستم را بدهید.

اگر همه چیز ok باشد:
- LED **PC13** روی Blue Pill شروع به چشمک‌زدن ۱Hz می‌کند ✓

---

## مشاهده‌ی boot log

USB-to-TTL converter:
```
Converter RX  ← Blue Pill PA9
Converter GND ← Blue Pill GND
```

PuTTY / Tera Term: `115200 baud, 8N1`. ریست بزنید:

```
RATCX1-STM32 started
Calibrating loops...
Calibration done
Storage: W25Q80 ready          ← اگر فلش وصل است
Storage: SD card ready         ← اگر کارت وصل است
Air780 init...
Air780 ready
```

---

## اشکال‌زدایی سریع

### CubeMX پرسید "open with X"
انتخاب کنید: **STM32 Device Configuration Tool** (پیش‌فرض).

### setup script خطا داد "ساختار پروژه پیدا نشد"
هنوز Generate Code نزدید. مرحله ۲ را تکرار کنید.

### Build خطا: `multiple definition of TIM2_IRQHandler`
setup script اجرا نشده یا اجرا شده ولی Refresh نزدید. F5 بزنید و Build دوباره.

### Build خطا: `'ff.h' file not found`
شما `SD_USE_FATFS=1` گذاشتید ولی FatFS را اضافه نکردید. یا برگردید به `0`، یا طبق `Drivers/FatFS/README.md` فایل‌های FatFS را اضافه کنید.

### Build خطا: `undefined reference to HAL_*`
HAL F1 firmware package نصب نیست. در CubeIDE:
```
Help → Manage Embedded Software Packages
بخش STM32Cube MCU Packages → STM32F1 → آخرین نسخه را تیک بزنید → Install
```

### LED PC13 چشمک نمی‌زند
- خازن رزوناتور 8MHz سالم است؟ بدون HSE، کلاک روی ۸MHz پیش‌فرض می‌ماند و فلش fail می‌شود.
- ولتاژ تغذیه‌ی ۳.۳V پایدار است؟ زیر ۳V → reset.

### ST-Link شناسایی نمی‌شود
درایور USB ST-Link نصب کنید: https://www.st.com/en/development-tools/stsw-link009.html

---

## بعد از تغییر `.ioc`

اگر می‌خواهید pin assignment ها را عوض کنید (مثلاً اضافه کردن یک پین جدید):

1. روی `RATCX1-STM32.ioc` دو-کلیک کنید → تغییرات بدهید → `Ctrl+S`
2. **Generate Code** را دوباره بزنید (CubeMX فایل‌های ما را overwrite می‌کند)
3. **`bash setup_after_cubemx.sh`** را دوباره اجرا کنید (golden copy های ما را برمی‌گرداند)
4. Build

اسکریپت idempotent است؛ هر بار بتوانید بدون مشکل اجرا کنید.

---

## ساختار پروژه پس از Setup

```
stm32-firmware/
├── RATCX1-STM32.ioc                ← فایل پیکربندی CubeMX
├── setup_after_cubemx.sh / .bat    ← اسکریپت بازگردانی فایل‌ها
├── QUICK_START.md                  ← این فایل
├── BUILD_FLASH_TEST.md             ← راهنمای جامع‌تر
├── .cubemx_backup/                 ← (خودکار) backup فایل‌های CubeMX
├── .ratcx1_originals/              ← (خودکار) golden copies اپلیکیشن
├── Core/
│   ├── Inc/                        ← header های اپلیکیشن + main.h + stm32f1xx_it.h
│   ├── Src/                        ← source های اپلیکیشن + main.c + stm32f1xx_it.c
│   └── Startup/startup_*.s         ← (تولید CubeMX)
├── Drivers/
│   ├── STM32F1xx_HAL_Driver/       ← (تولید CubeMX) HAL drivers
│   ├── CMSIS/                      ← (تولید CubeMX) ARM CMSIS
│   ├── Air780/SDCard/W25Q80/FatFS/ ← (ما) راهنماها
└── *.ld                            ← (تولید CubeMX) linker script
```

موفق باشید!
