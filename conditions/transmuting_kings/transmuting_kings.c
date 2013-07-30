#include "conditions/transmuting_kings/transmuting_kings.h"
#include "solving/move_generator.h"
#include "solving/observation.h"
#include "stipulation/stipulation.h"
#include "stipulation/proxy.h"
#include "stipulation/branch.h"
#include "debugging/trace.h"
#include "pydata.h"

#include <assert.h>

PieNam transmpieces[nr_sides][PieceCount];

/* Initialise the sequence of king transmuters
 * @param side for which side to initialise?
 */
void init_transmuters_sequence(Side side)
{
  unsigned int tp = 0;
  PieNam p;

  for (p = King; p<PieceCount; ++p) {
    if (may_exist[p] && p!=Dummy && p!=Hamster)
    {
      transmpieces[side][tp] = p;
      tp++;
    }
  }

  transmpieces[side][tp] = Empty;
}

/* Determine whether the moving side's king is transmuting as a specific piece
 * @param p the piece
 */
boolean is_king_transmuting_as(PieNam p, evalfunction_t *evaluate)
{
  boolean result;
  Side const side_attacking = trait[nbply];

  trait[nbply] = advers(side_attacking);
  result = (*checkfunctions[p])(king_square[side_attacking],p,evaluate);
  trait[nbply] = side_attacking;

  return result;
}

static boolean is_square_observed_by_opponent(PieNam p, square sq_departure)
{
  boolean result;

  nextply(advers(trait[nbply]));
  current_move[nbply] = current_move[nbply-1]+1;
  move_generation_stack[current_move[nbply]].capture = sq_departure;
  move_generation_stack[current_move[nbply]].auxiliary.hopper.sq_hurdle = initsquare;
  result = (*checkfunctions[p])(sq_departure,p,&validate_observation);
  finply();

  return result;
}

/* Generate moves of a potentially transmuting king
 * @param si identifies move generator slice
 * @param sq_departure common departure square of the generated moves
 * @return true iff the king is transmuting (which doesn't necessarily mean that
 *              any moves were generated!)
 */
boolean generate_moves_of_transmuting_king(slice_index si, square sq_departure)
{
  boolean result = false;
  Side const side_moving = trait[nbply];
  Side const side_transmuting = advers(side_moving);

  PieNam const *ptrans;
  for (ptrans = transmpieces[side_moving]; *ptrans!=Empty; ++ptrans)
    if (number_of_pieces[side_transmuting][*ptrans]>0
        && is_square_observed_by_opponent(*ptrans,sq_departure))
    {
      generate_moves_for_piece(slices[si].next1,sq_departure,*ptrans);
      result = true;
    }

  return result;
}

/* Generate moves for a single piece
 * @param identifies generator slice
 * @param sq_departure departure square of generated moves
 * @param p walk to be used for generating
 */
void transmuting_kings_generate_moves_for_piece(slice_index si,
                                                square sq_departure,
                                                PieNam p)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceSquare(sq_departure);
  TracePiece(p);
  TraceFunctionParamListEnd();

  if (!(p==King && generate_moves_of_transmuting_king(si,sq_departure)))
    generate_moves_for_piece(slices[si].next1,sq_departure,p);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Inialise the solving machinery with transmuting kings
 * @param si identifies root slice of solving machinery
 * @param side for whom
 */
void transmuting_kings_initialise_solving(slice_index si, Side side)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceEnumerator(Side,side,"");
  TraceFunctionParamListEnd();

  solving_instrument_move_generation(si,side,STTransmutingKingsMovesForPieceGenerator);
  instrument_alternative_is_square_observed_king_testing(si,side,STTransmutingKingIsSquareObserved);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Determine whether a square is observed be the side at the move according to
 * Transmuting Kings
 * @param si identifies next slice
 * @param sq_target the square
 * @return true iff sq_target is observed by the side at the move
 */
boolean transmuting_king_is_square_observed(slice_index si,
                                            square sq_target,
                                            evalfunction_t *evaluate)
{
  if (number_of_pieces[trait[nbply]][King]>0)
  {
    Side const side_attacked = advers(trait[nbply]);

    PieNam *ptrans;
    PieNam transmuter = Empty;

    for (ptrans = transmpieces[trait[nbply]]; *ptrans; ptrans++)
      if (number_of_pieces[side_attacked][*ptrans]>0
          && is_king_transmuting_as(*ptrans,evaluate))
      {
        if ((*checkfunctions[*ptrans])(sq_target,King,evaluate))
          return true;
        else
          transmuter = *ptrans;
      }

    if (transmuter!=Empty)
      return is_square_observed_recursive(slices[si].next2,sq_target,evaluate);
  }

  return is_square_observed_recursive(slices[si].next1,sq_target,evaluate);
}

typedef struct
{
    Side side;
    slice_type type;
    slice_index after_king;
    slice_index inserted;
} instrumentation_type;

static void instrument_testing(slice_index si, stip_structure_traversal *st)
{
  instrumentation_type * const it = st->param;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  assert(it->after_king==no_slice);

  if (it->side==nr_sides || it->side==slices[si].starter)
    is_square_observed_insert_slice(si,it->type);

  stip_traverse_structure_children_pipe(si,st);

  it->after_king = no_slice;

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static void remember_after_king(slice_index si, stip_structure_traversal *st)
{
  instrumentation_type * const it = st->param;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  stip_traverse_structure_children_pipe(si,st);

  it->after_king = si;

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static void connect_to_after_king(slice_index si, stip_structure_traversal *st)
{
  instrumentation_type * const it = st->param;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  stip_traverse_structure_children_pipe(si,st);

  if (slices[si].next2==no_slice)
  {
    slices[si].next2 = alloc_proxy_slice();
    link_to_branch(slices[si].next2,it->after_king);
    it->inserted = si;
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Instrument the square observation machinery for a side with an alternative
 * slice dealting with observations by kings.
 * @param si identifies the root slice of the solving machinery
 * @param side side for which to instrument the square observation machinery
 * @param type type of slice to insert
 * @return the inserted slice's identifier
 * @note next2 of inserted slices will be set to the position behind the
 *       regular square observation by king handler
 */
slice_index instrument_alternative_is_square_observed_king_testing(slice_index si,
                                                                   Side side,
                                                                   slice_type type)
{
  instrumentation_type it = { side, type, no_slice, no_slice };
  stip_structure_traversal st;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceEnumerator(Side,side,"");
  TraceEnumerator(slice_type,type,"");
  TraceFunctionParamListEnd();

  stip_structure_traversal_init(&st,&it);
  stip_structure_traversal_override_single(&st,
                                           STLandingAfterFindSquareObserverTrackingBackKing,
                                           &remember_after_king);
  stip_structure_traversal_override_single(&st,type,&connect_to_after_king);
  stip_structure_traversal_override_single(&st,
                                           STTestingIfSquareIsObserved,
                                           &instrument_testing);
  stip_traverse_structure(si,&st);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",it.inserted);
  TraceFunctionResultEnd();
  return it.inserted;
}
