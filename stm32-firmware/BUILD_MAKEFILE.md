# ساخت و پروگرام بدون STM32CubeIDE (روش Makefile)

اگر CubeIDE اذیت می‌کند، اصلاً لازمش ندارید. این ریپو حالا یک
`Makefile` کامل + HAL drivers + CMSIS + startup + linker script دارد و
با `arm-none-eabi-gcc` به‌تنهایی build می‌شود. حتی یک باینری از پیش
ساخته‌شده در پوشه‌ی `prebuilt/` هست که می‌توانید مستقیم flash کنید.

---

## گزینه‌ی الف — فقط می‌خواهم flash کنم (بدون build)

فایل آماده در ریپو هست:

```
stm32-firmware/prebuilt/RATCX1-STM32.hex
stm32-firmware/prebuilt/RATCX1-STM32.bin
```

> ⚠️ این باینری با مقادیر پیش‌فرض `config.h` ساخته شده
> (`SYSTEM_ID="10001704"`, `SERVER_IP={192,168,1,100}`, `APN="mtnirancell"`).
> اگر این‌ها برای شما فرق دارد، گزینه‌ی ب را برای build مجدد ببینید.

### پروگرام با STM32CubeProgrammer (ساده‌ترین)

1. دانلود: https://www.st.com/en/development-tools/stm32cubeprogrammer.html (رایگان)
2. ST-Link V2 را وصل کنید:
   | ST-Link | Blue Pill |
   |---------|-----------|
   | 3.3V    | 3.3V      |
   | GND     | GND       |
   | SWDIO   | PA13      |
   | SWCLK   | PA14      |
3. CubeProgrammer را باز کنید → سمت راست بالا `ST-LINK` → **Connect**
4. تب **Erasing & programming** (آیکون دانلود سمت چپ):
   - **File path:** `...\stm32-firmware\prebuilt\RATCX1-STM32.hex`
   - ✓ Verify programming
   - ✓ Run after programming
5. **Start Programming**
6. منتظر `Download verified successfully` بمانید
7. LED روی PC13 باید 1Hz چشمک بزند → موفق

### یا با st-flash (لینوکس/مک، از پکیج `stlink-tools`)

```bash
sudo apt install stlink-tools         # یا: brew install stlink
st-flash --reset write stm32-firmware/prebuilt/RATCX1-STM32.bin 0x08000000
```

---

## گزینه‌ی ب — build از سورس (وقتی config.h را تغییر داده‌اید)

### نصب toolchain

| سیستم | دستور |
|-------|------|
| Ubuntu/Debian | `sudo apt install gcc-arm-none-eabi make` |
| Fedora | `sudo dnf install arm-none-eabi-gcc-cs arm-none-eabi-newlib make` |
| macOS | `brew install --cask gcc-arm-embedded` و `brew install make` |
| Windows | [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) را نصب کنید (نسخه‌ی `arm-none-eabi`)، بعد در PATH اضافه کنید. `make` را از [MSYS2](https://www.msys2.org/) یا `choco install make` بگیرید. |

بررسی: `arm-none-eabi-gcc --version` باید نسخه نشان دهد.

### تنظیم config.h

`stm32-firmware/Core/Inc/config.h` را باز و ویرایش کنید:

```c
#define SYSTEM_ID    "10001704"            // ID ثبت‌شده روی سرور
#define SERVER_IP    {192, 168, 1, 100}    // IP سرور TC Manager
#define AIR780_APN   "mtnirancell"         // APN سیم‌کارت
```

### build

```bash
cd stm32-firmware
make            # خروجی در build/  →  .elf  .hex  .bin
```

خروجی موفق:
```
   text    data     bss     dec     hex  filename
  39640     148    5276   45064   b008  build/RATCX1-STM32.elf
```
(warningهای `_close/_lseek/_read/_write is not implemented` عادی‌اند و بی‌ضرر.)

### flash

```bash
make flash      # نیاز به st-flash از پکیج stlink-tools
```
یا فایل `build/RATCX1-STM32.hex` را با STM32CubeProgrammer بنویسید (مراحل بالا).

### پاک‌سازی

```bash
make clean
```

---

## ساختار فایل‌ها

```
stm32-firmware/
├── Makefile                       ← همه‌چیز اینجا تنظیم شده
├── STM32F103C8TX_FLASH.ld         ← linker script (64KB flash, 20KB RAM)
├── Core/
│   ├── Inc/                       ← هدرهای برنامه + stm32f1xx_hal_conf.h
│   └── Src/                       ← main.c و ماژول‌های برنامه
├── Drivers/
│   ├── STM32F1xx_HAL_Driver/      ← HAL درایور رسمی ST (BSD-3-Clause)
│   └── CMSIS/
│       ├── Include/               ← core_cm3.h و هدرهای CMSIS
│       └── Device/ST/STM32F1xx/   ← هدر دستگاه + startup + system_stm32f1xx.c
├── build/                         ← خروجی build (در .gitignore)
└── prebuilt/                      ← باینری آماده برای flash مستقیم
```

نکته: درایور قدیمی `Core/Src/w5500_tcp.c` در build شامل **نمی‌شود**
(با ماژول Air780 جایگزین شده). در `Makefile` فهرست `C_SOURCES` را
ببینید اگر می‌خواهید چیزی اضافه/کم کنید.

---

## استفاده از Makefile داخل STM32CubeIDE (اختیاری)

اگر باز هم می‌خواهید از CubeIDE برای debug استفاده کنید ولی build را
به Makefile بسپارید:

```
File → New → Makefile Project with Existing Code
  Existing Code Location: <مسیر>/stm32-firmware
  Toolchain: MCU ARM GCC   (یا Cross GCC)
  Finish
```
بعد `Project → Build`. برای debug یک Debug Configuration از نوع
`STM32 C/C++ Application` بسازید و در تب Main فایل `build/RATCX1-STM32.elf`
را انتخاب کنید.

ولی برای فقط flash کردن، به CubeIDE نیازی نیست — STM32CubeProgrammer کافی است.
