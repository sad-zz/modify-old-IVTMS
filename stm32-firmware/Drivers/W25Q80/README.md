# W25Q80 8Mbit SPI NOR Flash – راهنمای اتصال و استفاده

## معرفی

**W25Q80DV** (شرکت Winbond) یک حافظه فلش SPI NOR با ظرفیت ۸ مگابیت (۱ مگابایت) است.
در این پروژه برای ذخیره داده‌های بازه‌ای ۵ دقیقه‌ای به‌صورت غیر فرار (non-volatile) استفاده می‌شود.

| ویژگی | مقدار |
|-------|-------|
| ظرفیت | 8 Mbit = 1 MB |
| رابط | SPI (Mode 0 و Mode 3) |
| ولتاژ | 2.7V – 3.6V |
| حداکثر فرکانس SPI | 80 MHz |
| اندازه صفحه | 256 بایت |
| اندازه سکتور | 4 KB |
| اندازه بلوک | 32 KB / 64 KB |
| زمان پاکسازی سکتور | 60ms (معمولی) / 400ms (حداکثر) |
| دما | -40°C تا +85°C |
| JEDEC ID | 0xEF 0x40 0x14 |

---

## اتصال سخت‌افزاری به STM32F103C8T6

```
STM32F103       W25Q80DV   پین روی W25Q80
──────────────────────────────────────────
PA4  (GPIO Out) → /CS       پین ۱
PA5  (SPI1_SCK) → CLK       پین ۶
PA7  (SPI1_MOSI)→ DI        پین ۵
PA6  (SPI1_MISO)← DO        پین ۲
PB2  (GPIO Out) → /WP       پین ۳   (به VCC متصل کنید یا HIGH نگه دارید)
PB3  (GPIO Out) → /HOLD     پین ۷   (به VCC متصل کنید یا HIGH نگه دارید)
3.3V            → VCC       پین ۸
GND             → GND       پین ۴
```

**خازن‌های دکوپلینگ:**
- ۱۰۰nF بین VCC و GND نزدیک به IC اضافه کنید.

---

## طرح ذخیره داده

هر بازه ۵ دقیقه‌ای یک رشته ۲۶۴ بایتی تولید می‌کند (فرمت ۸۸۲۱ پروتکل RATCX1).

| آدرس | محتوا |
|------|-------|
| `0x000000` – `0x07FFFF` | ۱۲۸ اسلات بازه‌ای (هر اسلات ۴KB) |
| `0x080000` – `0x0FFFFF` | رزرو برای پیکربندی آینده |

**ساختار هر اسلات (در اولین ۲۷۴ بایت از ۴KB سکتور):**

```
[0]      Magic byte (0xA5) – نشانه اعتبار رکورد
[1..10]  Timestamp "YYMMDDHHmm" (10 کاراکتر)
[11..274] داده‌های بازه ۲۶۴ بایت (فرمت 8821)
```

**محاسبه آدرس اسلات از timestamp:**
```
slot = (DD×24×12 + HH×12 + mm÷5) % 128
addr = slot × 4096
```

پوشش زمانی: ۱۲۸ × ۵ دقیقه = **۱۰.۷ ساعت** ذخیره آفلاین.

---

## تأیید کارکرد با JEDEC ID

```c
// انتظار می‌رود:
// mfr  = 0xEF  (Winbond)
// type = 0x40  (SPI NOR)
// cap  = 0x14  (8 Mbit)
int ret = w25q80_init();
// ret == 0 → فلش OK
// ret != 0 → فلش پیدا نشد یا اشتباه است
```

---

## تست سریع (اسنیپت C)

```c
uint8_t buf[16];
// نوشتن
w25q80_sector_erase(0x000000);
w25q80_page_program(0x000000, (uint8_t *)"Hello W25Q80!\r\n", 15);
// خواندن
w25q80_read(0x000000, buf, 15);
// buf باید شامل "Hello W25Q80!\r\n" باشد
```

---

## منابع

- [W25Q80DV Datasheet (Winbond)](https://www.winbond.com/hq/product/code-storage-flash-memory/serial-nor-flash/?pn=W25Q80DV)
- [STM32 SPI HAL Reference](https://www.st.com/en/embedded-software/stm32cube-mcu-mpu-packages.html)
