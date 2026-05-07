/**
 * @file loop_detector.h
 * @brief Inductive loop detector – frequency measurement and vehicle detection
 *
 * پورت از: Capture_Int_Lib.h (dsPIC30F4011)
 *
 * Hardware mapping:
 *   dsPIC IC7 + TMR2 → STM32 TIM2_CH3 Input Capture (PA2) + TIM2 free-run @1MHz
 *   dsPIC LATD (4-bit mux) → STM32 PB0/PB1 (2-bit mux select)
 *   dsPIC TMR4 1ms ISR    → STM32 TIM4 1ms ISR
 *
 * The loop oscillator signal feeds into PA2 (TIM2_CH3).
 * A 4:1 analogue multiplexer (e.g. 4052) selects which loop is connected.
 * PB0 = mux select A, PB1 = mux select B.
 */

#ifndef LOOP_DETECTOR_H
#define LOOP_DETECTOR_H

#include <stdint.h>
#include "variables.h"

/* ─── Public API ─────────────────────────────────────────────────────── */

/** Initialise TIM2 (IC) and TIM4 (1ms tick) peripherals */
void loop_detector_init(void);

/** Perform baseline calibration (call after init, before normal operation).
 *  Blocks for ~1 second. All loops must be free of vehicles. */
void loop_calibrate(void);

/** Called from TIM4 ISR every 1ms – updates freq_mean, dev[], occupancy */
void loop_detector_isr(void);

/** Called from main loop when cal_class0/cal_class1 flags are set */
void measure_loops(void);

#endif /* LOOP_DETECTOR_H */
