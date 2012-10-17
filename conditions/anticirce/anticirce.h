#if !defined(CONDITIONS_ANTICIRCE_ANTICIRCE_H)
#define CONDITIONS_ANTICIRCE_ANTICIRCE_H

#include "solving/solve.h"
#include "stipulation/slice_type.h"

extern piece anticirce_current_reborn_piece[maxply+1];
extern Flags anticirce_current_reborn_spec[maxply+1];

extern piece anticirce_current_relevant_piece[maxply+1];
extern Flags anticirce_current_relevant_spec[maxply+1];
extern Side anticirce_current_relevant_side[maxply+1];

/* Try to solve in n half-moves.
 * @param si slice index
 * @param n maximum number of half moves
 * @return length of solution found and written, i.e.:
 *            slack_length-2 the move just played or being played is illegal
 *            <=n length of shortest solution found
 *            n+2 no solution found
 */
stip_length_type anticirce_determine_reborn_piece_solve(slice_index si,
                                                        stip_length_type n);


/* Try to solve in n half-moves.
 * @param si slice index
 * @param n maximum number of half moves
 * @return length of solution found and written, i.e.:
 *            slack_length-2 the move just played or being played is illegal
 *            <=n length of shortest solution found
 *            n+2 no solution found
 */
stip_length_type anticirce_determine_relevant_piece_solve(slice_index si,
                                                          stip_length_type n);

/* Try to solve in n half-moves.
 * @param si slice index
 * @param n maximum number of half moves
 * @return length of solution found and written, i.e.:
 *            slack_length-2 the move just played or being played is illegal
 *            <=n length of shortest solution found
 *            n+2 no solution found
 */
stip_length_type anticirce_determine_rebirth_square_solve(slice_index si,
                                                          stip_length_type n);

/* Use an alternative type of slices for determining the piece relevant for
 * determining the rebirth square
 * @param si identifies root slice of stipulation
 * @param substitute substitute slice type
 */
void stip_replace_anticirce_determine_relevant_piece(slice_index si,
                                                     slice_type substitute);

/* Instrument a stipulation for strict Circe rebirths
 * @param si identifies root slice of stipulation
 */
void stip_insert_anticirce(slice_index si);

#endif