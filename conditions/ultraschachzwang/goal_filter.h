#if !defined(STIPULATION_ULTRASCHACHZWANG_GOAL_FILTER_H)
#define STIPULATION_ULTRASCHACHZWANG_GOAL_FILTER_H

#include "solving/solve.h"

/* This module provides slice type STUltraschachzwangGoalFilter. This slice
 * suspends Ultraschachzwang when testing for mate.
 */

/* Instrument a stipulation with Ultraschachzwang mate filter slices
 * @param si root of branch to be instrumented
 */
void ultraschachzwang_initialise_solving(slice_index si);

#endif
