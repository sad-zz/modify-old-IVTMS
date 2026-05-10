# روش ساده — راه‌اندازی بدون درگیری با CubeMX

این روش همه‌ی مشکلات Generate Code رو دور می‌زنه. main.c ما خودش
init تمام پریفرال‌ها (clock، GPIO، TIM، UART، SPI، ADC، NVIC) رو
انجام می‌ده، پس واقعاً نیازی به پیکربندی .ioc نیست.

---

## پیش‌نیاز

- **STM32CubeIDE** نصب شده باشه (نه CubeMX standalone)
  - دانلود: https://www.st.com/en/development-tools/stm32cubeide.html
  - نصب پیش‌فرض. در حین نصب پکیج F1 رو هم می‌خواد؛ تیک بزنید.
- ST-Link V2
- USB-to-TTL converter (برای دیدن پیام‌های debug)

---

## مرحله ۱ — ساخت پروژه‌ی خالی STM32F103

در STM32CubeIDE:

```
File → New → STM32 Project
```

پنجره‌ای باز می‌شه (ممکنه چند ثانیه طول بکشه):

| فیلد | مقدار |
|------|------|
| **Part Number Search** (در نوار جستجو بالا چپ) | تایپ کنید: `STM32F103C8` |
| لیست نتایج | `STM32F103C8Tx` (LQFP48) را انتخاب کنید |
| **Next** | کلیک |

صفحه‌ی بعدی:

| فیلد | مقدار |
|------|------|
| **Project Name** | `RATCX1-STM32` |
| **Use default location** | تیک بمونه ✓ |
| **Targeted Language** | `C` |
| **Targeted Binary Type** | `Executable` |
| **Targeted Project Type** | `STM32Cube` |
| **Finish** | کلیک |

اگر پرسید `Initialize all peripherals with their default Mode?` → **Yes** بزنید (تا HAL modules فعال بشن، فرقی نمی‌کنه چه پریفرالی).

اگر پرسید `Open Associated Perspective?` → **Yes**.

CubeIDE یه پروژه‌ی خالی می‌سازه با ساختار:

```
RATCX1-STM32/
├── RATCX1-STM32.ioc           ← فایل خالی CubeMX (نگران نباشید، استفاده نمی‌شه)
├── Core/
│   ├── Inc/main.h
│   ├── Inc/stm32f1xx_it.h
│   ├── Inc/stm32f1xx_hal_conf.h
│   ├── Src/main.c             ← خالی، باید عوض بشه
│   ├── Src/stm32f1xx_it.c     ← باید عوض بشه
│   ├── Src/stm32f1xx_hal_msp.c
│   └── Src/system_stm32f1xx.c
├── Drivers/STM32F1xx_HAL_Driver/   ← HAL درایورها (CubeIDE خودکار آورده)
├── Drivers/CMSIS/                  ← هدرهای ARM
└── STM32F103C8TX_FLASH.ld          ← linker script
```

