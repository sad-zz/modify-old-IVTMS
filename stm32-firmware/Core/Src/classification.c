/**
 * @file classification.c
 * @brief Vehicle classification – ported from Classification.h (dsPIC30F4011)
 *
 * محاسبه سرعت و طول وسیله نقلیه و طبقه‌بندی در کلاس‌های A-E/X
 *
 * The dual-loop speed trap algorithm:
 *   T1 = time vehicle front arrives at first loop
 *   T2 = time vehicle front leaves first loop (= arrives at second loop)
 *   T3 = time vehicle front leaves second loop
 *
 *   speed0 = loop_distance / T1          (from first-loop crossing time)
 *   speed1 = loop_distance / (T3 - T2)   (from second-loop crossing time)
 *   speed  = (speed0 + speed1) / 2       (average)
 *
 *   vehicle_length = speed * (T2 + (T3-T1))/2 - loop_width
 *
 * Time unit: milliseconds (from software timer in loop_detector.c)
 */

#include "classification.h"
#include "variables.h"
#include "config.h"

#include <math.h>
#include <string.h>

/* ─── Private helper: update datetimesec header in tsdata ───────────── */
static char tsdata[34] = "YY.MM.DD,HH:MM:SS,L,C,SSS,1  ";

static void update_tsdata_datetime(void)
{
    /* Copy year, month, day, hour, minute, second from datetimesec        */
    /* datetimesec format: "YYYY.MM.DD-HH:MM:SS.t"                         */
    /* tsdata format:      "YY.MM.DD,HH:MM:SS,lane,class,speed,violation"  */
    tsdata[0]  = datetimesec[2];  /* YY */
    tsdata[1]  = datetimesec[3];
    tsdata[3]  = datetimesec[5];  /* MM */
    tsdata[4]  = datetimesec[6];
    tsdata[6]  = datetimesec[8];  /* DD */
    tsdata[7]  = datetimesec[9];
    tsdata[9]  = datetimesec[11]; /* HH */
    tsdata[10] = datetimesec[12];
    tsdata[12] = datetimesec[14]; /* MM */
    tsdata[13] = datetimesec[15];
    tsdata[15] = datetimesec[17]; /* SS */
    tsdata[16] = datetimesec[18];
    tsdata[18] = datetimesec[20]; /* tenths */
    tsdata[2]  = '.';
    tsdata[5]  = '.';
    tsdata[8]  = ',';
    tsdata[11] = ':';
    tsdata[14] = ':';
    tsdata[17] = ',';
}

/* ─── reset_class ────────────────────────────────────────────────────── */
void reset_class(uint16_t lane)
{
    if (lane == 0) {
        onloop[0] = 0;
        onloop[1] = 0;
        line1step = 0;
    } else {
        onloop[2] = 0;
        onloop[3] = 0;
        line2step = 0;
    }
    T1[lane] = 0;
    T2[lane] = 0;
    T3[lane] = 0;
    Line_Dir[lane] = 0;
    timexen[lane] = 0;
    timex[lane] = 0;
}

