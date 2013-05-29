#if !defined(PIECESS_PAWNS_PAWN_H)
#define PIECESS_PAWNS_PAWN_H

#include "pyproc.h"
#include "position/board.h"
#include "position/position.h"

unsigned int pawn_get_no_capture_length(Side side, square sq_departure);

void  pawn_generate_moves(Side side, square sq_departure);

/* Does any pawn deliver check?
 * @param sq_departure departure square of king capaturing move
 * @param sq_arrival arrival square of king capaturing move
 * @param sq_capture square where king is captured (often ==sq_arrival)
 * @return if any pawn delivers check
 */
boolean pawn_test_check(square sq_departure,
                        square sq_arrival,
                        square sq_capture,
                        piece p,
                        evalfunction_t *evaluate);

#endif
