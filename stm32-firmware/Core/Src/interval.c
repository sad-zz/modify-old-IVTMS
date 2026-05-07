/**
 * @file interval.c
 * @brief 5-minute interval data – ported from Interval.h (dsPIC30F4011)
 *
 * Builds the 262-character interval_data string that is sent as the body of
 * the 8821 message.
 *
 * Format (indices relative to start of interval_data):
 *  [0-7]    sysId (8 chars, from SYSTEM_ID)
 *  [8-17]   datetime YYMMDDHHmm
 *  [18-131] Lane 1: 6 classes × 19 chars each
 *             each class: count(4) + avgSpeed(3) + violations(4) + grab(4) + headway(4)
 *  [132-134] Lane 1 occupancy (3 chars)
 *  [135-248] Lane 2: same as Lane 1
 *  [249-251] Lane 2 occupancy (3 chars)
 *  [252-254] Battery voltage (3 chars)
 *  [255-257] Solar voltage (3 chars)
 *  [258-261] Error byte (4 chars)
 *  [262-263] CR LF
 */

#include "interval.h"
#include "variables.h"
#include "config.h"
#include "storage.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ─── Helper: write 3-char decimal (with leading zeros) to buf[pos] ──── */
static void write3(char *buf, int pos, int val)
{
    if (val < 0 || val > 999) val = 0;
    buf[pos]   = '0' + (val / 100) % 10;
    buf[pos+1] = '0' + (val / 10)  % 10;
    buf[pos+2] = '0' + (val      ) % 10;
}

/* ─── Helper: write 4-char decimal (with leading zeros) to buf[pos] ──── */
static void write4(char *buf, int pos, int val)
{
    if (val < 0 || val > 9999) val = 0;
    buf[pos]   = '0' + (val / 1000) % 10;
    buf[pos+1] = '0' + (val / 100 ) % 10;
    buf[pos+2] = '0' + (val / 10  ) % 10;
    buf[pos+3] = '0' + (val       ) % 10;
}

/* ─── Helper: write 2-char decimal (with leading zero) to buf[pos] ───── */
static void write2(char *buf, int pos, int val)
{
    if (val < 0 || val > 99) val = 0;
    buf[pos]   = '0' + (val / 10) % 10;
    buf[pos+1] = '0' + (val     ) % 10;
}

/* ─── reset_interval_data ────────────────────────────────────────────── */
void reset_interval_data(void)
{
    uint16_t i;
    for (i = 0; i < 262; i++) interval_data[i] = '0';
    for (i = 0; i < 8;   i++) interval_data[i] = SYSTEM_ID[i];
    interval_data[262] = '\r';
    interval_data[263] = '\n';
    interval_data[264] = '\0';
}

/* ─── reset_interval ─────────────────────────────────────────────────── */
void reset_interval(void)
{
    memset(&current_interval, 0, sizeof(current_interval));
    totalv1a = totalv1b = totalv1c = totalv1d = totalv1e = totalv1x = 0;
    totalv2a = totalv2b = totalv2c = totalv2d = totalv2e = totalv2x = 0;
    l1occ = 0;
    l2occ = 0;
}

