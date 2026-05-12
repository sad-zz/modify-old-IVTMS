/**
 * @file console.h
 * @brief Serial console + runtime configuration store (USART1 / debug port)
 *
 * هنگام boot، config_load() تنظیمات را از صفحه‌ی آخر flash داخلی STM32
 * می‌خواند. اگر معتبر نباشد (دستگاه نو، یا هنوز چیزی ذخیره نشده) مقادیر
 * پیش‌فرض از config.h استفاده می‌شوند.
 *
 * console_task() که از حلقه‌ی اصلی فراخوانده می‌شود، روی USART1 (PA9/PA10،
 * 115200 baud) دستورات متنی را می‌خواند. دستورات:
 *
 *   help                 – فهرست دستورات
 *   show                 – نمایش تنظیمات فعلی
 *   status               – وضعیت زنده (اتصال، لوپ‌ها، باتری، خطاها)
 *   loops                – مقادیر زنده‌ی فرکانس/کالیبراسیون لوپ‌ها
 *   set ip A.B.C.D       – تنظیم IP سرور
 *   set port N           – تنظیم پورت TCP
 *   set apn STRING       – تنظیم APN سیم‌کارت
 *   set id STRING        – تنظیم کد دستگاه (۸ کاراکتر)
 *   save                 – ذخیره در flash
 *   default              – بازگشت به مقادیر پیش‌فرض config.h (در RAM)
 *   reboot               – ری‌ست نرم‌افزاری دستگاه
 */

#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdint.h>

/* ─── Runtime configuration (loaded from flash, falls back to config.h) ── */
typedef struct {
    uint32_t magic;          /* CFG_MAGIC when valid                       */
    char     system_id[16];  /* device ID string                          */
    uint8_t  server_ip[4];   /* TC Manager server IP                       */
    uint16_t server_port;    /* TCP port                                   */
    uint16_t _pad;
    char     apn[40];        /* SIM APN                                    */
    uint32_t crc;            /* checksum of everything above               */
} app_cfg_t;

extern app_cfg_t g_cfg;

/* Load config from internal flash; if invalid, fill from config.h defaults */
void config_load(void);
/* Reset g_cfg to config.h defaults (RAM only – call save to persist)       */
void config_defaults(void);
/* Erase + program the last flash page with g_cfg. Returns 0 on success.    */
int  config_save(void);

/* ─── Console ────────────────────────────────────────────────────────── */
void console_init(void);   /* prints banner; call after USART1 init        */
void console_task(void);   /* poll USART1 RX, parse + execute commands      */

#endif /* CONSOLE_H */
