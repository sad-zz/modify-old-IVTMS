/**
 * @file classification.h
 * @brief Vehicle classification – ported from Classification.h (dsPIC30F4011)
 *
 * محاسبه سرعت، طول وسیله نقلیه و طبقه‌بندی آن
 */

#ifndef CLASSIFICATION_H
#define CLASSIFICATION_H

#include <stdint.h>
#include "variables.h"

/** Calculate vehicle speed/length and classify into A-E/X.
 *  Updates current_interval counters. Call from main loop when cal_class0/1 set.
 *  @param lane  0 = lane 1, 1 = lane 2 */
void cal_class(uint16_t lane);

/** Reset state machine for a lane (e.g. after stale detection) */
void reset_class(uint16_t lane);

#endif /* CLASSIFICATION_H */
