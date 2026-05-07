# راهنمای گام‌به‌گام Build، Flash و تست RATCX1-STM32

این راهنما کل فرایند از صفر تا اولین interval موفق روی سرور را پوشش می‌دهد.

## فهرست مراحل

| # | مرحله | زمان تقریبی |
|---|-------|-------------|
| 1 | نصب ابزار توسعه | ۲۰ دقیقه |
| 2 | ساخت پروژه در STM32CubeIDE | ۱۰ دقیقه |
| 3 | افزودن سورس‌ها به پروژه | ۵ دقیقه |
| 4 | پیکربندی پریفرال‌ها در `.ioc` | ۲۰ دقیقه |
| 5 | تنظیم config.h | ۵ دقیقه |
| 6 | (اختیاری) فعال‌سازی FatFS | ۱۰ دقیقه |
| 7 | Build اولیه | ۵ دقیقه |
| 8 | اتصالات سخت‌افزاری | ۳۰ دقیقه |
| 9 | Flash روی برد | ۵ دقیقه |
| 10 | تست USART debug | ۱۰ دقیقه |
| 11 | تست هر backend به‌صورت ایزوله | ۲۰ دقیقه |
| 12 | تست end-to-end با سرور | ۱۵ دقیقه |

---

## ۱. نصب ابزار توسعه

### ۱.۱ STM32CubeIDE
از سایت ST دانلود کنید (نسخه پیشنهادی ≥ 1.13):

> https://www.st.com/en/development-tools/stm32cubeide.html

نصب پیش‌فرض را بپذیرید. در ضمن نصب، **STM32Cube MCU Package for STM32F1 Series** نیز نصب می‌شود (شامل HAL).

### ۱.۲ ST-Link USB driver (فقط ویندوز)
> https://www.st.com/en/development-tools/stsw-link009.html

روی لینوکس و مک نیاز نیست.

### ۱.۳ Terminal سریال
- ویندوز: PuTTY یا Tera Term
- لینوکس: `minicom`، `picocom`، یا `screen`
- مک: `screen /dev/tty.usbserial-XXXX 115200`

### ۱.۴ یک USB-to-TTL converter
برای مشاهده پیام‌های USART1 (debug). معمول‌ترین: مدل CH340G یا CP2102.

---

## ۲. ساخت پروژه جدید در STM32CubeIDE

```
File → New → STM32 Project
```

**Target Selection:**
- بخش "Part Number Search" تایپ کنید: `STM32F103C8`
- در نتایج، **STM32F103C8Tx** را انتخاب کرده و Next بزنید.

**Project Setup:**
- Project Name: `RATCX1-STM32`
- Targeted Language: `C`
- Targeted Binary Type: `Executable`
- Targeted Project Type: `STM32Cube`
- Finish بزنید.

اگر STM32CubeIDE پرسید "Initialize all peripherals with their default Mode?" → **No** بزنید (ما خودمان دستی اضافه می‌کنیم).

---

## ۳. افزودن سورس‌های پروژه

پنجره‌ی Project Explorer در سمت چپ:

1. کل محتوای `stm32-firmware/Core/Inc/` این ریپو را به پوشه‌ی `Core/Inc/` پروژه کپی کنید (drag & drop در Project Explorer کافی است → "Copy files" را انتخاب کنید).
2. کل محتوای `stm32-firmware/Core/Src/` را به `Core/Src/` کپی کنید.
3. پنجره‌ی پاپ‌آپ بپرسد فایل را replace کنیم؟ → **Yes to All** (مخصوصاً برای `main.c`).

> اگر STM32CubeIDE فایل `main.c` خودکار خود را با `/* USER CODE BEGIN */` ها ساخته باشد، آن را با نسخه‌ی این پروژه جایگزین کنید (نسخه‌ی این پروژه self-contained است).

---

## ۴. پیکربندی پریفرال‌ها در فایل `.ioc`

روی `RATCX1-STM32.ioc` در Project Explorer دو-کلیک کنید. تب **Pinout & Configuration** باز می‌شود.

### ۴.۱ System Core

