/**
 * @file interval.h
 * @brief 5-minute interval data collection – ported from Interval.h (dsPIC30F4011)
 *
 * هر INTERVAL_PERIOD دقیقه، داده‌های ترافیکی جمع‌آوری شده در یک رشته ۲۶۲ کاراکتری
 * فرمت‌بندی می‌شوند که همان قالب ۸۸۲۱ در پروتکل RATCX1 است.
 */

#ifndef INTERVAL_H
#define INTERVAL_H

#include <stdint.h>
#include "variables.h"

/** Reset the 262-char interval_data string to zeros (keeps sysId in [0-7]) */
void reset_interval_data(void);

/** Reset all interval accumulator counters */
void reset_interval(void);

/** Build the 262-char interval_data string from current_interval and reset counters.
 *  Call at end of each INTERVAL_PERIOD minutes. */
void cal_interval(void);

#endif /* INTERVAL_H */
