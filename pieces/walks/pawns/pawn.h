#if !defined(PIECES_PAWNS_PAWN_H)
#define PIECES_PAWNS_PAWN_H

#include "position/board.h"
#include "position/side.h"
#include "solving/observation.h"

unsigned int pawn_get_no_capture_length(Side side, square sq_departure);

void  pawn_generate_moves(void);

/* Does any pawn deliver check?
 * @param sq_departure departure square of king capaturing move
 * @param sq_arrival arrival square of king capaturing move
 * @return if any pawn delivers check
 */
boolean pawn_test_check(square sq_departure,
                        square sq_arrival,
                        validator_id evaluate);

boolean pawn_check(validator_id evaluate);

#endif