**RCC** (در منوی System Core):
- High Speed Clock (HSE): `Crystal/Ceramic Resonator`
- Low Speed Clock (LSE): `Disable`

**SYS:**
- Debug: `Serial Wire`
- Timebase Source: `SysTick`

### ۴.۲ Clock Configuration

به تب **Clock Configuration** بروید:
- HSE: `8 MHz`
- PLL Source: `HSE`
- PLLMul: `×9`
- System Clock Mux: `PLLCLK`
- HCLK: `72 MHz` (پر می‌شود)
- APB1: `÷2 → 36 MHz`
- APB2: `÷1 → 72 MHz`
- ADC Prescaler: `÷6 → 12 MHz`

### ۴.۳ Connectivity

**USART1** (debug):
- Mode: `Asynchronous`
- Baud rate: `115200`، 8N1
- پین‌ها خودکار: PA9 (TX)، PA10 (RX)

**USART3** (Air780):
- Mode: `Asynchronous`
- Baud rate: `115200`، 8N1
- پین‌ها خودکار: PB10 (TX)، PB11 (RX)
- در تب NVIC: ✓ `USART3 global interrupt`

**SPI1** (W25Q80):
- Mode: `Full-Duplex Master`
- Hardware NSS Signal: `Disable`
- Data Size: `8 bits`، MSB First
- Prescaler: `4` → 18 MHz baud rate
- CPOL/CPHA: Low / 1Edge (Mode 0)
- پین‌ها خودکار: PA5/PA6/PA7

**SPI2** (SD card):
- Mode: `Full-Duplex Master`
- Hardware NSS Signal: `Disable`
- Data Size: `8 bits`، MSB First
- Prescaler: `256` → ~140 kHz (در init)
- CPOL/CPHA: Low / 1Edge (Mode 0)
- پین‌ها خودکار: PB13/PB14/PB15

### ۴.۴ Analog

**ADC1**:
- IN0 ✓
- IN1 ✓
- Continuous Conversion Mode: Disable
- Discontinuous Conversion Mode: Disable

### ۴.۵ Timers

**TIM2**:
- Clock Source: `Internal Clock`
- Channel 3: `Input Capture direct mode`
- Counter Settings:
  - Prescaler: `71`
  - Counter Period: `0xFFFFFFFF`
- Input Capture Channel 3:
  - Polarity Selection: `Rising Edge`
  - Prescaler: `1`
- در تب NVIC: ✓ `TIM2 global interrupt`

**TIM4**:
- Clock Source: `Internal Clock`
- Counter Settings:
  - Prescaler: `71`
  - Counter Period: `999`
- در تب NVIC: ✓ `TIM4 global interrupt`

### ۴.۶ GPIO های دستی

روی این پین‌ها کلیک کنید و mode را به `GPIO_Output` تغییر دهید:
- PA4 (W25Q80 /CS) — output، high level، high speed
- PA8 (Air780 PWRKEY) — output، low level، low speed
- PB0, PB1 (Mux A/B) — output، low level، low speed
- PB2, PB3 (W25Q80 /WP, /HOLD) — output، high level، low speed
- PB5, PB6, PB7, PB8, PB9 (LEDs) — output، low level، low speed
- PB12 (SD /CS) — output، high level، high speed
- PC13 (LED) — output، high level، low speed

روی این پین‌ها mode را به `GPIO_Input` تغییر دهید:
- PA11 (Air780 STATUS) — input، pull-down

PA2 خودکار توسط TIM2_CH3 کانفیگ می‌شود؛ نیازی به تغییر دستی نیست.

### ۴.۷ JTAG → SWD remap

به تب **System Core → SYS** بروید:
- Debug: `Serial Wire`

این کار PB3/PB4/PA15 را آزاد می‌کند تا به‌عنوان GPIO استفاده شوند (W25Q80 /HOLD روی PB3 است).

> **مهم:** پروژه‌ی ما در `HAL_MspInit()` خودش `__HAL_AFIO_REMAP_SWJ_NOJTAG()` را فراخوانی می‌کند، اما اگر STM32CubeIDE خودش این تنظیم را ندید، در `.ioc` تب SYS یا در فایل `stm32f1xx_hal_msp.c` این فراخوانی را اضافه کنید.