/* ─── cal_class ──────────────────────────────────────────────────────── */
void cal_class(uint16_t lane)
{
    double vehicle_speed0, vehicle_speed1, vehicle_speed, vehicle_length;
    uint16_t wspeed = 0, wgrab = 0, wheadway = 0;
    char vclass;
    uint16_t lane_dir;

    /* Speed calculation (speed in mm/ms = m/s; multiply by 3.6 → km/h) */
    vehicle_speed0 = (double)(loop_distance) * 1000.0;
    vehicle_speed1 = (double)(loop_distance) * 1000.0;

    if (T1[lane] > 0)
        vehicle_speed0 /= (double)(T1[lane]);
    else
        vehicle_speed0 = 0;

    if ((T3[lane] - T2[lane]) > 0)
        vehicle_speed1 /= (double)(T3[lane] - T2[lane]);
    else
        vehicle_speed1 = 0;

    vehicle_speed = (vehicle_speed0 + vehicle_speed1) / 2.0;

    /* Vehicle length calculation */
    T2S = (T2[lane] + (T3[lane] - T1[lane])) / 2;
    vehicle_length = vehicle_speed * (double)(T2S);
    vehicle_length /= 1000.0;
    vehicle_length -= (double)(loop_width);

    lastvehicle.speed = (uint16_t)(floor(0.036 * vehicle_speed));
    lastvehicle.dir   = Line_Dir[lane];

    /* ── Speed violation check ─────────────────────────────────────────── */
    int is_day = (current_time.hour > DAY_HOUR_START &&
                  current_time.hour < DAY_HOUR_END);

    if (vehicle_length < LIMA_val) {
        /* Class A / X (small vehicle) */
        int limit = is_day ? DSPEED1_val : NSPEED1_val;
        if (lastvehicle.speed > (uint16_t)limit) wspeed = 1;
    } else {
        /* Class B-E (larger vehicle) */
        int limit = is_day ? DSPEED2_val : NSPEED2_val;
        if (lastvehicle.speed > (uint16_t)limit) wspeed = 1;
    }

    /* ── Wrong-way / grab detection ──────────────────────────────────── */
    lane_dir = (lane == 0) ? LINE1DIR : LINE2DIR;
    if (lastvehicle.dir != lane_dir) {
        wgrab = 1;
        /* swap lane for counter update */
        lane = (lane == 0) ? 1 : 0;
    }

    /* ── Headway / too-close detection ──────────────────────────────── */
    if (current_gap[lane] < GAP_DELAY_MS) {
        wheadway = 1;
    }

    /* ── Vehicle classification by length ────────────────────────────── */
#define UPDATE_CLASS(CLASS_CHAR, CNT, SPD, GRB, HDW, TOTAL) \
    do { \
        vclass = CLASS_CHAR; \
        CNT++;              \
        if (wspeed)   SPD++;   \
        if (wgrab)    GRB++;   \
        if (wheadway) HDW++;   \
        TOTAL += lastvehicle.speed; \
    } while(0)

    if (lane == 0) {
        /* Class X is used for two distinct cases:
         *   1. Too short  (< LIMX_val): likely noise, debris, or narrow vehicle
         *   2. Too long   (> LIMITE_val): oversized load, multi-trailer
         * Both are reported as 'X' to match the original firmware behaviour and
         * the RMTO SOAP C5 mapping (C5 = e + x). */
        if      (vehicle_length < LIMX_val)   UPDATE_CLASS('X', current_interval.l1xcount, current_interval.l1xspeed, current_interval.l1xgrab, current_interval.l1xheadway, totalv1x);
        else if (vehicle_length < LIMA_val)   UPDATE_CLASS('A', current_interval.l1acount, current_interval.l1aspeed, current_interval.l1agrab, current_interval.l1aheadway, totalv1a);
        else if (vehicle_length < LIMB_val)   UPDATE_CLASS('B', current_interval.l1bcount, current_interval.l1bspeed, current_interval.l1bgrab, current_interval.l1bheadway, totalv1b);
        else if (vehicle_length < LIMC_val)   UPDATE_CLASS('C', current_interval.l1ccount, current_interval.l1cspeed, current_interval.l1cgrab, current_interval.l1cheadway, totalv1c);
        else if (vehicle_length < LIMD_val)   UPDATE_CLASS('D', current_interval.l1dcount, current_interval.l1dspeed, current_interval.l1dgrab, current_interval.l1dheadway, totalv1d);
        else if (vehicle_length < LIMITE_val) UPDATE_CLASS('E', current_interval.l1ecount, current_interval.l1espeed, current_interval.l1egrab, current_interval.l1eheadway, totalv1e);
        else                                  UPDATE_CLASS('X', current_interval.l1xcount, current_interval.l1xspeed, current_interval.l1xgrab, current_interval.l1xheadway, totalv1x);
    } else {
        if      (vehicle_length < LIMX_val)   UPDATE_CLASS('X', current_interval.l2xcount, current_interval.l2xspeed, current_interval.l2xgrab, current_interval.l2xheadway, totalv2x);
        else if (vehicle_length < LIMA_val)   UPDATE_CLASS('A', current_interval.l2acount, current_interval.l2aspeed, current_interval.l2agrab, current_interval.l2aheadway, totalv2a);
        else if (vehicle_length < LIMB_val)   UPDATE_CLASS('B', current_interval.l2bcount, current_interval.l2bspeed, current_interval.l2bgrab, current_interval.l2bheadway, totalv2b);
        else if (vehicle_length < LIMC_val)   UPDATE_CLASS('C', current_interval.l2ccount, current_interval.l2cspeed, current_interval.l2cgrab, current_interval.l2cheadway, totalv2c);
        else if (vehicle_length < LIMD_val)   UPDATE_CLASS('D', current_interval.l2dcount, current_interval.l2dspeed, current_interval.l2dgrab, current_interval.l2dheadway, totalv2d);
        else if (vehicle_length < LIMITE_val) UPDATE_CLASS('E', current_interval.l2ecount, current_interval.l2espeed, current_interval.l2egrab, current_interval.l2eheadway, totalv2e);
        else                                  UPDATE_CLASS('X', current_interval.l2xcount, current_interval.l2xspeed, current_interval.l2xgrab, current_interval.l2xheadway, totalv2x);
    }
#undef UPDATE_CLASS

    lastvehicle.vclass = vclass;

    /* ── Debug output on USART1 (optional) ──────────────────────────── */
    update_tsdata_datetime();
    tsdata[20] = (char)(lane + '1');  /* lane number ASCII */
    tsdata[22] = vclass;
    /* speed 3 digits */
    tsdata[24] = '0' + (lastvehicle.speed / 100) % 10;
    tsdata[25] = '0' + (lastvehicle.speed / 10) % 10;
    tsdata[26] = '0' + lastvehicle.speed % 10;
    tsdata[28] = wspeed   ? '1' : '0';
    tsdata[29] = wgrab    ? '1' : '0';
    tsdata[30] = wheadway ? '1' : '0';

    /* Reset gap counter and lane state machine */
    current_gap[lane] = 0;
    reset_class(lane);
}

