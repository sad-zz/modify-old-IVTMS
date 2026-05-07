# W5500 Ethernet Driver

> ⚠️ **منسوخ شده (Deprecated):** این ماژول در سخت‌افزار فعلی استفاده نمی‌شود.
> ارتباط شبکه به **Air780 4G LTE** منتقل شده است (به `Drivers/Air780/README.md` مراجعه کنید).
> این فایل صرفاً برای مرجع تاریخی نگه داشته شده است.

---

## دریافت کتابخانه WIZnet ioLibrary

برای کار با چیپ W5500 به کتابخانه رسمی WIZnet نیاز دارید.

### مرحله ۱ – دانلود
```bash
git clone https://github.com/Wiznet/ioLibrary_Driver.git
```

### مرحله ۲ – کپی فایل‌های لازم
فایل‌های زیر را از کتابخانه دانلود‌شده به پوشه `Drivers/W5500/` کپی کنید:

```
ioLibrary_Driver/
├── Ethernet/
│   ├── wizchip_conf.h    → Drivers/W5500/wizchip_conf.h
│   ├── wizchip_conf.c    → Drivers/W5500/wizchip_conf.c
│   ├── socket.h          → Drivers/W5500/socket.h
│   ├── socket.c          → Drivers/W5500/socket.c
│   └── W5500/
│       ├── w5500.h       → Drivers/W5500/w5500.h
│       └── w5500.c       → Drivers/W5500/w5500.c
```

### مرحله ۳ – فعال‌سازی در w5500_tcp.c
بعد از کپی فایل‌ها، در `Core/Src/w5500_tcp.c`:
1. قسمت include را از comment خارج کنید
2. قسمت‌های `/* uncomment after adding library */` را از comment خارج کنید

### مرحله ۴ – تنظیم IP آدرس
در `Core/Inc/w5500_tcp.h` مقادیر زیر را متناسب با شبکه خود تنظیم کنید:
```c
#define W5500_IP    {192, 168, 1, 200}   // آدرس IP دستگاه
#define W5500_GW    {192, 168, 1, 1}     // gateway
#define SERVER_IP   {192, 168, 1, 100}   // آدرس سرور TC Manager (در config.h)
```

---

## Hardware Connections

| W5500 Pin | STM32F103 Pin | Description          |
|-----------|---------------|----------------------|
| SCLK      | PA5           | SPI1 clock           |
| MISO      | PA6           | SPI1 MISO            |
| MOSI      | PA7           | SPI1 MOSI            |
| /CS       | PA4           | Chip select (GPIO)   |
| /RST      | PB2           | Hardware reset       |
| INT       | PB3           | Interrupt (optional) |
| 3.3V      | 3.3V          | Power                |
| GND       | GND           | Ground               |

**Note:** W5500 operates at 3.3V – compatible with STM32F103 Blue Pill directly.
