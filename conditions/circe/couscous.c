#include "conditions/circe/couscous.h"
#include "stipulation/stipulation.h"
#include "conditions/circe/circe.h"
#include "pydata.h"
#include "debugging/trace.h"

#include <assert.h>

/* Try to solve in n half-moves.
 * @param si slice index
 * @param n maximum number of half moves
 * @return length of solution found and written, i.e.:
 *            slack_length-2 the move just played or being played is illegal
 *            <=n length of shortest solution found
 *            n+2 no solution found
 */
stip_length_type circe_couscous_determine_relevant_piece_solve(slice_index si,
                                                               stip_length_type n)
{
  stip_length_type result;
  square const sq_arrival = move_generation_stack[current_move[nbply]].arrival;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  current_circe_relevant_piece[nbply] = e[sq_arrival];
  current_circe_relevant_spec[nbply] = spec[sq_arrival];
  current_circe_capturer[nbply] = advers(slices[si].starter);

  result = solve(slices[si].next1,n);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Instrument a stipulation
 * @param si identifies root slice of stipulation
 */
void stip_insert_couscous_circe(slice_index si)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  stip_replace_circe_determine_relevant_piece(si,
                                              STCirceCouscousDetermineRelevantPiece);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}