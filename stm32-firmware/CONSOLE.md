# کنسول سریال — تنظیمات و مانیتورینگ از طریق USB/سریال

مثل دستگاه قبلی، حالا می‌توانید بدون rebuild، تنظیمات (IP سرور، پورت، APN،
کد دستگاه) را زنده عوض کنید و وضعیت دستگاه را ببینید — همه از طریق همان
پورت سریال debug.

## اتصال

پورت کنسول = **USART1** روی Blue Pill = **PA9 (TX)** و **PA10 (RX)**، با
سرعت **115200 baud, 8N1**.

با یک مبدل USB-to-TTL (3.3V):

| مبدل USB-TTL | Blue Pill |
|--------------|-----------|
| RX  | **PA9** |
| TX  | **PA10** |
| GND | **GND**  |

(اگر فقط می‌خواهید پیام‌ها را ببینید، کافی است RX مبدل به PA9 وصل شود.
برای تایپ دستور، TX مبدل هم باید به PA10 وصل باشد.)

در کامپیوتر: **PuTTY** (یا Tera Term / minicom) را روی COM port مبدل،
`Serial`, `115200`, `8`, `None`, `1`, Flow control `None` باز کنید.

## بعد از boot چه می‌بینید

```
RATCX1-STM32 started
Calibrating loops...
Calibration done
Storage: W25Q80 ready
Air780 init...
Air780 ready

=== RATCX1-STM32 console ===
type 'help' for commands, 'show' for config
Config:
  id   = 14050218
  ip   = 5.159.49.246
  port = 2022
  apn  = mtnirancell
  (firmware defaults: id=14050218 port=2022 apn=mtnirancell)
>
```

پشت `>` می‌توانید دستور تایپ کنید (با Enter اجرا می‌شود).

## دستورات

| دستور | کار |
|-------|-----|
| `help` | فهرست دستورات |
| `show` | نمایش تنظیمات ذخیره‌شده |
| `status` | وضعیت زنده: اتصال مودم، حالت لوپ‌ها، باتری/خورشیدی، خطاها، شمارنده‌ها، uptime |
| `loops` | مقادیر زنده‌ی فرکانس/کالیبراسیون هر ۴ لوپ |
| `set ip A.B.C.D` | تنظیم IP سرور |
| `set port N` | تنظیم پورت TCP |
| `set apn STR` | تنظیم APN سیم‌کارت |
| `set id STR` | تنظیم کد دستگاه (حداکثر ۱۵ کاراکتر) |
| `save` | نوشتن تنظیمات در flash داخلی (با reboot هم می‌ماند) |
| `default` | بازگشت به مقادیر پیش‌فرض فریمور (فقط در RAM؛ برای ماندگاری `save` بزنید) |
| `reboot` | ری‌ست نرم‌افزاری دستگاه |

## مثال — عوض کردن IP سرور

```
> set ip 5.159.49.246
ip = 5.159.49.246  (type 'save')
> set port 2022
port = 2022  (type 'save')
> set apn mtnirancell
apn = mtnirancell  (type 'save', then 'reboot')
> set id 14050218
id = 14050218  (type 'save')
> save
saved to flash. reboot to fully apply (modem/APN).
> reboot
rebooting...
```

نکته‌ها:
- تغییر **IP و پورت** بلافاصله در اتصال بعدی TCP اعمال می‌شود (نیازی به
  reboot نیست، ولی برای اطمینان reboot کنید).
- تغییر **APN** نیاز به **reboot** دارد (APN فقط موقع init مودم تنظیم می‌شود).
- اگر `save` نزنید، تغییرات بعد از قطع برق از بین می‌رود.

## مثال — مانیتورینگ

```
> status
Status:
  fw       = RATCX1 HW:B-06,SW:JA11-STM32
  uptime   = 412 s
  modem    = TCP connected
  server   = 5.159.49.246:2022
  onloop   = 0 0 0 0
  loop_en  = 1 1 1 1
  vbat_adc = 318   solar_adc = 240
  errors   = 0x0000
  time     = 2026.05.12-14:20:11.3
  totals L1: A=12 B=87 C=4 D=1 E=0 X=2
  totals L2: A=9 B=63 C=2 D=0 E=0 X=1

> loops
Loop  freq(us)   cal(us)   dev(x1e4)  on
 0        4521      4521          0   0
 1        4498      4498          0   0
 2        4510      4510          0   0
 3        4502      4502          0   0
```

## ذخیره‌سازی

تنظیمات در **صفحه‌ی آخر flash داخلی STM32** نوشته می‌شوند (۱KB در آدرس
`0x0800FC00`، خارج از محدوده‌ی کد که ~۴۵KB از ۶۴KB است). با یک magic +
CRC32 اعتبارسنجی می‌شوند؛ اگر معتبر نباشد (دستگاه نو یا flash پاک شده)،
مقادیر پیش‌فرض از `config.h` استفاده می‌شود.

پاک کردن کامل flash دستگاه (mass erase) تنظیمات ذخیره‌شده را هم پاک می‌کند
و دستگاه به پیش‌فرض‌ها برمی‌گردد.

## فایل آماده

`prebuilt/RATCX1-STM32.hex` و `.bin` با همین قابلیت و با پیش‌فرض‌های
`14050218 / 5.159.49.246:2022 / mtnirancell` ساخته شده‌اند — مستقیم با
STM32CubeProgrammer قابل flash هستند (راهنما: `BUILD_MAKEFILE.md`).
