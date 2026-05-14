# RATCX1-STM32 – نسخه SIM800L

این پوشه نسخه‌ای از firmware است که به‌جای Air780 4G LTE از **SIM800L GPRS** استفاده می‌کند.

## فایل‌های این پوشه (اختصاصی SIM800L)

| فایل | توضیح |
|------|-------|
| `Core/Inc/sim800l_tcp.h` | header driver SIM800L |
| `Core/Src/sim800l_tcp.c` | driver SIM800L (AT commands GPRS/TCP) |
| `Core/Inc/config.h` | تنظیمات خاص SIM800L (APN، baud) |
| `Core/Src/main.c` | main.c با sim800l_init و sim800l_task |

## فایل‌های مشترک (از stm32-firmware/Core/ کپی کنید)

```
classification.h / .c
interval.h / .c
loop_detector.h / .c
protocol.h / .c
sd_spi.h / .c
storage.h / .c
variables.h
w25q80.h / .c
w5500_tcp.h / .c   (فقط header برای type compatibility)
main.h
```

## تفاوت‌های کلیدی با نسخه Air780

| | Air780 | SIM800L |
|--|--------|---------|
| نوع شبکه | 4G LTE | 2G GPRS |
| Baud rate | 115200 | 9600 (auto-baud) |
| PWRKEY | idle LOW، پالس HIGH ≥600ms | idle HIGH، پالس LOW ≥1s |
| ثبت شبکه | AT+CEREG? | AT+CREG? |
| تنظیم APN | AT+CGDCONT | AT+CSTT |
| فعال‌سازی | AT+NETOPEN | AT+CIICR |
| اتصال TCP | AT+CIPOPEN=0,"TCP",... | AT+CIPSTART="TCP",... |
| تایید ارسال | +CIPSEND: 0,len | SEND OK |
| فرمت دریافت | +IPD: 0,len (جداگانه) | +IPD,len:data (inline) |

## نکته سخت‌افزاری مهم

SIM800L نیاز به **3.7–4.2V** دارد – از 3.3V مستقیم Blue Pill تغذیه نکنید.
از یک باتری LiPo یا رگولاتور 4V با ظرفیت جریان حداقل 2A استفاده کنید.

پین‌های USART3 یکسان است: PB10 (TX) و PB11 (RX).

## APN اپراتورهای ایران

| اپراتور | APN |
|---------|-----|
| ایران‌سل (MCI) | `mcinet` یا `mci` |
| همراه اول | `mtnirancell` |
| رایتل | `rtl` |
