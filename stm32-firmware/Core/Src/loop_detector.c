/**
 * @file loop_detector.c
 * @brief Inductive loop detector – ported from Capture_Int_Lib.h
 *
 * Hardware:
 *   TIM2 free-runs at 1MHz (PSC=71 @72MHz, ARR=0xFFFFFFFF)
 *   TIM2_CH3 (PA2) Input Capture – every rising edge triggers CC3 interrupt
 *   TIM4 (PSC=71, ARR=999) fires every 1ms – main timing tick
 *
 *   MUX select:  PB0=A, PB1=B  → selects 1-of-4 loop oscillators
 *   Status LEDs: PB5-PB8 (onloop0-3), PC13 (1Hz blink)
 */

#include "loop_detector.h"
#include "classification.h"
#include "variables.h"
#include "config.h"

#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdlib.h>

/* ─── Extern HAL handles (defined in main.c) ─────────────────────────── */
extern TIM_HandleTypeDef htim2;
extern TIM_HandleTypeDef htim4;

/* ─── Private variables ──────────────────────────────────────────────── */
static uint32_t ic_last_capture = 0;   /* last TIM2_CH3 capture value     */
static uint32_t ic_prev_capture = 0;   /* previous capture for period calc */
static volatile uint8_t ic_updated = 0;/* set by CC3 ISR, cleared by TIM4 */

/* ─── Peripheral init ────────────────────────────────────────────────── */
void loop_detector_init(void)
{
    /* TIM2: 32-bit free-running counter @1MHz for Input Capture           */
    HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_3);

    /* TIM4: 1ms interrupt tick                                             */
    HAL_TIM_Base_Start_IT(&htim4);
}

/* ─── TIM2 CC3 interrupt callback (called from HAL IRQ handler) ──────── */
/* Saves the captured timer value so TIM4 ISR can compute the period.      */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2 && htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3)
    {
        ic_last_capture = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_3);
        ic_updated = 1;
    }
}

/* ─── TIM4 1ms ISR body (call from HAL_TIM_PeriodElapsedCallback) ──────── */
/**
 * Mirrors dsPIC TMR4INT():
 *  1. Compute freq_mean[current_loop] from IC captures
 *  2. Compute deviation dev[current_loop]
 *  3. Advance MUX to next active loop
 *  4. Update occupancy counters
 *  5. Increment software RTC
 *  6. Call measure_loops()
 */
void loop_detector_isr(void)
{
    /* ── Compute period for current loop ──────────────────────────────── */
    if (ic_updated)
    {
        uint32_t period = ic_last_capture - ic_prev_capture;
        ic_prev_capture = ic_last_capture;
        ic_updated = 0;

        /* period == 0 means the capture timestamp did not advance, which
         * indicates no oscillator edge arrived since the last ISR tick.
         * This is treated as a lost signal (loop error), not a valid
         * zero-period measurement which is physically impossible. */
        if (period == 0)
        {
            dev[current_loop] = 0;
            switch (current_loop) {
                case 0: set_error(LP1_ERR); break;
                case 1: set_error(LP2_ERR); break;
                case 2: set_error(LP3_ERR); break;
                case 3: set_error(LP4_ERR); break;
            }
        }
        else
        {
            freq_mean[current_loop] = period;

            /* deviation = (baseline_period - current_period) / baseline * 10000
             * When a vehicle is on the loop the inductance drops, oscillator
             * frequency rises, period DECREASES → dev > 0 */
            if (caldata[current_loop] > 0)
            {
                freq_help = (float)(caldata[current_loop]) - (float)(period);
                freq_help /= (float)(caldata[current_loop]);
                freq_help *= 10000.0f;
                dev[current_loop] = (int16_t)(freq_help);
            }

            switch (current_loop) {
                case 0: reset_error(LP1_ERR); break;
                case 1: reset_error(LP2_ERR); break;
                case 2: reset_error(LP3_ERR); break;
                case 3: reset_error(LP4_ERR); break;
            }
        }
    }

    /* ── Occupancy counters ───────────────────────────────────────────── */
    if (!docal)
    {
        if (current_gap[0] < 10000) current_gap[0]++;
        if (current_gap[1] < 10000) current_gap[1]++;
        if (onloop[0]) l1occ++;
        if (onloop[1]) l1occ++;
        if (onloop[2]) l2occ++;
        if (onloop[3]) l2occ++;
    }

    /* ── Advance MUX to next active loop ─────────────────────────────── */
    do {
        current_loop++;
        if (current_loop == 4) current_loop = 0;
    } while (loop[current_loop] == 0);

    /* Set MUX select pins (PB0, PB1) */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, (current_loop & 1) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, (current_loop & 2) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    if (docal) forth++;

    /* ── Software RTC (1ms resolution) ───────────────────────────────── */
    how_many_micro++;
    milisec = how_many_micro;

    if (how_many_micro >= 1000)
    {
        how_many_micro = 0;
        milisec = 0;
        timer_1_sec = 1;

        if (current_time.second < 59) {
            current_time.second++;
        } else {
            current_time.second = 0;
            if (current_time.minute < 59) {
                current_time.minute++;
            } else {
                current_time.minute = 0;
                if (current_time.hour < 23) {
                    current_time.hour++;
                } else {
                    current_time.hour = 0;
                    if (current_time.day < 31) {
                        current_time.day++;
                    } else {
                        current_time.day = 1;
                        if (current_time.month < 12) {
                            current_time.month++;
                        } else {
                            current_time.month = 1;
                            current_time.year++;
                        }
                    }
                }
            }
        }
    }

    /* ── Call loop detection state machine ───────────────────────────── */
    measure_loops();
}

/* ─── Calibration ────────────────────────────────────────────────────── */
/**
 * Measure baseline period of each loop oscillator with no vehicles present.
 * Forces all loops to appear "occupied" during measurement so the mux
 * cycles through all of them, then takes 10 readings and averages.
 */
void loop_calibrate(void)
{
    uint8_t k;
    uint32_t cal_k;

    docal = 1;
    for (cal_k = 0; cal_k < 4; cal_k++) {
        caldata[cal_k] = 0;
        calsum[cal_k] = 0;
        calpointer[cal_k] = 0;
    }

    for (k = 0; k < 10; k++) {
        forth = 0;
        while (forth < 4);  /* wait for one full mux cycle (4 loops × ~1ms) */
        for (cal_k = 0; cal_k < 4; cal_k++) {
            calsum[cal_k] += freq_mean[cal_k];
            calpointer[cal_k]++;
        }
        HAL_Delay(100);
    }

    for (cal_k = 0; cal_k < 4; cal_k++) {
        if (calpointer[cal_k] > 0)
            caldata[cal_k] = calsum[cal_k] / calpointer[cal_k];
    }

    docal = 0;
}