/* ─── measure_loops ──────────────────────────────────────────────────── */
/**
 * Ported from measure_loops() in 91-7.c.
 * Called every 1ms from TIM4 ISR.
 * Detects vehicle entering/leaving each loop and manages the state machine.
 */
void measure_loops(void)
{
    /* ── Lane 1 – Loop 0 (far, direction 1→2) ────────────────────────── */
    if (!onloop[0] && loop[0] && dev[0] > MARGINTOP &&
        (line1step == 0 || (line1step == 1 && Line_Dir[0] == 21)))
    {
        onloop[0] = 1;
        if (timexen[0]) {
            T1[0] = timex[0];
            line1step = 2;
        } else {
            timexen[0] = 1;
            line1step = 1;
            caldata[1] = freq_mean[1]; /* recal adjacent loop */
            Line_Dir[0] = 12;
        }
    }
    else if (onloop[0] && loop[0] && dev[0] < MARGINBOT &&
             ((line1step == 2 && Line_Dir[0] == 12) ||
              (line1step == 3 && Line_Dir[0] == 21)))
    {
        onloop[0] = 0;
        if (T2[0] > 0) {
            T3[0] = timex[0];
            line1step = 0;
            caldata[1] = freq_mean[1];
            cal_class0 = 1;
        } else {
            T2[0] = timex[0];
            line1step = 3;
        }
    }
    else if (onloop[0] && loop[0] && dev[0] < MARGINBOT)
    {
        reset_class(0);
    }

    /* ── Lane 1 – Loop 1 (near, direction 2→1) ───────────────────────── */
    if (!onloop[1] && loop[1] && dev[1] > MARGINTOP &&
        (line1step == 0 || (line1step == 1 && Line_Dir[0] == 12)))
    {
        onloop[1] = 1;
        if (timexen[0]) {
            T1[0] = timex[0];
            line1step = 2;
        } else {
            timexen[0] = 1;
            line1step = 1;
            caldata[0] = freq_mean[0];
            Line_Dir[0] = 21;
        }
    }
    else if (onloop[1] && loop[1] && dev[1] < MARGINBOT &&
             ((line1step == 3 && Line_Dir[0] == 12) ||
              (line1step == 2 && Line_Dir[0] == 21)))
    {
        onloop[1] = 0;
        if (T2[0] > 0) {
            T3[0] = timex[0];
            line1step = 0;
            caldata[0] = freq_mean[0];
            cal_class0 = 1;
        } else {
            T2[0] = timex[0];
            line1step = 3;
        }
    }
    else if (onloop[1] && loop[1] && dev[1] < MARGINBOT)
    {
        reset_class(0);
    }

    /* ── Lane 2 – Loop 2 (far, direction 1→2) ────────────────────────── */
    if (!onloop[2] && loop[2] && dev[2] > MARGINTOP &&
        (line2step == 0 || (line2step == 1 && Line_Dir[1] == 21)))
    {
        onloop[2] = 1;
        if (timexen[1]) {
            T1[1] = timex[1];
            line2step = 2;
        } else {
            timexen[1] = 1;
            line2step = 1;
            caldata[3] = freq_mean[3];
            Line_Dir[1] = 12;
        }
    }
    else if (onloop[2] && loop[2] && dev[2] < MARGINBOT &&
             ((line2step == 2 && Line_Dir[1] == 12) ||
              (line2step == 3 && Line_Dir[1] == 21)))
    {
        onloop[2] = 0;
        if (T2[1] > 0) {
            T3[1] = timex[1];
            line2step = 0;
            caldata[3] = freq_mean[3];
            cal_class1 = 1;
        } else {
            T2[1] = timex[1];
            line2step = 3;
        }
    }
    else if (onloop[2] && loop[2] && dev[2] < MARGINBOT)
    {
        reset_class(1);
    }

    /* ── Lane 2 – Loop 3 (near, direction 2→1) ───────────────────────── */
    if (!onloop[3] && loop[3] && dev[3] > MARGINTOP &&
        (line2step == 0 || (line2step == 1 && Line_Dir[1] == 12)))
    {
        onloop[3] = 1;
        if (timexen[1]) {
            T1[1] = timex[1];
            line2step = 2;
        } else {
            timexen[1] = 1;
            line2step = 1;
            caldata[2] = freq_mean[2];
            Line_Dir[1] = 21;
        }
    }
    else if (onloop[3] && loop[3] && dev[3] < MARGINBOT &&
             ((line2step == 3 && Line_Dir[1] == 12) ||
              (line2step == 2 && Line_Dir[1] == 21)))
    {
        onloop[3] = 0;
        if (T2[1] > 0) {
            T3[1] = timex[1];
            line2step = 0;
            caldata[2] = freq_mean[2];
            cal_class1 = 1;
        } else {
            T2[1] = timex[1];
            line2step = 3;
        }
    }
    else if (onloop[3] && loop[3] && dev[3] < MARGINBOT)
    {
        reset_class(1);
    }
}