### ۴.۸ Generate code

`Ctrl+S` بزنید. STM32CubeIDE می‌پرسد "Generate Code?" → **Yes**.

> اگر گزینه‌ی "Open Associated Perspective" را انتخاب کرد → Yes.

---

## ۵. تنظیم `config.h`

فایل `Core/Inc/config.h` را باز کنید و این موارد را تنظیم کنید:

```c
#define SYSTEM_ID       "10001704"          /* با ID ثبت‌شده روی سرور یکسان باشد */
#define SERVER_IP       {192, 168, 1, 100}  /* IP سرور TC Manager */
#define SERVER_PORT     2022
#define AIR780_APN      "mtnirancell"       /* یا "mcinet" یا "internet" */
```

### پیشنهادات APN ایران

| اپراتور | APN |
|---------|-----|
| ایران‌سل (MTN-Irancell) | `mtnirancell` |
| همراه اول (MCI) | `mcinet` |
| رایتل | `internet` |
| شاتل موبایل | `shatelmobile` |

---

## ۶. (اختیاری) فعال‌سازی FAT32 با FatFS

اگر می‌خواهید فایل‌های روی SD کارت روی PC قابل خواندن باشند:

1. از http://elm-chan.org/fsw/ff/ نسخه R0.15 یا بالاتر را دانلود کنید.
2. فایل‌های `ff.c`, `ff.h`, `ffconf.h`, `ffsystem.c`, `diskio.h` را به پوشه‌ی `Drivers/FatFS/` پروژه کپی کنید.
3. `ffconf.h` را طبق دستور `Drivers/FatFS/README.md` تنظیم کنید (مهم: `FF_FS_TINY=1`، `FF_USE_LFN=0`).
4. در `config.h`:
   ```c
   #define SD_USE_FATFS  1
   ```
5. مسیر `Drivers/FatFS` را به include path پروژه اضافه کنید:
   - راست‌کلیک روی پروژه → Properties → C/C++ Build → Settings → Include paths → Add
   - مسیر: `${workspace_loc:/${ProjName}/Drivers/FatFS}`

اگر این مرحله را رد کنید، SD به‌صورت raw block استفاده می‌شود (پیش‌فرض، بدون نیاز به اقدام اضافه).

---

## ۷. Build اولیه

```
Project → Build All  (Ctrl+B)
```

خروجی موفق:
```
arm-none-eabi-size  RATCX1-STM32.elf
   text    data     bss     dec     hex
  29384      96   12288   41768    a328
Build Finished. 0 errors, 0 warnings.
```

اگر error داشت رایج‌ترین علت‌ها:
| Error | چاره |
|-------|------|
| `undefined reference to HAL_*` | پکیج HAL F1 نصب نیست؛ در CubeIDE: Help → Manage Embedded Software Packages |
| `multiple definition of main` | فایل `main.c` خودکار CubeIDE با نسخه‌ی ما تداخل دارد؛ نسخه ما را نگه دارید |
| `'hspi2' undeclared` | SPI2 در `.ioc` فعال نشده؛ مرحله ۴.۳ را تکرار کنید |
| `'ff.h' file not found` | FatFS قرار داده نشده اما `SD_USE_FATFS=1` است؛ یا FatFS را اضافه کنید یا `=0` کنید |

---

## ۸. اتصالات سخت‌افزاری

### ۸.۱ Blue Pill (STM32F103C8T6)

```
3.3V    →  ریگولاتور خروجی پایدار 3.3V (150mA)
GND     →  زمین مشترک
PA0     →  مقسم ولتاژ باتری (تا حداکثر 3.3V پای ADC)
PA1     →  مقسم ولتاژ پنل خورشیدی
```

### ۸.۲ Air780E (USART3)

```
Air780 RXD   ←  Blue Pill PB10 (USART3_TX)
Air780 TXD   →  Blue Pill PB11 (USART3_RX)
Air780 PWRKEY ←  Blue Pill PA8
Air780 STATUS →  Blue Pill PA11
Air780 GND   →  GND مشترک
Air780 VBAT  →  3.3V–4.2V با ≥500mA
                (خازن 470µF حتماً نزدیک VBAT)
```

