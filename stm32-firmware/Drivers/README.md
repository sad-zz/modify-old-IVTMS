# RATCX1 – پورت STM32

## پورت فریمور دستگاه RATCX1 از dsPIC30F4011 به STM32F103C8T6

---

## فهرست مطالب
1. [معرفی](#معرفی)
2. [سخت‌افزار مورد نیاز](#سخت‌افزار)
3. [نقشه پین‌ها](#نقشه-پین‌ها)
4. [ساختار پروژه](#ساختار-پروژه)
5. [نگاشت ماژول‌ها](#نگاشت-ماژول‌ها)
6. [راه‌اندازی](#راه‌اندازی)
7. [تفاوت‌های کلیدی با فریمور اصلی](#تفاوت‌های-کلیدی)
8. [کالیبراسیون لوپ](#کالیبراسیون)
9. [پروتکل RATCX1](#پروتکل)
10. [دیباگ](#دیباگ)

---

## معرفی

این پروژه فریمور دستگاه شمارشگر ترافیکی **RATCX1** (دستگاه اصلی با `dsPIC30F4011`) را به
میکروکنترلر **STM32F103C8T6** (Blue Pill) پورت می‌کند.

به جای ماژول GSM/GPRS **SIM900** در دستگاه اصلی، از ماژول **Air780E 4G LTE** استفاده می‌شود.
برای ذخیره آفلاین داده‌های بازه‌ای، **دو backend** به‌صورت auto-detect در زمان boot:
- **W25Q80 8Mbit SPI NOR Flash** روی SPI1
- **SD/MMC کارت** روی SPI2 (اختیاری – با raw block یا FAT32/FatFS)

ساعت سخت‌افزاری (DS1305) حذف شده؛ ساعت نرم‌افزاری توسط TIM4 شمارش می‌شود و
هر بار اتصال به سرور با پیام `0012` با ساعت سرور سنکرون می‌شود.

### مقایسه دستگاه‌ها

| مشخصه | dsPIC30F4011 (اصلی) | STM32F103C8T6 (جدید) |
|--------|---------------------|----------------------|
| معماری | 16-bit DSP | 32-bit Cortex-M3 |
| سرعت | 40 MIPS | 72 MHz |
| Flash | 48 KB | 64 KB |
| RAM | 2 KB | 20 KB |
| شبکه | SIM900 (GPRS/TCP) | Air780E (4G LTE/TCP) |
| حافظه آفلاین | SD/MMC Card (الزامی) | W25Q80 1MB SPI NOR و/یا SD کارت (auto-detect) |
| ساعت | DS1305 RTC خارجی | نرم‌افزاری + sync از سرور |
| قیمت تقریبی | گران و کمیاب | ~۱۰ دلار |
| ابزار توسعه | MPLAB + mikroC | STM32CubeIDE (رایگان) |

---

## سخت‌افزار

### اجزای اصلی
- **STM32F103C8T6** (Blue Pill) یا هر برد مشابه
- **Air780E** ماژول 4G LTE (با سیم‌کارت)
- **W25Q80DV** ماژول SPI NOR Flash 8Mbit (اختیاری – ذخیره آفلاین کوتاه‌مدت)
- **سوکت SD/microSD** (اختیاری – ذخیره طولانی‌مدت با FAT32 قابل خواندن روی PC)
  - **حداقل یکی از این دو** (W25Q80 یا SD) باید برای ذخیره data interval ها در دسترس باشد
- **4 عدد لوپ القایی** (Inductive Loop) + آسیلاتور برای هر لوپ
- **مالتی‌پلکسر 4:1** آنالوگ (مثل CD4052 یا 74HC4052)
- **مقسم ولتاژ** برای ADC باتری و پنل خورشیدی
- **ST-Link V2** برای پروگرم و دیباگ

### ماژول آسیلاتور لوپ
هر لوپ القایی باید به یک آسیلاتور LC متصل باشد. آسیلاتور باید:
- در بازه **۵۰ تا ۲۰۰ kHz** کار کند
- خروجی Square Wave داشته باشد (سازگار با ۳.۳V)
- با عبور وسیله نقلیه، فرکانس آن تغییر کند (به‌خاطر تغییر inductance)

در دستگاه اصلی از تراشه **IC7 (Input Capture)** برای اندازه‌گیری فرکانس استفاده می‌شود.
در این پورت از **TIM2_CH3 (PA2)** در مد Input Capture با TIM2 آزاد در ۱MHz استفاده می‌شود.

---

## نقشه پین‌ها

### STM32F103C8T6 (Blue Pill)

```
                        ┌─────────────────────────┐
         Oscillator MUX →┤ PA2  TIM2_CH3 (IC)       │
              VBAT ADC  →┤ PA0  ADC1_IN0            │
             Solar ADC  →┤ PA1  ADC1_IN1            │
           W25Q80 /CS  ←─┤ PA4  SPI1_NSS            │
           W25Q80 CLK  ←─┤ PA5  SPI1_SCK            │
           W25Q80 DO    →┤ PA6  SPI1_MISO           │
           W25Q80 DI   ←─┤ PA7  SPI1_MOSI           │
         Air780 PWRKEY ←─┤ PA8  GPIO Out            │
          Debug TX UART←─┤ PA9  USART1_TX           │
          Debug RX UART →┤ PA10 USART1_RX           │
         Air780 STATUS  →┤ PA11 GPIO In             │
                         │                          │
          MUX Select A  ←─┤ PB0  GPIO Out            │
          MUX Select B  ←─┤ PB1  GPIO Out            │
          W25Q80 /WP   ←─┤ PB2  GPIO Out (HIGH)     │
          W25Q80 /HOLD ←─┤ PB3  GPIO Out (HIGH)     │
          onloop[0] LED←─┤ PB5  GPIO Out            │
          onloop[1] LED←─┤ PB6  GPIO Out            │
          onloop[2] LED←─┤ PB7  GPIO Out            │
          onloop[3] LED←─┤ PB8  GPIO Out            │
       Connection LED  ←─┤ PB9  GPIO Out            │
         Air780 TX     ←─┤ PB10 USART3_TX           │
         Air780 RX      →┤ PB11 USART3_RX           │
              SD /CS   ←─┤ PB12 GPIO Out            │
              SD SCK   ←─┤ PB13 SPI2_SCK            │
              SD MISO   →┤ PB14 SPI2_MISO           │
              SD MOSI  ←─┤ PB15 SPI2_MOSI           │
                         │                          │
        Heartbeat LED  ←─┤ PC13 (Built-in LED)      │
                        └─────────────────────────┘
```

### مالتی‌پلکسر ۴:۱ (CD4052)

```
Loop 0 (Lane1 Far)  ──→ IN0 ─┐
Loop 1 (Lane1 Near) ──→ IN1 ─┤ CD4052 ──→ OUT ──→ PA2 (TIM2_CH3)
Loop 2 (Lane2 Far)  ──→ IN2 ─┤    ↑
Loop 3 (Lane2 Near) ──→ IN3 ─┘    │
                              PB0 (A) + PB1 (B)
```

---

## ساختار پروژه

```
stm32-firmware/
├── README.md                    ← این فایل
├── Core/
│   ├── Inc/
│   │   ├── main.h
│   │   ├── config.h             ← تنظیمات دستگاه (APN، IP سرور، …)
│   │   ├── variables.h          ← متغیرهای سراسری
│   │   ├── loop_detector.h      ← تشخیص لوپ
│   │   ├── classification.h     ← طبقه‌بندی وسیله نقلیه
│   │   ├── interval.h           ← داده‌های بازه‌ای
│   │   ├── protocol.h           ← پروتکل RATCX1
│   │   ├── air780_tcp.h         ← درایور Air780 4G LTE
│   │   ├── storage.h            ← لایه abstraction روی W25Q80/SD
│   │   ├── w25q80.h             ← درایور W25Q80 Flash
│   │   └── sd_spi.h             ← درایور SD/MMC کارت روی SPI2
│   └── Src/
│       ├── main.c               ← تابع main، init پریفرال‌ها، حلقه اصلی
│       ├── loop_detector.c      ← اندازه‌گیری فرکانس و تشخیص وسیله
│       ├── classification.c     ← محاسبه سرعت، طول، طبقه
│       ├── interval.c           ← ساخت رشته ۲۶۲ کاراکتری ۸۸۲۱ + ذخیره
│       ├── protocol.c           ← پروتکل TCP
│       ├── air780_tcp.c         ← AT command driver برای Air780
│       ├── storage.c            ← runtime auto-detect و dispatch
│       ├── w25q80.c             ← SPI NOR Flash driver
│       └── sd_spi.c             ← SD/MMC SPI block driver (init/read/write)
└── Drivers/
    ├── Air780/
    │   └── README.md            ← راهنمای اتصال و APN
    ├── W25Q80/
    │   └── README.md            ← راهنمای اتصال و طرح ذخیره
    ├── SDCard/
    │   └── README.md            ← راهنمای اتصال SD کارت روی SPI2
    └── FatFS/
        ├── diskio.c             ← glue لایه‌ی FatFS↔sd_spi
        └── README.md            ← دستور دانلود و فعال‌سازی FAT32
```

---

## نگاشت ماژول‌ها

### ماژول‌های dsPIC → STM32

| ماژول dsPIC اصلی | فایل اصلی | STM32 معادل | فایل جدید |
|-----------------|-----------|-------------|-----------|
| TMR4 ISR (1ms) | `91-7.c:34` | TIM4 ISR (1ms) | `loop_detector.c` |
| IC7 Input Capture | `Capture_Int_Lib.h` | TIM2_CH3 IC | `loop_detector.c` |
| `measure_loops()` | `91-7.c:254` | `measure_loops()` | `classification.c` |
| `cal_class()` | `Classification.h:25` | `cal_class()` | `classification.c` |
| `cal_interval()` | `Interval.h:103` | `cal_interval()` + storage save | `interval.c` + `storage.c` |
| GPRS/SIM900 state machine | `91-7.c:703-986` | Air780E 4G AT driver | `protocol.c` + `air780_tcp.c` |
| `rtc_read/write()` | `DS1305_Lib.h` | Software RTC در TIM4 ISR (sync با سرور) | `loop_detector.c` + `protocol.c` |
| EEPROM read/write | `91-7.c:232` | config.h constants | `config.h` |
| MMC/SD Card storage | `91-7.c:Mmc_Read/Write` | W25Q80 + SD (auto-detect) | `storage.c` + `w25q80.c` + `sd_spi.c` |
| UART1 (debug) | `UART_Int_Lib.h` | USART1 | `main.c` |
| ADC1 (VBAT/Solar) | `91-7.c:518` | ADC1 CH0/CH1 | `main.c` |

### تغییر اصلی: GPRS → 4G LTE

در فریمور اصلی، ارتباط با سرور از طریق ماژول **SIM900** و دستورات AT انجام می‌شد.
در پورت STM32، از **Air780 4G LTE** استفاده می‌شود که همان API TCP را ارائه می‌دهد:
```c
tcp_connect(server_ip, port);
tcp_send(data, len);
tcp_recv(buf, len);
```

### تغییر جدید: ذخیره‌سازی دو-backend با auto-detect

داده‌های بازه‌ای روی هر کدام از این backend ها که در زمان boot شناسایی شوند، ذخیره می‌گردند:

- **W25Q80 SPI NOR Flash** روی SPI1 (PA4-PA7): ۱۲۸ اسلات × ۵ دقیقه = **۱۰.۷ ساعت** آفلاین
- **SD/MMC کارت** روی SPI2 (PB12-PB15): ۲۰۴۸ اسلات × ۵ دقیقه = **~۷ روز** آفلاین (raw block) یا
  ظرفیت کامل کارت با FAT32 (نیازمند FatFS؛ راهنما در `Drivers/FatFS/README.md`)

اگر هر دو هم‌زمان حاضر باشند، روی هر دو نوشته می‌شود (redundancy). در پاسخ به درخواست `0197`،
ابتدا SD سپس W25Q80 جستجو می‌شود.

---

## راه‌اندازی

### ۱. ایجاد پروژه در STM32CubeIDE

```
File → New → STM32 Project
Target: STM32F103C8T6
Name: RATCX1-STM32
```

### ۲. کپی فایل‌های این پروژه
تمام فایل‌های `Core/Src/*.c` و `Core/Inc/*.h` را به پروژه اضافه کنید.

### ۳. تنظیم APN سیم‌کارت
فایل `Core/Inc/config.h`:
```c
#define AIR780_APN  "mtnirancell"  // ایران‌سل، یا "mcinet" برای MCI
```

### ۴. تنظیم آدرس IP سرور
فایل `Core/Inc/config.h`:
```c
#define SERVER_IP    {192, 168, 1, 100}  // IP سرور TC Manager شما
#define SERVER_PORT  2022
```

### ۵. تنظیم شناسه دستگاه
فایل `Core/Inc/config.h`:
```c
#define SYSTEM_ID   "10001704"  // باید با آنچه در سرور ثبت شده مطابقت داشته باشد
```

### ۶. Build و Flash
```
Project → Build All  (Ctrl+B)
Run → Debug          (F11)
```

---

## تنظیم پارامترها

همه پارامترها در `Core/Inc/config.h` قابل تنظیم هستند:

```c
/* هندسه لوپ */
#define LOOP_DISTANCE   200   // فاصله بین مراکز دو لوپ (mm)
#define LOOP_WIDTH      80    // طول هر لوپ در جهت حرکت (mm)

/* آستانه تشخیص */
#define MARGIN_TOP      200   // آستانه تشخیص وسیله نقلیه (0.01%)
#define MARGIN_BOT      100   // آستانه خروج وسیله نقلیه

/* محدودیت‌های طولی طبقه‌بندی (cm) */
#define LIMA   150   // A = موتورسیکلت
#define LIMB   250   // B = سواری
#define LIMC   450   // C = وانت/مینی‌بوس
#define LIMD   600   // D = اتوبوس
#define LIMITE 1200  // E = کامیون (بزرگتر از این = X)

/* محدودیت سرعت (km/h) */
#define DSPEED1  80   // روز - کلاس A
#define NSPEED1  100  // شب - کلاس A
```

---

## تفاوت‌های کلیدی

### ۱. ارتباط شبکه
- **اصلی:** SIM900 GPRS + AT commands
- **جدید:** Air780 4G LTE + AT commands (سریع‌تر، پوشش بهتر در ایران)

### ۲. ذخیره تنظیمات
- **اصلی:** EEPROM داخلی dsPIC
- **جدید:** ثابت‌های `config.h` در Flash (یا Flash Emulated EEPROM برای تنظیم پویا)

### ۳. ذخیره داده (دوگانه با auto-detect)
- **اصلی:** MMC/SD card الزامی روی SPI
- **جدید:** SPI NOR (W25Q80) و/یا SD کارت روی SPI2 با تشخیص خودکار. هر کدام حاضر باشد فعال می‌شود.

### ۴. ساعت
- **اصلی:** DS1305 RTC خارجی روی SPI
- **جدید:** نرم‌افزاری در TIM4 ISR + سنکرون با سرور از طریق پیام `0012`

---

## کالیبراسیون

در هنگام راه‌اندازی، تابع `loop_calibrate()` اجرا می‌شود:
1. ۱ ثانیه صبر می‌کند
2. ۱۰ بار فرکانس پایه هر لوپ را اندازه‌گیری می‌کند
3. میانگین را به عنوان baseline ذخیره می‌کند

**مهم:** در هنگام کالیبراسیون، هیچ وسیله‌ای نباید روی لوپ‌ها باشد.

پس از کالیبراسیون، اگر `AUTCAL=1` باشد، baseline هر ۳۰ ثانیه به آرامی بروزرسانی می‌شود.

---

## پروتکل

پروتکل RATCX1 کاملاً مشابه دستگاه اصلی است و سرور TC Manager بدون هیچ تغییری کار می‌کند:

```
دستگاه → سرور:  8000 + datetime(21) + sysId(8) + model + version + "READY"
سرور → دستگاه:  0012 + "YYYY.MM.DD-HH:MM:SS.0"   ← سنکرون ساعت
دستگاه → سرور:  8012 + datetime(21) + sysId(8)    ← تأیید
سرور → دستگاه:  0197 + "YYMMDDHHmm"               ← درخواست داده
دستگاه → سرور:  8821 + datetime(21) + data(262) + CRLF
```

---

## دیباگ

### دیباگ از طریق USART1

دستگاه پیام‌های وضعیت را روی **PA9 (TX) @ 115200 baud** ارسال می‌کند.

با یک USB-to-Serial converter (مثل CH340):
```
PA9 → RX converter
PA10 → TX converter
GND → GND
```

سپس از PuTTY یا minicom:
```
Port: COM3 (یا /dev/ttyUSB0)
Baud: 115200
8N1
```

### پیام‌های دیباگ
```
RATCX1-STM32 started
Calibrating loops...
Calibration done
Storage: W25Q80 ready          ← فقط فلش پیدا شد
Storage: SD card ready         ← فقط SD پیدا شد
                                  (یا هر دو خط با هم اگر هر دو حاضر باشند)
Air780 init...
Air780 ready
Interval ready                 ← هر ۵ دقیقه
```

### بررسی داده با سرور
لاگ سرور TC Manager را بررسی کنید:
```bash
pm2 logs tc-manager --lines 50
```
باید پیام `[TCP] *** 8000` را ببینید.

---

## مجوز

این فریمور برای استفاده با سخت‌افزار اصلی RATCX1 و سرور TC Manager طراحی شده است.
