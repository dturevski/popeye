#if !defined(STIPULATION_GOAL_IMMOBILE_REACHED_TESTER_NON_KING_H)
#define STIPULATION_GOAL_IMMOBILE_REACHED_TESTER_NON_KING_H

#include "pyslice.h"

/* This module provides functionality dealing with slices that detect
 * whether a side is immobile apart from possible king moves
 */

/* Convert a help branch into a branch testing for immobility apart from
 * possible king moves
 * @param help_branch identifies entry slice into help branch
 * @return identifier of entry slice into tester branch
 */
slice_index make_immobility_tester_non_king(slice_index help_branch);

#endif
