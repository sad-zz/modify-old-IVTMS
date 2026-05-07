# Air780 4G LTE Module – راهنمای اتصال و راه‌اندازی

## معرفی

**Air780E** (سری EC618، شرکت LuatOS) یک ماژول 4G LTE Cat.1 با پشتیبانی از دستورات AT استاندارد است.

| ویژگی | مقدار |
|-------|-------|
| استاندارد | LTE Cat.1 |
| باند (ایران) | B1, B3, B8 (ایران‌سل / ایرانسل / رایتل) |
| رابط | UART (AT commands) |
| ولتاژ | 3.3V – 4.2V |
| جریان بیشینه | ~500mA (حین اتصال) |
| دما | -40°C تا +85°C |
| بسته | LCC+LGA 23.6×23.0×2.2mm |

---

## اتصال سخت‌افزاری به STM32F103C8T6

```
STM32F103        Air780
─────────────────────────────────────────
PB10 (USART3_TX) → RXD
PB11 (USART3_RX) ← TXD
PB12  (GPIO Out) → PWRKEY
PB13  (GPIO In)  ← STATUS
GND              → GND
3.3V / 4.0V      → VCC  (توصیه: تغذیه مستقل 500mA با خازن 100µF)
```

> ⚠️ **نکته برق:** جریان لحظه‌ای Air780 هنگام اتصال تا 500mA می‌رسد.
> اگر Blue Pill را از USB تغذیه می‌کنید، حتماً یک منبع جریان مستقل برای Air780 تهیه کنید.

---

## راه‌اندازی ماژول (PWRKEY)

1. **روشن کردن:** PWRKEY را حداقل ۶۰۰ms به HIGH ببرید، سپس LOW کنید.
2. **STATUS:** بعد از ~3-5 ثانیه پین STATUS به HIGH می‌رود (ماژول بوت شده).
3. **خاموش کردن:** PWRKEY را ۲۵۰۰ms به HIGH ببرید.

این رفتار در `air780_tcp.c → air780_power_on()` پیاده‌سازی شده است.

---

## تنظیم APN

در `Core/Inc/config.h`:

```c
// ایران‌سل
#define AIR780_APN  "mtnirancell"

// ایرانسل (MCI / Hamrahe Aval)
#define AIR780_APN  "mcinet"

// رایتل
#define AIR780_APN  "internet"
```

---

## دستورات AT اصلی (سری EC618)

| دستور | توضیح |
|-------|-------|
| `AT` | بررسی ارتباط |
| `ATE0` | غیرفعال کردن echo |
| `AT+CIMI` | خواندن IMSI (سیم‌کارت موجود است؟) |
| `AT+CEREG?` | وضعیت ثبت شبکه LTE |
| `AT+CGDCONT=1,"IP","APN"` | تنظیم APN |
| `AT+NETOPEN` | باز کردن اتصال شبکه |
| `AT+CIPOPEN=0,"TCP","IP",PORT` | باز کردن سوکت TCP |
| `AT+CIPSEND=0,LEN` | ارسال داده (سپس `>` → داده → پاسخ) |
| `AT+CIPCLOSE=0` | بستن سوکت |
| `AT+NETCLOSE` | بستن اتصال شبکه |

---

## URC های مهم (Unsolicited Result Codes)

| URC | معنی |
|-----|------|
| `+NETOPEN: 0` | شبکه با موفقیت باز شد |
| `+CIPOPEN: 0,0` | سوکت ۰ با موفقیت متصل شد |
| `+IPD: 0,LEN` | LEN بایت داده دریافتی در سوکت ۰ |
| `+CIPEVENT: 0,CLOSED` | اتصال TCP قطع شد |
| `+CIPEVENT: 0,CONNECTED` | اتصال TCP برقرار شد |

---

## تست با Telnet / Serial Terminal

```bash
# تست سریع با minicom یا PuTTY (115200 baud)
AT
# پاسخ: OK

AT+CIMI
# پاسخ: 432XXXXXXXXXX (IMSI)

AT+CEREG?
# پاسخ: +CEREG: 0,1  (1=ثبت شده)

AT+NETOPEN
# پاسخ: +NETOPEN: 0  (0=موفق)

AT+CIPOPEN=0,"TCP","192.168.1.100",2022
# پاسخ: +CIPOPEN: 0,0
```

---

## منابع

- [Air780E AT Command Manual](https://doc.openluat.com/wiki/37)
- [Air780E Hardware Design Guide](https://doc.openluat.com/wiki/37)
- [LuatOS GitHub](https://github.com/openLuat)
