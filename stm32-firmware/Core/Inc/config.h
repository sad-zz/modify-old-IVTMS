/**
 * @file config.h
 * @brief Device configuration for RATCX1 on STM32F103C8T6
 *
 * این فایل جایگزین تنظیمات EEPROM دستگاه اصلی (dsPIC30F4011) می‌شود.
 * تمام مقادیر قابل تنظیم در اینجا تعریف شده‌اند.
 *
 * Original device: dsPIC30F4011 (RATCX1)
 * Target device:   STM32F103C8T6 (Blue Pill) + Air780 4G LTE + W25Q80 Flash
 */

#ifndef CONFIG_H
#define CONFIG_H

/* ─── Device Identity ─────────────────────────────────────────────────── */
/* مقادیر زیر «پیش‌فرض کارخانه» هستند. اگر از طریق کنسول سریال
 * (USART1 = همان پورت debug، PA9/PA10، 115200 baud) مقداری ذخیره شده باشد،
 * هنگام boot از flash داخلی بارگذاری و جایگزین این‌ها می‌شود.
 * در PuTTY تایپ کنید: help  (برای فهرست دستورات)                          */
#define SYSTEM_ID       "14050218"        /* 8-char device ID (must match server) */
#define SYSTEM_MODEL    "RATCX1"          /* model string sent in handshake       */
#define FW_VERSION      "HW:B-06,SW:JA11-STM32" /* firmware version string       */

/* ─── Server Connection ───────────────────────────────────────────────── */
#define SERVER_IP       {5, 159, 49, 246}   /* TC Manager server IP (default)      */
#define SERVER_PORT     2022                /* TCP port                           */

/* ─── Air780 4G LTE settings ─────────────────────────────────────────── */
/* APN تنظیم اپراتور: ایران‌سل "mtnirancell"، همراه اول "mcinet"، رایتل "RighTel" */
#define AIR780_APN      "mtnirancell"       /* APN name – match your SIM operator */

/* TCP receive buffer (must match air780_tcp.c RX_DATA_SZ) */
#define TCP_RX_BUF      512

/* ─── Storage Backend ────────────────────────────────────────────────── */
/* در زمان boot هر دو W25Q80 (SPI1) و SD card (SPI2) به‌صورت auto-detect
 * چک می‌شوند. هر کدام در دسترس باشد فعال می‌شود.
 *
 *   SD_USE_FATFS = 0   →  کارت SD به‌صورت raw block استفاده می‌شود
 *                          (پیش‌فرض، بدون نیاز به library خارجی)
 *   SD_USE_FATFS = 1   →  کارت SD با فایل‌سیستم FAT32 (نیازمند ChaN FatFS؛
 *                          راهنمای نصب: Drivers/FatFS/README.md)
 */
#define SD_USE_FATFS    0

/* ─── Active Loops (1=enabled, 0=disabled) ───────────────────────────── */
/* Lane 1 uses loop 0 (far) + loop 1 (near) for bidirectional detection   */
/* Lane 2 uses loop 2 (far) + loop 3 (near)                               */
#define LOOP0_EN        1
#define LOOP1_EN        1
#define LOOP2_EN        1
#define LOOP3_EN        1

/* ─── Loop Geometry ──────────────────────────────────────────────────── */
#define LOOP_DISTANCE   200   /* distance between loop centres (mm)         */
#define LOOP_WIDTH      80    /* length of each loop in direction of travel (mm) */

/* ─── Detection Thresholds ───────────────────────────────────────────── */
/* Dev value = (caldata - freq_mean) / caldata * 10000                    */
/* Positive = frequency decreased = inductance increased = vehicle on loop */
#define MARGIN_TOP      200   /* threshold to declare vehicle ON loop  (0.01%) */
#define MARGIN_BOT      100   /* threshold to declare vehicle OFF loop (0.01%) */

/* ─── Vehicle Classification Length Limits (cm) ─────────────────────── */
/* X < LIMX < A < LIMA < B < LIMB < C < LIMC < D < LIMD < E < LIMITE < X */
#define LIMX   80    /* shorter than this → class X (unknown / motorcycle)  */
#define LIMA   150   /* A = motorcycle / moped (cm)                          */
#define LIMB   250   /* B = passenger car                                    */
#define LIMC   450   /* C = van / light truck                                */
#define LIMD   600   /* D = bus / medium truck                               */
#define LIMITE 1200  /* E = heavy truck / semi (above this → class X)        */

/* ─── Speed Violation Limits (km/h) ─────────────────────────────────── */
#define DSPEED1  80   /* day   speed limit for class A (motorbike)           */
#define NSPEED1  100  /* night speed limit for class A                       */
#define DSPEED2  80   /* day   speed limit for classes B-E                   */
#define NSPEED2  100  /* night speed limit for classes B-E                   */

/* Day = hours 06:00-21:00 */
#define DAY_HOUR_START  6
#define DAY_HOUR_END    21

/* ─── Interval Period ────────────────────────────────────────────────── */
#define INTERVAL_PERIOD 5     /* minutes per data interval (5 or 15)         */

/* ─── Gap / Headway Threshold ───────────────────────────────────────── */
#define GAP_DELAY_MS    2000  /* minimum gap between vehicles (ms)           */

/* ─── ADC Voltage Scaling ────────────────────────────────────────────── */
/* Battery: V = ADC * 0.388509 + 7  (matches original formula)             */
/* Solar:   V = ADC * 0.388509      (matches original formula)             */
#define VBAT_SCALE      0.388509f
#define VBAT_OFFSET     7.0f
#define VSOL_SCALE      0.388509f
#define VBAT_LOW_ADC    230   /* ADC value below which LBT_ERR is set       */
#define VSOL_LOW_ADC    200   /* solar low threshold                        */
#define VSOL_HIGH_ADC   250   /* solar recovered threshold                  */

/* ─── Power Supply Type ──────────────────────────────────────────────── */
#define POWER_TYPE_SOLAR   0
#define POWER_TYPE_NIGHT   1
#define POWER_TYPE_BACKUP  2
#define POWER_TYPE         POWER_TYPE_SOLAR

/* ─── Auto Calibration ───────────────────────────────────────────────── */
#define AUTCAL          1   /* 1 = auto-calibrate loop baseline continuously */

/* ─── STM32 Clock ────────────────────────────────────────────────────── */
#define SYSCLK_MHZ      72   /* must match SystemClock_Config()              */

/* ─── Timer Periods ──────────────────────────────────────────────────── */
/* TIM4: 1 ms tick  → PSC=71, ARR=999  @72MHz                             */
#define TIM4_PSC        71
#define TIM4_ARR        999

/* TIM2: free-running 1MHz counter for Input Capture                      */
/* PSC=71 gives 1µs resolution                                             */
#define TIM2_PSC        71

#endif /* CONFIG_H */
