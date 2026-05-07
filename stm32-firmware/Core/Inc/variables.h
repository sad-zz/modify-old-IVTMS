/**
 * @file variables.h
 * @brief Global variables – ported from Variables.h (dsPIC30F4011 original)
 *
 * متغیرهای سراسری که مستقیماً از فایل Variables.h اصلی پورت شده‌اند.
 * تغییرات اصلی:
 *   - sbit → GPIO calls
 *   - EEPROM read/write → Flash emulation
 *   - mikroC string functions → standard C string.h
 */

#ifndef VARIABLES_H
#define VARIABLES_H

#include <stdint.h>
#include "config.h"

/* ─── Error Byte Bits ────────────────────────────────────────────────── */
#define MMC_ERR  0x0001
#define LP1_ERR  0x0002
#define LP2_ERR  0x0004
#define LP3_ERR  0x0008
#define LP4_ERR  0x0010
#define VMN_ERR  0x0020
#define SOL_ERR  0x0040
#define LBT_ERR  0x0080
#define L1D_ERR  0x0100
#define L2D_ERR  0x0200

/* ─── Lane direction codes (used by classification) ─────────────────── */
#define LINE1DIR  12   /* vehicle entering lane 1 from direction 12 (loop0 first) */
#define LINE2DIR  12   /* vehicle entering lane 2 from direction 12 (loop2 first) */

/* ─── Global State ───────────────────────────────────────────────────── */
extern volatile uint16_t error_byte;          /* bitmask of active errors         */
extern volatile uint16_t error_byte_last;     /* previous error bitmask           */

extern volatile int16_t  timer_1_sec;         /* set to 1 every second by TIM4    */
extern volatile int32_t  milisec;             /* milliseconds within current second */
extern volatile uint32_t how_many_micro;      /* ms counter (resets at 1000)       */

extern volatile int16_t  onloop[4];           /* 1 when vehicle is detected on loop i */
extern volatile int16_t  Line_Dir[2];         /* direction of detected vehicle on lane */
extern volatile int16_t  timexen[2];          /* timer active for lane             */
extern volatile int32_t  timex[2];            /* elapsed ms since first-loop trigger */

extern volatile int16_t  current_loop;        /* active mux channel (0-3)         */
extern volatile int16_t  docal;               /* 1 during calibration             */
extern volatile int16_t  forth;               /* calibration sample counter       */
extern volatile int32_t  current_gap[2];      /* ms since last vehicle left lane  */
extern volatile int16_t  loop_error_tmp;      /* capture error flag               */

/* Capture buffer (from TIM2 Input Capture) */
extern volatile uint32_t capw, capx, capy, capz;

/* Loop frequency data */
extern volatile uint32_t freq_mean[4];       /* current oscillator period (µs)   */
extern volatile uint32_t caldata[4];         /* baseline period (calibration)     */
extern volatile uint32_t calsum[4];
extern volatile uint16_t calpointer[4];
extern volatile float    freq_help;

/* Device deviation values */
extern volatile int16_t  dev[4];             /* frequency deviation * 10000       */

/* Vehicle counters */
extern volatile uint32_t totalv1a, totalv1b, totalv1c, totalv1d, totalv1e, totalv1x;
extern volatile uint32_t totalv2a, totalv2b, totalv2c, totalv2d, totalv2e, totalv2x;

/* Occupancy counters (1ms ticks with vehicle on loop) */
extern volatile int32_t  l1occ, l2occ;

/* ADC readings */
extern volatile int32_t  vbat;             /* battery ADC value                  */
extern volatile int32_t  solar;            /* solar panel ADC value              */

/* Line state machine */
extern volatile int16_t  line1step, line2step;
extern volatile int32_t  T1[2], T2[2], T3[2];
extern volatile int32_t  T2S;

/* RTC / datetime */
extern char datetimesec[22];              /* "YYYY.MM.DD-HH:MM:SS.t\0"           */

/* Interval data string (264 chars + null) */
extern char interval_data[512];

/* Calibration trigger flags (set in ISR, processed in main loop) */
extern volatile uint8_t  cal_class0;
extern volatile uint8_t  cal_class1;

/* Active loop bitmask */
extern int loop[4];                       /* 1=enabled, 0=disabled                */

/* Configuration parameters (loaded from flash / config.h) */
extern int MARGINTOP;
extern int MARGINBOT;
extern int loop_distance;
extern int loop_width;
extern int LIMX_val;
extern int LIMA_val;
extern int LIMB_val;
extern int LIMC_val;
extern int LIMD_val;
extern int LIMITE_val;
extern int DSPEED1_val, NSPEED1_val;
extern int DSPEED2_val, NSPEED2_val;
extern int AUTCAL_val;
extern int HMM;                           /* heading/direction threshold           */
extern int power_type;

/* Interval calibration counter */
extern uint16_t cal_interval_cnt;

/* cal_timer for adaptive baseline */
extern uint16_t cal_timer[4];

/* ─── Vehicle struct ─────────────────────────────────────────────────── */
typedef struct {
    int16_t       dir;
    uint16_t      speed;   /* km/h */
    char          vclass;  /* 'A','B','C','D','E','X' */
} Vehicle_t;

extern Vehicle_t lastvehicle;

/* ─── RTC time struct ─────────────────────────────────────────────────── */
typedef struct {
    uint8_t year;    /* 0-99 (2000+year) */
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} RTC_Time_t;

extern volatile RTC_Time_t current_time;

/* ─── Interval data struct ───────────────────────────────────────────── */
typedef struct {
    int l1acount; uint16_t l1avavg; int l1aspeed; int l1agrab; int l1aheadway;
    int l1bcount; uint16_t l1bvavg; int l1bspeed; int l1bgrab; int l1bheadway;
    int l1ccount; uint16_t l1cvavg; int l1cspeed; int l1cgrab; int l1cheadway;
    int l1dcount; uint16_t l1dvavg; int l1dspeed; int l1dgrab; int l1dheadway;
    int l1ecount; uint16_t l1evavg; int l1espeed; int l1egrab; int l1eheadway;
    int l1xcount; uint16_t l1xvavg; int l1xspeed; int l1xgrab; int l1xheadway;
    int l1occ;

    int l2acount; uint16_t l2avavg; int l2aspeed; int l2agrab; int l2aheadway;
    int l2bcount; uint16_t l2bvavg; int l2bspeed; int l2bgrab; int l2bheadway;
    int l2ccount; uint16_t l2cvavg; int l2cspeed; int l2cgrab; int l2cheadway;
    int l2dcount; uint16_t l2dvavg; int l2dspeed; int l2dgrab; int l2dheadway;
    int l2ecount; uint16_t l2evavg; int l2espeed; int l2egrab; int l2eheadway;
    int l2xcount; uint16_t l2xvavg; int l2xspeed; int l2xgrab; int l2xheadway;
    int l2occ;
} IntervalData_t;

extern IntervalData_t current_interval;

/* ─── Error helpers ──────────────────────────────────────────────────── */
static inline void set_error(uint16_t bit)   { error_byte |=  bit; }
static inline void reset_error(uint16_t bit) { error_byte &= ~bit; }
static inline int  is_error(uint16_t bit)    { return (error_byte & bit) != 0; }

#endif /* VARIABLES_H */