> **مهم:** Air780E هنگام attach شدن جریان لحظه‌ای ~۵۰۰mA می‌کشد. اگر از LDO ضعیف یا USB استفاده کنید، یا ماژول reset می‌شود یا اصلاً attach نمی‌شود. یک خازن **بزرگ (470µF–1000µF) نزدیک VBAT** الزامی است.

### ۸.۳ W25Q80 SPI Flash (SPI1)

```
W25Q80 /CS (1)   ←  PA4
W25Q80 DO  (2)   →  PA6 (MISO)
W25Q80 /WP (3)   ←  PB2 (یا مستقیم به VCC اگر گرافیکی نیست)
W25Q80 GND (4)   →  GND
W25Q80 DI  (5)   ←  PA7 (MOSI)
W25Q80 CLK (6)   ←  PA5 (SCK)
W25Q80 /HOLD(7)  ←  PB3 (یا مستقیم به VCC)
W25Q80 VCC (8)   →  3.3V با خازن 100nF
```

### ۸.۴ SD Card / microSD socket (SPI2)

```
SD /CS    (1)   ←  PB12  (10kΩ pull-up به 3.3V)
SD DI     (2)   ←  PB15 (MOSI) (10kΩ pull-up به 3.3V)
SD GND    (3,6) →  GND
SD VCC    (4)   →  3.3V با 10µF + 100nF
SD CLK    (5)   ←  PB13 (SCK)
SD DO     (7)   →  PB14 (MISO) (10kΩ pull-up به 3.3V)
```

> **حداقل یکی** از W25Q80 یا SD باید وصل باشد. اگر هر دو وصل باشند، فریمور هر دو را تشخیص می‌دهد و روی هر دو می‌نویسد.

### ۸.۵ Loop Multiplexer (CD4052)

```
Loop 0 osc → CD4052 IN0 (X0)
Loop 1 osc → CD4052 IN1 (X1)
Loop 2 osc → CD4052 IN2 (X2)
Loop 3 osc → CD4052 IN3 (X3)
CD4052 OUT (X) → PA2 (TIM2_CH3)
CD4052 A      ← PB0
CD4052 B      ← PB1
CD4052 INH    → GND (همیشه فعال)
CD4052 VEE    → GND
CD4052 VDD    → 3.3V
```

### ۸.۶ ST-Link V2 (برای flash)

```
ST-Link 3.3V    →  Blue Pill 3.3V
ST-Link GND     →  Blue Pill GND
ST-Link SWDIO   →  Blue Pill PA13 (DIO)
ST-Link SWCLK   →  Blue Pill PA14 (CLK)
```

### ۸.۷ USB-to-TTL برای debug log

```
TTL RX        ←  Blue Pill PA9  (TX)
TTL TX        →  Blue Pill PA10 (RX) [اختیاری]
TTL GND       →  GND مشترک
TTL 3.3V/5V   ×  وصل نکنید (Blue Pill خودش تغذیه دارد)
```

---

## ۹. Flash روی برد

### ۹.۱ اتصال ST-Link
ST-Link را به PC وصل کنید. در ویندوز "ST-Link Debug Driver" باید سبز باشد در Device Manager.

### ۹.۲ Run/Debug Configuration
در STM32CubeIDE:
```
Run → Debug Configurations → STM32 C/C++ Application
دو-کلیک روی RATCX1-STM32.elf
Debugger tab:
   Debug Probe: ST-LINK (ST-LINK GDB Server)
   Reset Behaviour: Connect under Reset (اگر مشکل اتصال داشت)
   Frequency: 4 MHz (پیش‌فرض)
```
Apply → Debug.

اگر همه چیز ok باشد، STM32CubeIDE می‌رود به فضای Debug، کد flash می‌شود و در `main()` متوقف می‌شود. F8 (Resume) بزنید.

### ۹.۳ Flash بدون debug
برای flash سریع (بدون توقف):
```
Run → Run As → STM32 C/C++ Application
```

---

