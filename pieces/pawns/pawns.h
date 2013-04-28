#if !defined(PIECES_PAWNS_PAWNS_H)
#define PIECES_PAWNS_PAWNS_H

#include "position/board.h"
#include "position/position.h"
#include "py.h"

/* Generate en passant capture moves for any kind of pawn if there are any
 * @param side side for which to generate
 * @param sq_departure departure square of generated move
 * @param sq_arrival arrival square of generated move
 * @param sq_arrival_singlestep imaginary single step arrival square of the
 *                              double step
 * @note pawns_generate_ep_capture_move() is called by
 *       pawns_generate_capture_move(). Only call it from function that
 *       generate captures using other means.
 */
void pawns_generate_ep_capture_move(Side side,
                                    square sq_departure,
                                    square sq_arrival,
                                    square sq_arrival_singlestep);

/* generates move of a pawn of side camp on departure capturing a piece on
 * arrival
 */
void pawns_generate_capture_move(Side camp,
                                 square sq_departure,
                                 square sq_arrival);

/* generates moves of a pawn in direction dir where steps single steps are
 * possible.
 */
void pawns_generate_nocapture_moves(square sq_departure, numvec dir, int steps);

#endif
