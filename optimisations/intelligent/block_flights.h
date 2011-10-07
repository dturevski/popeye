#if !defined(OPTIMISATIONS_INTELLIGENT_BLOCK_FLIGHTS_H)
#define OPTIMISATIONS_INTELLIGENT_BLOCK_FLIGHTS_H

#include "py.h"

void intelligent_block_flights(unsigned int nr_remaining_white_moves,
                                 unsigned int nr_remaining_black_moves,
                                 unsigned int min_nr_captures_by_white,
                                 stip_length_type n);

#endif