## ۱۰. تست USART debug

### ۱۰.۱ باز کردن سریال
PuTTY یا minicom را با تنظیمات زیر باز کنید:
- COM port که USB-to-TTL را به آن وصل کرده‌اید (مثلاً COM3 یا /dev/ttyUSB0)
- Baud rate: `115200`
- Data bits: 8
- Stop bits: 1
- Parity: None
- Flow control: None

### ۱۰.۲ ریست برد و مشاهده‌ی boot log
دکمه‌ی RESET روی Blue Pill را بزنید. باید این متن‌ها را به‌ترتیب ببینید:

```
RATCX1-STM32 started
Calibrating loops...
Calibration done
Storage: W25Q80 ready          ← اگر فلش وصل است
Storage: SD card ready         ← اگر کارت وصل است
Air780 init...
Air780 ready
```

### ۱۰.۳ تفسیر خطاها

| پیام | معنی | چاره |
|------|------|------|
| `Storage: no backend (...)` | نه W25Q80 و نه SD پاسخ نداد | اتصالات SPI، تغذیه ۳.۳V، CS صحیح |
| `Air780 init failed` | ماژول روشن نشد یا attach نکرد | تغذیه‌ی ضعیف، APN غلط، SIM فعال نیست |
| LED PC13 خاموش | برد reset می‌خورد یا کلاک پیدا نمی‌کند | کریستال HSE 8MHz سالم است؟ |
| LED PC13 پیوسته روشن | کد در `Error_Handler()` گیر کرده | breakpoint بگذارید روی `Error_Handler` |

---

## ۱۱. تست هر backend به‌صورت ایزوله

### ۱۱.۱ تست فقط W25Q80

1. کارت SD را خارج کنید.
2. ریست بزنید.
3. باید فقط `Storage: W25Q80 ready` ببینید (نه `SD card ready`).
4. ۵ دقیقه صبر کنید (یا `INTERVAL_PERIOD` را موقتاً به `1` کاهش دهید).
5. باید `Interval ready` ببینید.
6. تأیید نوشتن:
   ```c
   /* اضافه کنید موقتاً به main.c بعد از Interval ready */
   uint8_t buf[280];
   w25q80_read(0x0000, buf, 280);
   /* اولین بایت باید 0xA5 باشد، بایت‌های 1..10 = timestamp */
   ```

### ۱۱.۲ تست فقط SD

1. W25Q80 را disconnect کنید (یا فقط /CS را HIGH ثابت کنید).
2. کارت SD را با FAT32 فرمت‌شده وصل کنید.
3. ریست بزنید.
4. باید فقط `Storage: SD card ready` ببینید.
5. بعد از ۵ دقیقه `Interval ready`.
6. کارت را در PC چک کنید:
   - **Raw mode (`SD_USE_FATFS=0`):** سکتور 1024 به بعد را با هگزدامپ ببینید. باید `A5` در شروع و timestamp ASCII ببینید.
   - **FatFS mode (`SD_USE_FATFS=1`):** پوشه‌ی `intervals/` و یک فایل `.dat` ۲۶۴ بایتی.

### ۱۱.۳ تست لوپ بدون شبکه

```c
/* در حلقه‌ی main اضافه کنید موقتاً */
char dbg[80];
snprintf(dbg, sizeof(dbg), "L0:%5d L1:%5d L2:%5d L3:%5d\r\n",
         dev[0], dev[1], dev[2], dev[3]);
HAL_UART_Transmit(&huart1, (uint8_t*)dbg, strlen(dbg), 100);
HAL_Delay(200);
```

با حرکت دست روی لوپ‌ها، مقدار `dev[i]` باید مثبت شود. اگر همیشه صفر است → آسیلاتور سیگنال نمی‌سازد یا مالتی‌پلکسر کار نمی‌کند.

---

## ۱۲. تست end-to-end با سرور

### ۱۲.۱ آماده‌سازی سرور
سرور TC Manager را روی پورت `2022` در حال listen قرار دهید. یا برای تست بدون سرور واقعی:

```bash
# روی PC
ncat -lk -p 2022
```

