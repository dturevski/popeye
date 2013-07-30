#include "conditions/woozles.h"
#include "pydata.h"
#include "stipulation/stipulation.h"
#include "stipulation/pipe.h"
#include "stipulation/branch.h"
#include "solving/move_generator.h"
#include "solving/observation.h"
#include "debugging/trace.h"

#include <stdlib.h>

boolean woozles_rex_exclusive;

static square sq_woo_from;
static square sq_woo_to;

static PieNam woozlers[PieceCount];

static void init_woozlers(void)
{
  unsigned int i = 0;
  PieNam p;

  for (p = King; p<PieceCount; ++p) {
    if (may_exist[p] && p!=Dummy && p!=Hamster)
    {
      woozlers[i] = p;
      i++;
    }
  }

  woozlers[i] = Empty;
}

static boolean woozles_aux_whx(square sq_departure,
                               square sq_arrival,
                               square sq_capture)
{
  return (sq_departure==sq_woo_from
          && validate_observation_geometry(sq_departure,sq_arrival,sq_capture));
}

static boolean heffalumps_aux_whx(square sq_departure,
                                  square sq_arrival,
                                  square sq_capture)
{
  if (sq_departure==sq_woo_from)
  {
    int cd1= sq_departure%onerow - sq_arrival%onerow;
    int rd1= sq_departure/onerow - sq_arrival/onerow;
    int cd2= sq_woo_to%onerow - sq_departure%onerow;
    int rd2= sq_woo_to/onerow - sq_departure/onerow;
    int t= 7;

    if (cd1 != 0)
      t= abs(cd1);
    if (rd1 != 0 && t > abs(rd1))
      t= abs(rd1);

    while (!(cd1%t == 0 && rd1%t == 0))
      t--;
    cd1= cd1/t;
    rd1= rd1/t;

    t= 7;
    if (cd2 != 0)
      t= abs(cd2);
    if (rd2 != 0 && t > abs(rd2))
      t= abs(rd2);

    while (!(cd2%t == 0 && rd2%t == 0))
      t--;

    cd2= cd2/t;
    rd2= rd2/t;

    if ((cd1==cd2 && rd1==rd2) || (cd1==-cd2 && rd1==-rd2))
      return validate_observation_geometry(sq_departure,sq_arrival,sq_capture);
  }

  return false;
}

static boolean woozles_aux_wh(square sq_departure,
                              square sq_arrival,
                              square sq_capture)
{
  boolean result = false;

  if (validate_observation_geometry(sq_departure,sq_arrival,sq_capture))
  {
    Side const side_woozled = trait[parent_ply[nbply]];
    PieNam const p = get_walk_of_piece_on_square(sq_woo_from);
    if (number_of_pieces[side_woozled][p]>0)
    {
      Side save_trait = trait[nbply];
      trait[nbply] = side_woozled;
      result = (*checkfunctions[p])(sq_departure,p,&woozles_aux_whx);
      trait[nbply] = save_trait;
    }
  }

  return result;
}

static boolean heffalumps_aux_wh(square sq_departure,
                                 square sq_arrival,
                                 square sq_capture)
{
  boolean result = false;

  if (validate_observation_geometry(sq_departure,sq_arrival,sq_capture))
  {
    Side const side_woozled = trait[parent_ply[nbply]];
    PieNam const p = get_walk_of_piece_on_square(sq_woo_from);
    if (number_of_pieces[side_woozled][p]>0)
    {
      Side save_trait = trait[nbply];
      trait[nbply] = side_woozled;
      result = (*checkfunctions[p])(sq_departure,p,&heffalumps_aux_whx);
      trait[nbply] = save_trait;
    }
  }

  return result;
}

static boolean woozles_is_paralysed(Side side_woozle, square to, square sq_observer)
{
  Side const side_woozled = trait[nbply];
  boolean result = false;

  if (!woozles_rex_exclusive || sq_observer!=king_square[side_woozled])
  {
    PieNam const *pcheck = woozlers;

    if (woozles_rex_exclusive)
      ++pcheck;

    sq_woo_from = sq_observer;
    sq_woo_to = to;

    nextply(side_woozle);
    current_move[nbply] = current_move[nbply-1]+1;
    move_generation_stack[current_move[nbply]].auxiliary.hopper.sq_hurdle = initsquare;
    move_generation_stack[current_move[nbply]].capture = sq_observer;

    for (; *pcheck; ++pcheck)
      if (number_of_pieces[side_woozle][*pcheck]>0
          && (*checkfunctions[*pcheck])(sq_observer,*pcheck,&woozles_aux_wh))
      {
        result = true;
        break;
      }

    finply();
  }

  return result;
}

