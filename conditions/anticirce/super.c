#include "conditions/anticirce/super.h"
#include "pydata.h"
#include "conditions/circe/rebirth_handler.h"
#include "stipulation/has_solution_type.h"
#include "stipulation/stipulation.h"
#include "stipulation/pipe.h"
#include "stipulation/branch.h"
#include "stipulation/move_player.h"
#include "solving/post_move_iteration.h"
#include "conditions/anticirce/rebirth_handler.h"
#include "debugging/trace.h"

#include <assert.h>

static post_move_iteration_id_type prev_post_move_iteration_id[maxply+1];

static boolean is_rebirth_square_dirty[maxply+1];

static square next_rebirth_square(void)
{
  square const sq_capture = move_generation_stack[current_move[nbply]].capture;
  piece const pi_captured = e[sq_capture];
  square result = current_anticirce_rebirth_square[nbply]+1;

  /* TODO simplify relation to Chelan type */
  e[sq_capture] = vide;

  while (e[result]!=vide && result<=square_h8)
    ++result;

  if (AntiCirCheylan && result==sq_capture)
  {
    ++result;
    while (e[result]!=vide && result<=square_h8)
      ++result;
  }

  e[sq_capture] = pi_captured;

  return result;
}

static boolean advance_rebirth_square()
{
  boolean result;

  TraceFunctionEntry(__func__);
  TraceFunctionParamListEnd();

  {
    square const next = next_rebirth_square();
    if (next>square_h8)
    {
      current_anticirce_rebirth_square[nbply] = square_a1;
      result = false;
    }
    else
    {
      current_anticirce_rebirth_square[nbply] = next;
      result = true;
    }
  }

  is_rebirth_square_dirty[nbply] = false;

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Try to solve in n half-moves after a defense.
 * @param si slice index
 * @param n maximum number of half moves until goal
 * @return length of solution found and written, i.e.:
 *            slack_length-2 defense has turned out to be illegal
 *            <=n length of shortest solution found
 *            n+2 no solution found
 */
stip_length_type antisupercirce_rebirth_handler_attack(slice_index si,
                                                       stip_length_type n)
{
  stip_length_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  if (pprise[nbply]==vide)
  {
    current_anticirce_rebirth_square[nbply] = initsquare;
    result = attack(slices[si].next1,n);
  }
  else
  {
    if (post_move_iteration_id[nbply]!=prev_post_move_iteration_id[nbply])
    {
      current_anticirce_rebirth_square[nbply] = square_a1-1;
      is_rebirth_square_dirty[nbply] = true;
    }

    if (is_rebirth_square_dirty[nbply] && !advance_rebirth_square())
    {
      current_anticirce_rebirth_square[nbply] = initsquare;
      result = n+2;
    }
    else
    {
      anticirce_do_rebirth(move_effect_reason_antisupercirce_rebirth);
      result = attack(slices[si].next1,n);

      if (!post_move_iteration_locked[nbply])
      {
        is_rebirth_square_dirty[nbply] = true;
        lock_post_move_iterations();
      }
    }

    prev_post_move_iteration_id[nbply] = post_move_iteration_id[nbply];
  }

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
 * @return <slack_length - no legal defense found
 *         <=n solved  - <=acceptable number of refutations found
 *                       return value is maximum number of moves
 *                       (incl. defense) needed
 *         n+2 refuted - >acceptable number of refutations found
 */
stip_length_type antisupercirce_rebirth_handler_defend(slice_index si,
                                                       stip_length_type n)
{
  stip_length_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  if (pprise[nbply]==vide)
  {
    current_anticirce_rebirth_square[nbply] = initsquare;
    result = defend(slices[si].next1,n);
  }
  else
  {
    if (post_move_iteration_id[nbply]!=prev_post_move_iteration_id[nbply])
    {
      current_anticirce_rebirth_square[nbply] = square_a1-1;
      is_rebirth_square_dirty[nbply] = true;
    }

    if (is_rebirth_square_dirty[nbply] && !advance_rebirth_square())
    {
      current_anticirce_rebirth_square[nbply] = initsquare;
      result = slack_length-1;
    }
    else
    {
      anticirce_do_rebirth(move_effect_reason_antisupercirce_rebirth);
      result = defend(slices[si].next1,n);

      if (!post_move_iteration_locked[nbply])
      {
        is_rebirth_square_dirty[nbply] = true;
        lock_post_move_iterations();
      }
    }

    prev_post_move_iteration_id[nbply] = post_move_iteration_id[nbply];
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Instrument a stipulation
 * @param si identifies root slice of stipulation
 */
void stip_insert_antisupercirce_rebirth_handlers(slice_index si)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  stip_instrument_moves(si,STAntisupercirceRebirthHandler);
  stip_instrument_moves(si,STAnticirceRebornPromoter);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}