و IP این PC را در `config.h → SERVER_IP` بگذارید.

### ۱۲.۲ مشاهده handshake
بعد از boot و `Air780 ready`، دستگاه به سرور وصل می‌شود و این پیام را می‌فرستد:

```
8000<datetime>10001704RATCX1HW:B-06,SW:JA11-STM32READY
```

### ۱۲.۳ پاسخ ساعت از سرور
سرور باید پیام `0012` را با ساعت فعلی برگرداند. برای تست دستی با ncat:

```
0012 2025.05.07-14:30:00.0
```
(دقت کنید بدون newline ارسال شود.)

دستگاه پاسخ می‌دهد:
```
8012<datetime>10001704
```

### ۱۲.۴ درخواست داده interval
از سرور بفرستید:
```
0197 2505071425
```
(فرمت `YYMMDDHHmm` بدون فاصله)

دستگاه پاسخ:
```
8821<datetime><264-byte interval>
```

اگر آن interval هرگز ساخته نشده، رشته‌ی صفر برمی‌گرداند با همان timestamp.

### ۱۲.۵ مشاهده بازه‌ی واقعی
صبر کنید تا `current_time.minute % 5 == 0`. در آن لحظه دستگاه `Interval ready` چاپ می‌کند و رکورد را در storage می‌نویسد. بعد از ۵ دقیقه‌ی بعدی، اگر سرور `0197` با timestamp همان interval قبلی بفرستد، دستگاه دیتا را از storage برمی‌گرداند.

---

## مشکلات رایج و عیب‌یابی

| نشانه | احتمال | چاره |
|-------|--------|------|
| تمام LED ها خاموش | تغذیه ندارد | سنسور voltage روی 3V3 |
| فقط PC13 ۱Hz چشمک | کد طبیعی کار می‌کند ولی هیچ پیام UART نیست | TX/RX برعکس وصل شده |
| `Air780 init failed` | تغذیه‌ی ضعیف، APN، SIM | خازن 470µF نزدیک Air780، APN صحیح |
| مدام `Calibrating loops...` پشت سر هم | reset مدام (watchdog یا HardFault) | breakpoint روی `Error_Handler`، چک stack |
| dev[] همیشه ۰ | آسیلاتور لوپ کار نمی‌کند یا مالتی‌پلکسر | ابتدا با اسیلوسکوپ روی PA2 سیگنال square ببینید |
| سرور پیام دریافت نمی‌کند | فایروال یا APN | روی Air780 با AT+CIPOPEN دستی تست کنید |
| داده روی سرور خراب | byte order یا encoding | `interval_data` را قبل از send روی USART1 hex-dump کنید |

### مشاهده‌ی AT log Air780
برای دیباگ عمیق Air780، در `air780_tcp.c` این flag را موقتاً فعال کنید:

```c
#define AIR780_DEBUG  1  /* قبل از includes */
```

سپس همه‌ی AT commands و پاسخ‌ها روی USART1 echo می‌شوند.

---

## چک‌لیست نهایی

قبل از deploy روی محل واقعی:

- [ ] HSE 8MHz سالم (بدون آن firmware روی 8MHz اولیه می‌رود نه 72MHz)
- [ ] خازن 470µF نزدیک VBAT ماژول Air780
- [ ] خازن 100nF کنار VCC هر تراشه‌ی SPI
- [ ] Pull-up روی DAT0 (MISO) و DI (MOSI) و /CS کارت SD
- [ ] APN، SYSTEM_ID، SERVER_IP در `config.h` تنظیم شده
- [ ] LIMA-LIMITE برای کاربری ترافیک محل کالیبره شده
- [ ] LOOP_DISTANCE و LOOP_WIDTH طبق هندسه‌ی فیزیکی محل
- [ ] حداقل یکی از W25Q80 یا SD وصل و در boot log تأیید شده
- [ ] سرور TC Manager روی IP/PORT تنظیم‌شده listen می‌کند
- [ ] حداقل ۲ بازه‌ی متوالی روی سرور دریافت شده
- [ ] تست `0197` (درخواست بازه‌ی قدیمی) جواب درست داده

موفق باشید!