static boolean heffalumps_is_paralysed(Side side_woozle, square to, square sq_observer)
{
  Side const side_woozled = trait[nbply];
  boolean result = false;

  if (!woozles_rex_exclusive || sq_observer!=king_square[side_woozled])
  {
    PieNam const *pcheck = woozlers;

    if (woozles_rex_exclusive)
      ++pcheck;

    sq_woo_from = sq_observer;
    sq_woo_to = to;

    nextply(side_woozle);
    current_move[nbply] = current_move[nbply-1]+1;
    move_generation_stack[current_move[nbply]].auxiliary.hopper.sq_hurdle = initsquare;

    for (; *pcheck; ++pcheck)
      if (number_of_pieces[side_woozle][*pcheck]>0)
      {
        move_generation_stack[current_move[nbply]].capture = sq_observer;
        if ((*checkfunctions[*pcheck])(sq_observer,*pcheck,&heffalumps_aux_wh))
        {
          result = true;
          break;
        }
      }

    finply();
  }

  return result;
}

/* Validate an observation according to Woozles
 * @param sq_observer position of the observer
 * @param sq_landing landing square of the observer (normally==sq_observee)
 * @param sq_observee position of the piece to be observed
 * @return true iff the observation is valid
 */
boolean woozles_validate_observation(slice_index si,
                                     square sq_observer,
                                     square sq_landing,
                                     square sq_observee)
{
  boolean result;
  Side const side_woozle = trait[nbply];

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceSquare(sq_observer);
  TraceSquare(sq_landing);
  TraceSquare(sq_observee);
  TraceFunctionParamListEnd();

  result = (!woozles_is_paralysed(side_woozle,sq_landing,sq_observer)
            && validate_observation_recursive(slices[si].next1,
                                              sq_observer,
                                              sq_landing,
                                              sq_observee));

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Validate an observation according to BiWoozles
 * @param sq_observer position of the observer
 * @param sq_landing landing square of the observer (normally==sq_observee)
 * @param sq_observee position of the piece to be observed
 * @return true iff the observation is valid
 */
boolean biwoozles_validate_observation(slice_index si,
                                       square sq_observer,
                                       square sq_landing,
                                       square sq_observee)
{
  boolean result;
  Side const side_woozle = advers(trait[nbply]);

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceSquare(sq_observer);
  TraceSquare(sq_landing);
  TraceSquare(sq_observee);
  TraceFunctionParamListEnd();

  result = (!woozles_is_paralysed(side_woozle,sq_landing,sq_observer)
            && validate_observation_recursive(slices[si].next1,
                                              sq_observer,
                                              sq_landing,
                                              sq_observee));

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Validate an observation according to Heffalumps
 * @param sq_observer position of the observer
 * @param sq_landing landing square of the observer (normally==sq_observee)
 * @param sq_observee position of the piece to be observed
 * @return true iff the observation is valid
 */
boolean heffalumps_validate_observation(slice_index si,
                                        square sq_observer,
                                        square sq_landing,
                                        square sq_observee)
{
  boolean result;
  Side const side_woozle = CondFlag[biheffalumps] ? advers(trait[nbply]) : trait[nbply];

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceSquare(sq_observer);
  TraceSquare(sq_landing);
  TraceSquare(sq_observee);
  TraceFunctionParamListEnd();

  result = (!heffalumps_is_paralysed(side_woozle,sq_landing,sq_observer)
            && validate_observation_recursive(slices[si].next1,
                                              sq_observer,
                                              sq_landing,
                                              sq_observee));

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Validate an observation according to BiHeffalumps
 * @param sq_observer position of the observer
 * @param sq_landing landing square of the observer (normally==sq_observee)
 * @param sq_observee position of the piece to be observed
 * @return true iff the observation is valid
 */
boolean biheffalumps_validate_observation(slice_index si,
                                          square sq_observer,
                                          square sq_landing,
                                          square sq_observee)
{
  boolean result;
  Side const side_woozle = advers(trait[nbply]);

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceSquare(sq_observer);
  TraceSquare(sq_landing);
  TraceSquare(sq_observee);
  TraceFunctionParamListEnd();

  result = (!heffalumps_is_paralysed(side_woozle,sq_landing,sq_observer)
            && validate_observation_recursive(slices[si].next1,
                                              sq_observer,
                                              sq_landing,
                                              sq_observee));

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

static boolean woozles_is_not_illegal_capture(square sq_departure,
                                              square sq_arrival,
                                              square sq_capture)
{
  boolean result;
  Side const side_woozle = trait[nbply];

  TraceFunctionEntry(__func__);
  TraceSquare(sq_departure);
  TraceSquare(sq_arrival);
  TraceSquare(sq_capture);
  TraceFunctionParamListEnd();

  result = !(!is_square_empty(sq_capture)
             && woozles_is_paralysed(side_woozle,sq_arrival,sq_departure));

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Try to solve in n half-moves.
 * @param si slice index
 * @param n maximum number of half moves
 * @return length of solution found and written, i.e.:
 *            previous_move_is_illegal the move just played (or being played)
 *                                     is illegal
 *            immobility_on_next_move  the moves just played led to an
 *                                     unintended immobility on the next move
 *            <=n+1 length of shortest solution found (n+1 only if in next
 *                                     branch)
 *            n+2 no solution found in this branch
 *            n+3 no solution found in next branch
 */
stip_length_type woozles_remove_illegal_captures_solve(slice_index si,
                                                       stip_length_type n)
{
  stip_length_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  move_generator_filter_moves(&woozles_is_not_illegal_capture);

  result = solve(slices[si].next1,n);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

static void woozles_insert_remover(slice_index si, stip_structure_traversal *st)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  stip_traverse_structure_children(si,st);

  {
    slice_index const prototype = alloc_pipe(STWoozlesRemoveIllegalCaptures);
    branch_insert_slices_contextual(si,st->context,&prototype,1);
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Instrument solving in Woozles
 * @param si identifies the root slice of the stipulation
 */
void woozles_initialise_solving(slice_index si)
{
  stip_structure_traversal st;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  TraceStipulation(si);

  stip_structure_traversal_init(&st,0);
  stip_structure_traversal_override_single(&st,
                                           STDoneGeneratingMoves,
                                           &woozles_insert_remover);
  stip_traverse_structure(si,&st);

  stip_instrument_observation_validation(si,nr_sides,STValidateObservationWoozles);

  init_woozlers();

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static boolean biwoozles_is_not_illegal_capture(square sq_departure,
                                                square sq_arrival,
                                                square sq_capture)
{
  boolean result;
  Side const side_woozle = advers(trait[nbply]);

  TraceFunctionEntry(__func__);
  TraceSquare(sq_departure);
  TraceSquare(sq_arrival);
  TraceSquare(sq_capture);
  TraceFunctionParamListEnd();

  result = !(!is_square_empty(sq_capture)
             && woozles_is_paralysed(side_woozle,sq_arrival, sq_departure));

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Try to solve in n half-moves.
 * @param si slice index
 * @param n maximum number of half moves
 * @return length of solution found and written, i.e.:
 *            previous_move_is_illegal the move just played (or being played)
 *                                     is illegal
 *            immobility_on_next_move  the moves just played led to an
 *                                     unintended immobility on the next move
 *            <=n+1 length of shortest solution found (n+1 only if in next
 *                                     branch)
 *            n+2 no solution found in this branch
 *            n+3 no solution found in next branch
 */
stip_length_type biwoozles_remove_illegal_captures_solve(slice_index si,
                                                         stip_length_type n)
{
  stip_length_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  move_generator_filter_moves(&biwoozles_is_not_illegal_capture);

  result = solve(slices[si].next1,n);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

static void biwoozles_insert_remover(slice_index si, stip_structure_traversal *st)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  stip_traverse_structure_children(si,st);

  {
    slice_index const prototype = alloc_pipe(STBiWoozlesRemoveIllegalCaptures);
    branch_insert_slices_contextual(si,st->context,&prototype,1);
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Instrument solving in BiWoozles
 * @param si identifies the root slice of the stipulation
 */
void biwoozles_initialise_solving(slice_index si)
{
  stip_structure_traversal st;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  TraceStipulation(si);

  stip_structure_traversal_init(&st,0);
  stip_structure_traversal_override_single(&st,
                                           STDoneGeneratingMoves,
                                           &biwoozles_insert_remover);
  stip_traverse_structure(si,&st);

  stip_instrument_observation_validation(si,nr_sides,STValidateObservationBiWoozles);

  init_woozlers();

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static boolean heffalumps_is_not_illegal_capture(square sq_departure,
                                                 square sq_arrival,
                                                 square sq_capture)
{
  boolean result;
  Side const side_woozle = trait[nbply];

  TraceFunctionEntry(__func__);
  TraceSquare(sq_departure);
  TraceSquare(sq_arrival);
  TraceSquare(sq_capture);
  TraceFunctionParamListEnd();

  result = !(!is_square_empty(sq_capture)
             && heffalumps_is_paralysed(side_woozle,sq_arrival, sq_departure));

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Try to solve in n half-moves.
 * @param si slice index
 * @param n maximum number of half moves
 * @return length of solution found and written, i.e.:
 *            previous_move_is_illegal the move just played (or being played)
 *                                     is illegal
 *            immobility_on_next_move  the moves just played led to an
 *                                     unintended immobility on the next move
 *            <=n+1 length of shortest solution found (n+1 only if in next
 *                                     branch)
 *            n+2 no solution found in this branch
 *            n+3 no solution found in next branch
 */
stip_length_type heffalumps_remove_illegal_captures_solve(slice_index si,
                                                          stip_length_type n)
{
  stip_length_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  move_generator_filter_moves(&heffalumps_is_not_illegal_capture);

  result = solve(slices[si].next1,n);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

static void heffalumps_insert_remover(slice_index si, stip_structure_traversal *st)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  stip_traverse_structure_children(si,st);

  {
    slice_index const prototype = alloc_pipe(STHeffalumpsRemoveIllegalCaptures);
    branch_insert_slices_contextual(si,st->context,&prototype,1);
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Instrument solving in Heffalumps
 * @param si identifies the root slice of the stipulation
 */
void heffalumps_initialise_solving(slice_index si)
{
  stip_structure_traversal st;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  TraceStipulation(si);

  stip_structure_traversal_init(&st,0);
  stip_structure_traversal_override_single(&st,
                                           STDoneGeneratingMoves,
                                           &heffalumps_insert_remover);
  stip_traverse_structure(si,&st);

  stip_instrument_observation_validation(si,nr_sides,STValidateObservationHeffalumps);

  init_woozlers();

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static boolean biheffalumps_is_not_illegal_capture(square sq_departure,
                                                   square sq_arrival,
                                                   square sq_capture)
{
  boolean result;
  Side const side_woozle = advers(trait[nbply]);

  TraceFunctionEntry(__func__);
  TraceSquare(sq_departure);
  TraceSquare(sq_arrival);
  TraceSquare(sq_capture);
  TraceFunctionParamListEnd();

  result = !(!is_square_empty(sq_capture)
             && heffalumps_is_paralysed(side_woozle,sq_arrival, sq_departure));

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Try to solve in n half-moves.
 * @param si slice index
 * @param n maximum number of half moves
 * @return length of solution found and written, i.e.:
 *            previous_move_is_illegal the move just played (or being played)
 *                                     is illegal
 *            immobility_on_next_move  the moves just played led to an
 *                                     unintended immobility on the next move
 *            <=n+1 length of shortest solution found (n+1 only if in next
 *                                     branch)
 *            n+2 no solution found in this branch
 *            n+3 no solution found in next branch
 */
stip_length_type biheffalumps_remove_illegal_captures_solve(slice_index si,
                                                            stip_length_type n)
{
  stip_length_type result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  move_generator_filter_moves(&biheffalumps_is_not_illegal_capture);

  result = solve(slices[si].next1,n);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

static void biheffalumps_insert_remover(slice_index si, stip_structure_traversal *st)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  stip_traverse_structure_children(si,st);

  {
    slice_index const prototype = alloc_pipe(STBiHeffalumpsRemoveIllegalCaptures);
    branch_insert_slices_contextual(si,st->context,&prototype,1);
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Instrument solving in BiHeffalumps
 * @param si identifies the root slice of the stipulation
 */
void biheffalumps_initialise_solving(slice_index si)
{
  stip_structure_traversal st;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  TraceStipulation(si);

  stip_structure_traversal_init(&st,0);
  stip_structure_traversal_override_single(&st,
                                           STDoneGeneratingMoves,
                                           &biheffalumps_insert_remover);
  stip_traverse_structure(si,&st);

  stip_instrument_observation_validation(si,nr_sides,STValidateObservationBiHeffalumps);

  init_woozlers();

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}