/* ─── cal_interval ───────────────────────────────────────────────────── */
void cal_interval(void)
{
    reset_interval_data();

    /* ── Calculate average speeds ─────────────────────────────────────── */
#define AVG_SPD(total, count) ((count) > 0 ? (uint16_t)((total) / (count)) : 0)
    current_interval.l1avavg = AVG_SPD(totalv1a, current_interval.l1acount);
    current_interval.l1bvavg = AVG_SPD(totalv1b, current_interval.l1bcount);
    current_interval.l1cvavg = AVG_SPD(totalv1c, current_interval.l1ccount);
    current_interval.l1dvavg = AVG_SPD(totalv1d, current_interval.l1dcount);
    current_interval.l1evavg = AVG_SPD(totalv1e, current_interval.l1ecount);
    current_interval.l1xvavg = AVG_SPD(totalv1x, current_interval.l1xcount);
    current_interval.l2avavg = AVG_SPD(totalv2a, current_interval.l2acount);
    current_interval.l2bvavg = AVG_SPD(totalv2b, current_interval.l2bcount);
    current_interval.l2cvavg = AVG_SPD(totalv2c, current_interval.l2ccount);
    current_interval.l2dvavg = AVG_SPD(totalv2d, current_interval.l2dcount);
    current_interval.l2evavg = AVG_SPD(totalv2e, current_interval.l2ecount);
    current_interval.l2xvavg = AVG_SPD(totalv2x, current_interval.l2xcount);
#undef AVG_SPD

    /* ── Timestamp: indices [8-17] = YYMMDDHHmm ──────────────────────── */
    write2(interval_data,  8, current_time.year);
    write2(interval_data, 10, current_time.month);
    write2(interval_data, 12, current_time.day);
    write2(interval_data, 14, current_time.hour);
    write2(interval_data, 16, current_time.minute);

    /* ── Lane 1 classes (18-131) ──────────────────────────────────────── */
    /* Each class occupies 19 bytes: count(4)+avgSpeed(3)+violations(4)+grab(4)+headway(4) */
#define WRITE_CLASS_L1(base, cnt, avg, spd, grb, hdw) \
    write4(interval_data, (base),    (cnt)); \
    write3(interval_data, (base)+4,  (avg)); \
    write4(interval_data, (base)+7,  (spd)); \
    write4(interval_data, (base)+11, (grb)); \
    write4(interval_data, (base)+15, (hdw))

    WRITE_CLASS_L1(18,  current_interval.l1acount, current_interval.l1avavg, current_interval.l1aspeed, current_interval.l1agrab, current_interval.l1aheadway);
    WRITE_CLASS_L1(37,  current_interval.l1bcount, current_interval.l1bvavg, current_interval.l1bspeed, current_interval.l1bgrab, current_interval.l1bheadway);
    WRITE_CLASS_L1(56,  current_interval.l1ccount, current_interval.l1cvavg, current_interval.l1cspeed, current_interval.l1cgrab, current_interval.l1cheadway);
    WRITE_CLASS_L1(75,  current_interval.l1dcount, current_interval.l1dvavg, current_interval.l1dspeed, current_interval.l1dgrab, current_interval.l1dheadway);
    WRITE_CLASS_L1(94,  current_interval.l1ecount, current_interval.l1evavg, current_interval.l1espeed, current_interval.l1egrab, current_interval.l1eheadway);
    WRITE_CLASS_L1(113, current_interval.l1xcount, current_interval.l1xvavg, current_interval.l1xspeed, current_interval.l1xgrab, current_interval.l1xheadway);
#undef WRITE_CLASS_L1

    /* Lane 1 occupancy [132-134] */
    write3(interval_data, 132, (int)(l1occ / (INTERVAL_PERIOD * 2 * 600)));

    /* ── Lane 2 classes (135-248) ─────────────────────────────────────── */
#define WRITE_CLASS_L2(base, cnt, avg, spd, grb, hdw) \
    write4(interval_data, (base),    (cnt)); \
    write3(interval_data, (base)+4,  (avg)); \
    write4(interval_data, (base)+7,  (spd)); \
    write4(interval_data, (base)+11, (grb)); \
    write4(interval_data, (base)+15, (hdw))

    WRITE_CLASS_L2(135, current_interval.l2acount, current_interval.l2avavg, current_interval.l2aspeed, current_interval.l2agrab, current_interval.l2aheadway);
    WRITE_CLASS_L2(154, current_interval.l2bcount, current_interval.l2bvavg, current_interval.l2bspeed, current_interval.l2bgrab, current_interval.l2bheadway);
    WRITE_CLASS_L2(173, current_interval.l2ccount, current_interval.l2cvavg, current_interval.l2cspeed, current_interval.l2cgrab, current_interval.l2cheadway);
    WRITE_CLASS_L2(192, current_interval.l2dcount, current_interval.l2dvavg, current_interval.l2dspeed, current_interval.l2dgrab, current_interval.l2dheadway);
    WRITE_CLASS_L2(211, current_interval.l2ecount, current_interval.l2evavg, current_interval.l2espeed, current_interval.l2egrab, current_interval.l2eheadway);
    WRITE_CLASS_L2(230, current_interval.l2xcount, current_interval.l2xvavg, current_interval.l2xspeed, current_interval.l2xgrab, current_interval.l2xheadway);
#undef WRITE_CLASS_L2

    /* Lane 2 occupancy [249-251] */
    write3(interval_data, 249, (int)(l2occ / (INTERVAL_PERIOD * 2 * 600)));

    /* ── Battery voltage [252-254]: V = ADC * 0.388509 + 7 ──────────── */
    write3(interval_data, 252, (int)(vbat * VBAT_SCALE + VBAT_OFFSET));

    /* ── Solar voltage  [255-257]: V = ADC * 0.388509 ────────────────── */
    write3(interval_data, 255, (solar < 50) ? 0 : (int)(solar * VSOL_SCALE));

    /* ── Error byte [258-261] ─────────────────────────────────────────── */
    write4(interval_data, 258, (int)(error_byte & 0xFFFF));

    /* ── Replace any remaining spaces with '0' ────────────────────────── */
    for (cal_interval_cnt = 0; cal_interval_cnt < 262; cal_interval_cnt++) {
        if (interval_data[cal_interval_cnt] == ' ')
            interval_data[cal_interval_cnt] = '0';
    }

    /* ── Terminate with CR LF ─────────────────────────────────────────── */
    interval_data[262] = '\r';
    interval_data[263] = '\n';
    interval_data[264] = '\0';

    /* ── Persist to all available storage backends ────────────────────── */
    /* ts10 is interval_data[8..17] which was just written above           */
    char ts10[11];
    memcpy(ts10, interval_data + 8, 10);
    ts10[10] = '\0';
    storage_save_interval((uint8_t *)interval_data, ts10);

    reset_interval();
}
