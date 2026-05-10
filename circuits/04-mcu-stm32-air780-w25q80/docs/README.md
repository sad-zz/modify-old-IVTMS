# MCU-C — STM32F103C8 + AIR780E + W25Q80

variant مدرن: STM32F103C8 (Cortex-M3 72MHz) + AIR780E + W25Q80 + SD card.
این طراحی با firmware موجود در `/stm32-firmware/` یکی به یک منطبق شده.

## مزیت
- ۳.۳V native ➜ نیازی به سطح‌ساز ندارد (به جز سمت loop 5V)
- ۲ SPI داخلی (W25Q80 روی SPI1، SD روی SPI2)
- ۳ UART (debug، AIR780، spare)
- USB device برای DFU/CDC
- Cortex-M3 و ابزار CubeIDE/PlatformIO

## Pin assignment STM32F103C8 (مطابق firmware موجود)
از `stm32-firmware/Core/Src/main.c`:

| Pin | Function | Net |
|-----|----------|-----|
| PA0 | ADC1_IN0 | VBAT_SENSE (تقسیم 1:11 از +12V) |
| PA1 | ADC1_IN1 | SOLAR_SENSE (اختیاری) |
| PA2 | TIM2_CH3 | LOOP_IN (input capture از MUX خروجی) |
| PA4 | GPIO out | FLASH_CS (W25Q80 /CS) |
| PA5 | SPI1_SCK | FLASH_SCK |
| PA6 | SPI1_MISO| FLASH_MISO |
| PA7 | SPI1_MOSI| FLASH_MOSI |
| PA8 | GPIO out | AIR_PWRKEY |
| PA9 | USART1_TX| DEBUG_TX (115200 baud) |
| PA10| USART1_RX| DEBUG_RX |
| PA11| GPIO in  | AIR_STATUS |
| PB0 | GPIO out | MUX_A → backplane J_BUS.8 |
| PB1 | GPIO out | MUX_B → backplane J_BUS.9 |
| PB2 | GPIO out | FLASH_WP (HIGH=disable WP) |
| PB3 | GPIO out | FLASH_HOLD |
| PB5 | GPIO out | LED_ONLOOP0 |
| PB6 | GPIO out | LED_ONLOOP1 |
| PB7 | GPIO out | LED_ONLOOP2 |
| PB8 | GPIO out | LED_ONLOOP3 |
| PB9 | GPIO out | LED_CONN_STATE |
| PB10| USART3_TX| AIR780_RXD |
| PB11| USART3_RX| AIR780_TXD |
| PB12| GPIO out | SD_CS |
| PB13| SPI2_SCK | SD_SCK |
| PB14| SPI2_MISO| SD_MISO |
| PB15| SPI2_MOSI| SD_MOSI |
| PC13| GPIO out | LED_HEARTBEAT (built-in) |
| PC14| OSC32_IN | RTC crystal 32.768kHz |
| PC15| OSC32_OUT|                       |
| PD0 | OSC_IN   | 8MHz HSE crystal |
| PD1 | OSC_OUT  |                  |
| BOOT0| GPIO    | DFU select (with switch/jumper) |
| NRST | reset   | + tactile button |

> **توجه**: backplane فقط 2 خط MUX (A, B) رو نیاز داره چون STM32 firmware از 4-channel استفاده می‌کنه (2 bit انتخاب). در BACKPLANE.md، MUX_C از J_BUS.10 رو هم به PB4 اختصاص می‌دیم اگر extension لازم بشه.

## بلوک‌های اصلی
1. **STM32F103C8T6** (LQFP-48) — 3.3V، 72MHz، 64KB Flash، 20KB RAM
2. **AIR780E** — همان MCU-B
3. **W25Q80** — همان MCU-B، روی SPI1
4. **microSD** — روی SPI2
5. **Buck 12V→3.8V** برای AIR780E Vbat (همان MP1584)
6. **LDO 5V→3.3V** برای STM32 + flash + SD (AMS1117-3.3 یا حتی LP5907 (low-noise))
7. **Crystal 8MHz HSE** + **Crystal 32.768kHz LSE** (RTC داخلی)
8. **USB-C connector** برای DFU + CDC debug
9. **SWD header** (4 پین: SWDIO/SWCLK/GND/3V3)

## ابعاد PCB
80 × 80 mm (compact)، 2-layer

## Bootloader / DFU
- BOOT0 jumper: NORMAL (GND) / DFU (3V3)
- USB-C: VBUS با حفاظت از 3V3 LDO جدا (USB power فقط اگر بدون 12V باشه)
