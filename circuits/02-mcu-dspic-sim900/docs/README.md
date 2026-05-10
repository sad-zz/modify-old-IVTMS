# MCU-A — dsPIC30F4011 + SIM900/SIM800

طراحی legacy. dsPIC30F4011 با مودم 2G SIM900 (یا SIM800).

> ⚠ **هشدار**: شبکه‌های 2G در حال خاموش شدن در بسیاری کشورهاست. این variant فقط
> برای retrofit دستگاه‌های موجود استفاده بشه. برای پروژه‌های جدید **MCU-B** یا
> **MCU-C** پیشنهاد می‌شه.

## بلوک‌های اصلی
1. **dsPIC30F4011** (PDIP-40 یا TQFP-44) — 5V، حداکثر 30 MIPS
2. **مودم SIM900/SIM800** — 2G GSM/GPRS — تغذیهٔ 4.0V@2A peak
3. **Buck محلی 12V→4.0V**: MP1584 یا TPS5430 برای تأمین جریان مودم
4. **سطح‌سازی UART**: 5V (dsPIC) ↔ 2.8V (SIM) — با MOSFET dual یا SN74LVC1T45
5. **مدار SIM card holder + ESD**
6. **Crystal 7.3728 MHz** (برای baud rate دقیق UART) + 32.768kHz RTC
7. **Crystal 16 MHz** برای dsPIC + PLL × 8 = 128 MHz / 4 = 32 MIPS (نزدیک max)
8. **DS1305 RTC** (SPI، با باتری backup)
9. **MMC/SD socket** (اختیاری، برای لاگ گسترده)
10. **هدر ICSP** (PGC/PGD/MCLR/VCC/GND) — برای برنامه‌ریزی PIC

## مهم: pin assignment dsPIC30F4011
بر اساس firmware موجود (`91-7.c`, `Variables.h`):

| Pin (TQFP44) | Net           | Function                    |
|--------------|---------------|-----------------------------|
| 1   (RB6)    | MDM_STAT      | input — وضعیت مودم          |
| 25  (RE0)    | onloop0       | output — LED loop1          |
| 26  (RE1)    | onloop1       | output — LED loop2          |
| 27  (RE2)    | onloop2       | output — LED loop3          |
| 28  (RE3)    | onloop3       | output — LED loop4          |
| 29  (RE4)    | STATUS_LED    | output                      |
| 30  (RE5)    | CONN_STATE    | output (LED اتصال)          |
| 17  (RF0)    | RTC_CS        | output — chip select RTC    |
| 18  (RF1)    | MMC_CS        | output — chip select SD     |
| 31  (RB2)    | MODEM_PWR     | output — Vbat enable        |
| 4   (RB5)    | MMC_ERROR     | output (LED خطا)            |
| 6   (RB7)    | PWRKEY        | output — pulse 1s فعال‌سازی مودم |
| 7   (RB8)    | CHARGE_CTRL   | output                      |
| 32  (U1RX)   | از مودم      | input — RX                  |
| 33  (U1TX)   | به مودم      | output — TX                 |
| 21  (U2RX)   | اختیاری PC   | input                       |
| 22  (U2TX)   | اختیاری PC   | output                      |
| 23  (SCK1)   | SPI clock     | به RTC و SD                 |
| 24  (SDI1)   | SPI MISO      |                             |
| 19  (SDO1)   | SPI MOSI      |                             |
| 13  (AN0)    | LOOP_IO_COM_U51 | analog از LOOP backplane  |
| 14  (AN1)    | LOOP_IO_COM_U52 | analog از LOOP backplane  |
| 15  (AN2)    | CMP_OUT       | digital                     |
| 16  (RC13)   | MUX_A         | output                      |
| 9   (RC14)   | MUX_B         | output                      |
| 8   (RC15)   | MUX_C         | output                      |
| 10  (RD0)    | MUX_INH       | output                      |
| 11  (MCLR)   | reset/ICSP    |                             |
| 38  (PGC)    | ICSP clock    |                             |
| 39  (PGD)    | ICSP data     |                             |
| 40  (VDD)    | +5V           |                             |
| 12  (VSS)    | GND           |                             |
| 35  (AVDD)   | +5V (filtered via FB+100nF)|                |
| 36  (AVSS)   | GND          |                             |

## ابعاد PCB پیشنهادی
80 × 100 mm، 2-layer
- مودم در یک طرف (آنتن لب برد)
- dsPIC وسط
- SIM holder و SD لب راست
- ICSP در گوشه
