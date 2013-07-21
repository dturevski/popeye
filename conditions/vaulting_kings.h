#if !defined(CONDITIONS_VAULTING_KINGS_H)
#define CONDITIONS_VAULTING_KINGS_H

/* This module implements the fairy condition Vaulting Kings */

#include "utilities/boolean.h"
#include "position/position.h"
#include "pyproc.h"

extern boolean vaulting_kings_transmuting[nr_sides];
extern PieNam king_vaulters[nr_sides][PieceCount];

/* Reset the king vaulters
 */
void reset_king_vaulters(void);

/* Append a piece to the king vaulters
 * @param side for who to add the piece?
 * @param p which piece to add?
 */
void append_king_vaulter(Side side, PieNam p);

/* Does the king of side trait[nbply] attack a particular square
 * (while vaulting or not)?
 * @param sq_target target square
 * @param evaluate attack evaluator
 * true iff the king attacks sq_target
 */
boolean vaulting_kings_is_square_attacked_by_king(square sq_target,
                                                  evalfunction_t *evaluate);

/* Generate moves for a vaulting king
 */
void vaulting_kings_generate_moves_for_piece(slice_index si,
                                             square sq_departure,
                                             PieNam p);

/* Initialise the solving machinery with Vaulting Kings
 * @param si root slice of the solving machinery
 */
void vaulting_kings_initalise_solving(slice_index si);

boolean vaulting_king_is_square_observed(slice_index si,
                                         square sq_target,
                                         evalfunction_t *evaluate);

void vaulting_kings_initialise_square_observation(slice_index si);

#endif