> **مهم:** **این مسیر را یادداشت کنید.** پیش‌فرض کجاست:
> - **ویندوز:** `C:\Users\<your_name>\STM32CubeIDE\workspace_<version>\RATCX1-STM32\`
> - **لینوکس:** `~/STM32CubeIDE/workspace_<version>/RATCX1-STM32/`
> - **مک:** `~/STM32CubeIDE/workspace_<version>/RATCX1-STM32/`
>
> راحت‌ترین راه پیدا کردن: در CubeIDE → روی پروژه راست‌کلیک → **Properties** → **Resource** → خط `Location` مسیر کامل را نشان می‌دهد.

---

## مرحله ۲ — کپی فایل‌های ما به پروژه

از repo ما پوشه‌ی `stm32-firmware/Core/` رو دارید. **کل محتوای** اون رو روی پروژه‌ی CubeIDE که در مرحله ۱ ساختید، **overwrite** می‌کنید.

### روش الف — کشیدن و رها کردن (Drag & Drop) در CubeIDE

این آسون‌ترین روشه:

1. در CubeIDE، Project Explorer رو باز کنید (سمت چپ).
2. پروژه‌ی `RATCX1-STM32` رو expand کنید.
3. **پوشه `Core/Inc`** رو انتخاب کنید.
4. از پنجره‌ی فایل سیستم (Windows Explorer / Finder / Nautilus)، کل **محتوای** پوشه‌ی `stm32-firmware/Core/Inc/` ما (یعنی همه‌ی فایل‌های `.h`) رو انتخاب کنید.
5. **Drag** کنید روی پوشه‌ی `Core/Inc` در CubeIDE.
6. پنجره‌ای باز می‌شه: انتخاب کنید **`Copy files`** → OK.
7. اگه پرسید "Overwrite?" → **Yes To All**.

همین کار رو برای `Core/Src/` تکرار کنید.

### روش ب — کپی مستقیم در فایل سیستم

1. CubeIDE رو ببندید (یا حداقل پروژه رو close کنید).
2. در فایل سیستم به مسیر پروژه برید (که در مرحله ۱ یادداشت کردید).
3. کل **محتوای** `stm32-firmware/Core/Inc/` ما رو کپی کنید روی `<workspace>/RATCX1-STM32/Core/Inc/` با overwrite.
4. کل محتوای `stm32-firmware/Core/Src/` رو روی `<workspace>/RATCX1-STM32/Core/Src/` با overwrite.
5. CubeIDE رو دوباره باز کنید.
6. روی پروژه راست‌کلیک → **Refresh** (F5).

### چک کنید این فایل‌ها در پروژه هستند

پس از کپی، `Core/Inc/` باید این فایل‌ها داشته باشه:
- `main.h`
- `stm32f1xx_it.h`
- `stm32f1xx_hal_conf.h` (نسخه‌ی ما، با همه ماژول‌ها enabled)
- `config.h`
- `variables.h`
- `loop_detector.h`، `classification.h`، `interval.h`، `protocol.h`
- `air780_tcp.h`، `storage.h`، `w25q80.h`، `sd_spi.h`

و `Core/Src/` باید داشته باشه:
- `main.c`
- `stm32f1xx_it.c`
- `system_stm32f1xx.c` (همون که CubeIDE اول گذاشته بود – دست نزدید)
- `stm32f1xx_hal_msp.c` (همون که CubeIDE گذاشته بود – دست نزدید)
- `loop_detector.c`، `classification.c`، `interval.c`، `protocol.c`
- `air780_tcp.c`، `storage.c`، `w25q80.c`، `sd_spi.c`

---

## مرحله ۳ — تنظیم config.h

در CubeIDE فایل `Core/Inc/config.h` رو باز کنید:

```c
#define SYSTEM_ID       "10001704"            // ID ثبت‌شده روی سرور
#define SERVER_IP       {192, 168, 1, 100}    // IP سرور TC Manager
#define AIR780_APN      "mtnirancell"         // APN سیم‌کارت
```

`Ctrl+S` ذخیره کنید.

---

## مرحله ۴ — Build

```
Project → Build All  (یا Ctrl+B)
```

اگه با خطای کامپایل مواجه شدید، احتمال‌دار:

| خطا | چاره |
|-----|------|
| `'hspi2' undeclared` | فایل main.c ما کپی نشده درست. مرحله ۲ رو دوباره چک کنید. |
| `multiple definition of 'main'` | فایل main.c قبلی هنوز هست. حتماً overwrite شه. |
| `multiple definition of 'TIM2_IRQHandler'` | stm32f1xx_it.c قدیمی هنوز هست. overwrite کنید. |
| `'air780_uart_irq' undefined` | فایل stm32f1xx_it.c ما کپی نشده. |
| `undefined reference to 'HAL_*'` | HAL F1 پک نصب نیست. در CubeIDE: Help → Manage Embedded Software Packages → STM32F1 → Install. |
| `'ff.h' file not found` | `SD_USE_FATFS=1` دادید ولی FatFS نصب نکردید. در config.h `=0` کنید. |

اگر build موفق:
```
arm-none-eabi-size  RATCX1-STM32.elf
   text    data     bss     dec     hex
  29384      96   12288   41768    a328
Build Finished. 0 errors, 0 warnings.
```

---

## مرحله ۵ — Flash روی برد

ST-Link V2 رو وصل کنید (3.3V، GND، SWDIO=PA13، SWCLK=PA14 به Blue Pill).

```
Run → Run  (Ctrl+F11)
```

اولین بار ممکنه پنجره‌ی **Run Configuration** باز شه:
- Project: RATCX1-STM32
- C/C++ Application: `Debug/RATCX1-STM32.elf`
- Apply → Run

اگر LED PC13 شروع به چشمک‌زدن ۱Hz کرد → **موفق!**

---

## مرحله ۶ — مشاهده‌ی boot log

USB-to-TTL converter رو وصل کنید:
```
Converter RX  ←  Blue Pill PA9
Converter GND ←  Blue Pill GND
```

PuTTY / Tera Term با تنظیمات `115200 baud, 8N1` باز کنید. RESET بزنید روی Blue Pill:

```
RATCX1-STM32 started
Calibrating loops...
Calibration done
Storage: W25Q80 ready
Storage: SD card ready
Air780 init...
Air780 ready
```

اگر این پیام‌ها رو دیدید، فریمور درست کار می‌کنه!

---

## چرا این روش کار می‌کنه؟

`main.c` ما شامل توابع زیر هست که همه چیز رو خودش راه می‌اندازه:
- `SystemClock_Config()` → کلاک ۷۲MHz از HSE با PLL×9
- `MX_GPIO_Init()` → پیکربندی همه‌ی GPIO ها
- `MX_TIM2_Init()` → Input Capture برای لوپ
- `MX_TIM4_Init()` → 1ms tick
- `MX_USART1_UART_Init()` → debug UART
- `MX_USART3_UART_Init()` → Air780 UART
- `MX_SPI1_Init()` → W25Q80
- `MX_SPI2_Init()` → SD card
- `MX_ADC1_Init()` → باتری/خورشیدی
- `HAL_MspInit()` → JTAG remap به SWD

پس فقط نیاز داریم HAL درایورها در پروژه باشن (که با ساخت پروژه‌ی STM32 توسط CubeIDE خودکار اضافه می‌شن). همین.

---

## تنظیمات بعدی

- **برای تنظیم APN/IP/SystemID:** `Core/Inc/config.h`
- **برای فعال کردن FAT32 روی SD:** `Drivers/FatFS/README.md`
- **برای راهنمای کامل سخت‌افزار + عیب‌یابی:** `BUILD_FLASH_TEST.md`
