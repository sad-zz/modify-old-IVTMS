# MCU-B — dsPIC30F4011 + AIR780E + W25Q80

dsPIC30F4011 (5V) + ماژول 4G AIR780E + W25Q80 SPI flash (1MB).

## چرا این variant
- AIR780E = LTE Cat.1bis (Air780E از Air724UG/EC200 ارزون‌تره و سازگار با SIM900 از نظر AT)
- جایگزین مدرن برای SIM900 وقتی شبکهٔ 2G خاموش شده
- W25Q80 جای MMC را برای لاگ سریع می‌گیره (بدون filesystem پیچیده)

## بلوک‌ها
1. **dsPIC30F4011** (TQFP-44) — 5V، همان firmware با تغییر AT command set
2. **AIR780E** — ماژول 4G، Vbat 3.4-4.2V (typical 3.8V)، logic 1.8V (با pad 3.0V هم)
3. **W25Q80** (8Mb / 1MB) — SPI flash، 2.7-3.6V
4. **Buck 12V → 3.8V** برای AIR780E Vbat (MP1584 یا TPS563200)
5. **LDO 5V → 3.3V** برای W25Q80 و سطح‌های 3.3V (AMS1117-3.3 کافی - چند mA)
6. **Level shifter** UART (5V dsPIC ↔ 1.8V AIR780E) — TXB0104 یا 4×SN74LVC1T45
7. **Level shifter** SPI (5V dsPIC ↔ 3.3V W25Q80) — راه ارزون: مقاومت divider (250kHz آرام) یا 74LVC125
8. **DS1305 RTC** + باتری backup (همان MCU-A)
9. **ICSP header**

## Pin assignment dsPIC30F4011 (همانند MCU-A با تغییرات)
بقیهٔ پین‌ها مشابه MCU-A. تنها تفاوت‌ها:
- اضافه: `FLASH_CS` روی `RD1` (پین 9 TQFP-44)
- اضافه: `AIR_RESET` روی `RD2`
- بقیه عیناً همان.

| Pin (TQFP44) | Net          | Function                     |
|--------------|--------------|------------------------------|
| 9 (RD1)      | FLASH_CS     | W25Q80 chip select           |
| 10 (RD2)     | AIR_RESET    | AIR780E reset (active low)   |
| سایر         | (همان MCU-A) |                              |

## ابعاد PCB
80 × 100 mm، 2-layer
