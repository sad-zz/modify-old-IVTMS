/**
 * @file config.h  [نسخه SIM800L]
 * @brief تنظیمات دستگاه RATCX1 روی STM32F103C8T6 با SIM800L GPRS
 *
 * این فایل فقط برای پوشه stm32-firmware-sim800l/ است.
 * بقیه فایل‌ها (classification، interval، protocol، storage ...) از
 * پوشه stm32-firmware/Core/ مشترک هستند – کپی کنید یا symlink بزنید.
 *
 * ─── تفاوت با نسخه Air780 ────────────────────────────────────────────
 *  - SIM800L_APN به‌جای AIR780_APN
 *  - نرخ باد 9600 به‌جای 115200 (auto-baud SIM800L)
 *  - سایر پارامترها یکسان هستند
 */

#ifndef CONFIG_H
#define CONFIG_H

/* ─── Device Identity ─────────────────────────────────────────────────── */
#define SYSTEM_ID       "10001704"
#define SYSTEM_MODEL    "RATCX1"
#define FW_VERSION      "HW:B-06,SW:JA11-STM32-SIM800L"

/* ─── Server Connection ───────────────────────────────────────────────── */
#define SERVER_IP       {192, 168, 1, 100}
#define SERVER_PORT     2022

/* ─── SIM800L GPRS settings ───────────────────────────────────────────── */
/* APN اپراتور: ایران‌سل "mcinet"، همراه اول "mci"، رایتل "rtl"           */
#define SIM800L_APN     "mcinet"

/* بافر دریافت TCP */
#define TCP_RX_BUF      512

/* ─── Storage Backend ────────────────────────────────────────────────── */
#define SD_USE_FATFS    0

/* ─── Active Loops ───────────────────────────────────────────────────── */
#define LOOP0_EN        1
#define LOOP1_EN        1
#define LOOP2_EN        1
#define LOOP3_EN        1

/* ─── Loop Geometry ──────────────────────────────────────────────────── */
#define LOOP_DISTANCE   200
#define LOOP_WIDTH      80

/* ─── Detection Thresholds ───────────────────────────────────────────── */
#define MARGIN_TOP      200
#define MARGIN_BOT      100

/* ─── Vehicle Classification Length Limits (cm) ─────────────────────── */
#define LIMX    80
#define LIMA   150
#define LIMB   250
#define LIMC   450
#define LIMD   600
#define LIMITE 1200

/* ─── Speed Limits (km/h) ────────────────────────────────────────────── */
#define DSPEED1  80
#define NSPEED1 100
#define DSPEED2  80
#define NSPEED2 100

#define DAY_HOUR_START  6
#define DAY_HOUR_END   21

/* ─── Interval ───────────────────────────────────────────────────────── */
#define INTERVAL_PERIOD 5

/* ─── Gap Threshold ──────────────────────────────────────────────────── */
#define GAP_DELAY_MS    2000

/* ─── ADC ────────────────────────────────────────────────────────────── */
#define VBAT_SCALE      0.388509f
#define VBAT_OFFSET     7.0f
#define VSOL_SCALE      0.388509f
#define VBAT_LOW_ADC    230
#define VSOL_LOW_ADC    200
#define VSOL_HIGH_ADC   250

/* ─── Power ──────────────────────────────────────────────────────────── */
#define POWER_TYPE_SOLAR   0
#define POWER_TYPE_NIGHT   1
#define POWER_TYPE_BACKUP  2
#define POWER_TYPE         POWER_TYPE_SOLAR

/* ─── Auto Calibration ───────────────────────────────────────────────── */
#define AUTCAL          1

/* ─── STM32 Clock ────────────────────────────────────────────────────── */
#define SYSCLK_MHZ      72

/* ─── Timer Periods ──────────────────────────────────────────────────── */
#define TIM4_PSC        71
#define TIM4_ARR        999
#define TIM2_PSC        71

#endif /* CONFIG_H */
