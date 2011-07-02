#include "conditions/owu/immobility_tester.h"
#include "stipulation/proxy.h"
#include "stipulation/branch.h"
#include "stipulation/goals/immobile/reached_tester_non_king.h"
#include "stipulation/boolean/and.h"
#include "solving/king_move_generator.h"
#include "solving/legal_move_counter.h"
#include "solving/capture_counter.h"
#include "trace.h"

#include <assert.h>

/* This module provides functionality dealing with slices that detect
 * whether a side is immobile
 */

static void substitute_owu_specific_testers(slice_index si,
                                            stip_structure_traversal *st)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  stip_traverse_structure_children(si,st);

  {
    slice_index const proxy1 = alloc_proxy_slice();
    slice_index const proxy2 = alloc_proxy_slice();
    slice_index const next = slices[si].u.pipe.next;
    slice_index const king_tester = alloc_pipe(STOWUImmobilityTesterKing);

    pipe_link(si,alloc_and_slice(proxy1,proxy2));
    pipe_link(proxy1,king_tester);
    pipe_link(king_tester,next);

    pipe_link(proxy2,make_immobility_tester_non_king(stip_deep_copy(next)));

    {
      slice_index const generator = branch_find_slice(STMoveGenerator,next);
      slice_index const prototype = alloc_pipe(STCaptureCounter);

      assert(generator!=no_slice);
      pipe_substitute(generator,alloc_king_move_generator_slice());

      branch_insert_slices(next,&prototype,1);
    }

    pipe_remove(si);
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Substitute OWU specific immobility testers
 * @param si where to start (entry slice into stipulation)
 */
void owu_replace_immobility_testers(slice_index si)
{
  stip_structure_traversal st;

  TraceFunctionEntry(__func__);
  TraceFunctionParamListEnd();

  stip_structure_traversal_init(&st,0);
  stip_structure_traversal_override_single(&st,
                                           STImmobilityTester,
                                           &substitute_owu_specific_testers);
  stip_traverse_structure(si,&st);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Determine whether a slice.has just been solved with the move
 * by the non-starter
 * @param si slice identifier
 * @return whether there is a solution and (to some extent) why not
 */
has_solution_type owu_immobility_tester_king_has_solution(slice_index si)
{
  has_solution_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  /* avoid concurrent counts */
  assert(legal_move_counter_count==0);
  assert(capture_counter_count==0);

  /* stop counting once we have >1 legal king moves */
  legal_move_counter_interesting = 0;

  /* stop counting once we have >1 legal king captures */
  capture_counter_interesting = 1;

  slice_has_solution(slices[si].u.pipe.next);

  result = (legal_move_counter_count==0 && capture_counter_count==1
            ? has_solution
            : has_no_solution);

  /* clean up after ourselves */
  capture_counter_count = 0;
  legal_move_counter_count = 0;

  TraceFunctionExit(__func__);
  TraceEnumerator(has_solution_type,result,"");
  TraceFunctionResultEnd();
  return result;
}