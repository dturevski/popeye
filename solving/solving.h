#if !defined(SOLVING_H)
#define SOLVING_H

#include "pystip.h"

/* Instrument the stipulation structure with solving slices
 * @param root_slice root slice of the stipulation
 */
void stip_insert_solvers(slice_index root_slice);

#endif