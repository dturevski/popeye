#include "output/plaintext/tree/try_writer.h"
#include "pydata.h"
#include "pymsg.h"
#include "pypipe.h"
#include "stipulation/battle_play/defense_play.h"
#include "solving/battle_play/try.h"
#include "solving/trivial_end_filter.h"
#include "output/plaintext/tree/tree.h"
#include "trace.h"

#include <assert.h>

/* Allocate a STTryWriter defender slice.
 * @return index of allocated slice
 */
slice_index alloc_try_writer(void)
{
  slice_index result;

  TraceFunctionEntry(__func__);
  TraceFunctionParamListEnd();

  result = alloc_pipe(STTryWriter);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Try to defend after an attacking move
 * When invoked with some n, the function assumes that the key doesn't
 * solve in less than n half moves.
 * @param si slice index
 * @param n maximum number of half moves until end state has to be reached
 * @return <slack_length_battle - no legal defense found
 *         <=n solved  - <=acceptable number of refutations found
 *                       return value is maximum number of moves
 *                       (incl. defense) needed
 *         n+2 refuted - >acceptable number of refutations found
 */
stip_length_type try_writer_defend(slice_index si, stip_length_type n)
{
  stip_length_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  if (n==max_unsolvable)
  {
    /* refutations are never trivial */
    do_write_trivial_ends[nbply] = true;

    Message(NewLine);
    sprintf(GlobalStr,"%*c",4,blank);
    StdString(GlobalStr);
    Message(But);
  }
  else if (table_length(refutations)>0)
    /* override the decoration attack_key just set by slice
     * STKeyWriter */
    output_plaintext_tree_remember_move_decoration(attack_try);

  result = defend(slices[si].u.pipe.next,n);

  do_write_trivial_ends[nbply] = false;

  TraceFunctionExit(__func__);
  TraceValue("%u",result);
  TraceFunctionResultEnd();
  return result;
}
