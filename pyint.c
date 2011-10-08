/******************** MODIFICATIONS to pyint.c **************************
 **
 ** Date       Who  What
 **
 ** 2006/06/14 TLi  bug fix in function guards_black_flight()
 **
 ** 2007/12/27 TLi  bug fix in function stalemate_immobilise_black()
 **
 **************************** End of List ******************************/

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#include "py.h"
#include "pyproc.h"
#include "pyhash.h"
#include "pyint.h"
#include "pydata.h"
#include "pyslice.h"
#include "stipulation/help_play/branch.h"
#include "pybrafrk.h"
#include "pyproof.h"
#include "pypipe.h"
#include "optimisations/intelligent/guard_flights.h"
#include "optimisations/intelligent/moves_left.h"
#include "optimisations/intelligent/filter.h"
#include "optimisations/intelligent/proof.h"
#include "optimisations/intelligent/duplicate_avoider.h"
#include "optimisations/intelligent/limit_nr_solutions_per_target.h"
#include "optimisations/intelligent/mate/generate_checking_moves.h"
#include "optimisations/intelligent/count_nr_of_moves.h"
#include "stipulation/branch.h"
#include "stipulation/temporary_hacks.h"
#include "platform/maxtime.h"
#include "trace.h"

typedef unsigned int index_type;

#define piece_usageENUMERATORS \
    ENUMERATOR(piece_is_unused), \
    ENUMERATOR(piece_pins), \
    ENUMERATOR(piece_is_fixed_to_diagram_square), \
    ENUMERATOR(piece_intercepts), \
    ENUMERATOR(piece_blocks), \
    ENUMERATOR(piece_guards), \
    ENUMERATOR(piece_gives_check), \
    ENUMERATOR(piece_is_missing), \
    ENUMERATOR(piece_is_king)

#define ENUMERATORS piece_usageENUMERATORS
#define ENUMERATION_TYPENAME piece_usage
#define ENUMERATION_MAKESTRINGS
#include "pyenum.h"


goal_type goal_to_be_reached;

unsigned int MaxPiece[nr_sides];

PIECE white[nr_squares_on_board];
PIECE black[nr_squares_on_board];
static square save_ep_1;
static square save_ep2_1;
unsigned int moves_to_white_prom[nr_squares_on_board];
square const *where_to_start_placing_unused_black_pieces;

PIECE target_position[MaxPieceId+1];

slice_index current_start_slice = no_slice;

boolean solutions_found;

boolean testcastling;

unsigned int MovesRequired[nr_sides][maxply+1];
unsigned int CapturesLeft[maxply+1];

unsigned int PieceId2index[MaxPieceId+1];

unsigned int nr_reasons_for_staying_empty[maxsquare+4];

unsigned int Nr_remaining_white_moves;
unsigned int Nr_remaining_black_moves;
unsigned int Nr_unused_black_masses;
unsigned int Nr_unused_white_masses;


void remember_to_keep_rider_line_open(square from, square to,
                                      int dir, int delta)
{
  square s;

  TraceFunctionEntry(__func__);
  TraceSquare(from);
  TraceSquare(to);
  TraceFunctionParam("%d",dir);
  TraceFunctionParam("%d",delta);
  TraceFunctionParamListEnd();

  for (s = from+dir; s!=to; s+=dir)
  {
    assert(e[s]==vide);
    nr_reasons_for_staying_empty[s] += delta;
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

boolean rider_guards(square to_be_guarded, square guarding_from, int dir)
{
  boolean result = false;

  if (dir!=0)
  {
    square tmp = guarding_from;
    do
    {
      tmp += dir;
      if (tmp==to_be_guarded)
      {
        result = true;
        break;
      }
    } while (e[tmp]==vide);
  }

  return result;
}

boolean guards(square to_be_guarded, piece guarding, square guarding_from)
{
  boolean result;
  int const diff = to_be_guarded-guarding_from;

  TraceFunctionEntry(__func__);
  TraceSquare(to_be_guarded);
  TracePiece(guarding);
  TraceSquare(guarding_from);
  TraceFunctionParamListEnd();

  switch (abs(guarding))
  {
    case Pawn:
      result = (guarding_from>=square_a2
                && (diff==+dir_up+dir_left || diff==+dir_up+dir_right));
      break;

    case Knight:
      result = CheckDirKnight[diff]!=0;
      break;

    case Bishop:
      result = rider_guards(to_be_guarded,guarding_from,CheckDirBishop[diff]);
      break;

    case Rook:
      result = rider_guards(to_be_guarded,guarding_from,CheckDirRook[diff]);
      break;

    case Queen:
      result = (rider_guards(to_be_guarded,guarding_from,CheckDirBishop[diff])
                || rider_guards(to_be_guarded,guarding_from,CheckDirRook[diff]));
      break;

    case King:
      result = move_diff_code[abs(diff)]<3;
      break;

    default:
      assert(0);
      result = false;
      break;
  }


  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Determine whether the white king would guard if it were placed on a
 * particular square
 * @param white_king_square square where white king would be placed
 * @return true iff the white king would guard from white_king_square
 */
boolean would_white_king_guard_from(square white_king_square)
{
  boolean result;

  TraceFunctionEntry(__func__);
  TraceFunctionParamListEnd();

  result = move_diff_code[abs(white_king_square-king_square[Black])]<9;

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

boolean uninterceptably_attacks_king(Side side, square from, piece p)
{
  boolean result;

  TraceFunctionEntry(__func__);
  TraceSquare(from);
  TracePiece(p);
  TraceFunctionParamListEnd();

  if (king_square[side]==initsquare)
    result = false;
  else
  {
    int const dir = king_square[side]-from;
    switch(p)
    {
      case db:
      case dn:
        result = CheckDirQueen[dir]==dir;
        break;

      case tb:
      case tn:
        result = CheckDirRook[dir]==dir;
        break;

      case fb:
      case fn:
        result = CheckDirBishop[dir]==dir;
        break;

      case cb:
      case cn:
        result = CheckDirKnight[dir]!=0;
        break;

      case pb:
        result = dir==dir_up+dir_right || dir==dir_up+dir_left;
        break;

      case pn:
        result = dir==dir_down+dir_right || dir==dir_down+dir_left;
        break;

      default:
        assert(0);
        result = false;
        break;
    }
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

boolean is_white_king_interceptably_attacked(void)
{
  return ((*checkfunctions[Bishop])(king_square[White],fn,eval_ortho)
          || (*checkfunctions[Rook])(king_square[White],tn,eval_ortho)
          || (*checkfunctions[Queen])(king_square[White],dn,eval_ortho));
}

boolean is_white_king_uninterceptably_attacked_by_non_king(square s)
{
  return ((*checkfunctions[Pawn])(s,pn,eval_ortho)
          || (*checkfunctions[Knight])(s,cn,eval_ortho)
          || (*checkfunctions[Fers])(s,fn,eval_ortho)
          || (*checkfunctions[Wesir])(s,tn,eval_ortho)
          || (*checkfunctions[ErlKing])(s,dn,eval_ortho));
}

/*#define DETAILS*/
#if defined(DETAILS)
static void trace_target_position(PIECE const position[MaxPieceId+1],
                                  unsigned int nr_required_captures)
{
  unsigned int moves_per_side[nr_sides] = { 0, 0 };
  square const *bnp;

  for (bnp = boardnum; *bnp!=initsquare; bnp++)
    if (e[*bnp]!=vide)
    {
      Flags const sp = spec[*bnp];
      PieceIdType const id = GetPieceId(sp);
      PIECE const * const target = &position[id];
      if (target->square!=vide)
      {
        unsigned int const time = intelligent_count_nr_of_moves_from_to_no_check(e[*bnp],
                                                                     *bnp,
                                                                     target->type,
                                                                     target->square);
        moves_per_side[e[*bnp]<vide ? Black : White] += time;
        TracePiece(e[*bnp]);
        TraceSquare(*bnp);
        TracePiece(target->type);
        TraceSquare(target->square);
        TraceEnumerator(piece_usage,target->usage,"");
        TraceValue("%u\n",time);
      }
    }

  TraceValue("%u",nr_required_captures);
  TraceValue("%u",moves_per_side[Black]);
  TraceValue("%u\n",moves_per_side[White]);
}

static piece_usage find_piece_usage(PieceIdType id)
{
  piece_usage result = piece_is_unused;

  unsigned int i;
  for (i = 0; i<MaxPiece[White]; ++i)
    if (id==GetPieceId(white[i].flags))
    {
      result = white[i].usage;
      break;
    }
  for (i = 0; i<MaxPiece[Black]; ++i)
    if (id==GetPieceId(black[i].flags))
    {
      result = black[i].usage;
      break;
    }

  assert(result!=piece_is_unused);

  return result;
}
#endif

void solve_target_position(stip_length_type n)
{
  square const save_king_square[nr_sides] = { king_square[White],
                                              king_square[Black] };

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  {
    PieceIdType id;
    for (id = 0; id<=MaxPieceId; ++id)
      target_position[id].diagram_square = initsquare;
  }

  {
    square const *bnp;
    for (bnp = boardnum; *bnp!=initsquare; bnp++)
    {
      piece const type = e[*bnp];
      if (type!=vide && type!=obs)
      {
        Flags const flags = spec[*bnp];
        PieceIdType const id = GetPieceId(flags);
        target_position[id].type = type;
        target_position[id].flags = flags;
        target_position[id].diagram_square = *bnp;
#if defined(DETAILS)
        target_position[id].usage = find_piece_usage(id);
#endif
      }
    }
  }

  /* solve the problem */
  ResetPosition();

  castling_supported = true;

  ep[1] = save_ep_1;
  ep2[1] = save_ep2_1;

#if defined(DETAILS)
  TraceText("target position:\n");
  trace_target_position(target_position,CapturesLeft[1]);
#endif

  reset_nr_solutions_per_target_position();

  closehash();
  inithash(current_start_slice);

  if (help(slices[current_start_slice].u.pipe.next,n)<=n)
    solutions_found = true;

  /* reset the old mating position */
  {
    square const *bnp;
    for (bnp = boardnum; *bnp!=initsquare; bnp++)
    {
      e[*bnp] = vide;
      spec[*bnp] = EmptySpec;
    }
  }

  {
    PieceIdType id;
    for (id = 0; id<=MaxPieceId; ++id)
      if (target_position[id].diagram_square != initsquare)
      {
        e[target_position[id].diagram_square] = target_position[id].type;
        spec[target_position[id].diagram_square] = target_position[id].flags;
      }
  }

  {
    int p;
    for (p = King; p<=Bishop; ++p)
    {
      nbpiece[-p] = 2;
      nbpiece[p] = 2;
    }
  }

  king_square[White] = save_king_square[White];
  king_square[Black] = save_king_square[Black];

  ep[1] = initsquare;
  ep2[1] = initsquare;

  castling_supported = false;

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

unsigned int find_check_directions(Side side, int check_directions[8])
{
  unsigned int result = 0;
  unsigned int i;

  /* don't use obs - intelligent mode supports boards with holes */
  piece const temporary_block = dummyb;

  for (i = vec_queen_end; i>=vec_queen_start ; --i)
  {
    numvec const vec_i = vec[i];
    if (e[king_square[side]+vec_i]==vide)
      e[king_square[side]+vec_i] = temporary_block;
  }

  for (i = vec_queen_end; i>=vec_queen_start ; --i)
  {
    numvec const vec_i = vec[i];
    if (e[king_square[side]+vec_i]==temporary_block)
    {
      e[king_square[side]+vec_i] = vide;
      if (echecc(nbply,side))
      {
        check_directions[result] = vec_i;
        ++result;
      }
      e[king_square[side]+vec_i] = temporary_block;
    }
  }

  for (i = vec_queen_end; i>=vec_queen_start ; --i)
  {
    numvec const vec_i = vec[i];
    if (e[king_square[side]+vec_i]==temporary_block)
      e[king_square[side]+vec_i] = vide;
  }

  return result;
}

static void GenerateBlackKing(stip_length_type n)
{
  Flags const king_flags = black[index_of_king].flags;
  square const *bnp;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  assert(black[index_of_king].type==roin);

  Nr_remaining_white_moves = MovesLeft[White];
  Nr_remaining_black_moves = MovesLeft[Black];
  TraceValue("%u",Nr_remaining_white_moves);
  TraceValue("%u\n",Nr_remaining_black_moves);

  Nr_unused_black_masses = MaxPiece[Black]-1;
  TraceValue("%u\n",Nr_unused_black_masses);

  for (bnp = boardnum; *bnp!=initsquare && !hasMaxtimeElapsed(); ++bnp)
  {
    TraceSquare(*bnp);TraceText("\n");
    if (e[*bnp]!=obs)
    {
      unsigned int const time = intelligent_count_nr_of_moves_from_to_king(roin,
                                                                           black[index_of_king].diagram_square,
                                                                           *bnp);
      if (time<=Nr_remaining_black_moves)
      {
        Nr_remaining_black_moves -= time;
        TraceValue("%u\n",Nr_remaining_black_moves);

        {
          square s;
          for (s = 0; s!=maxsquare+4; ++s)
          {
            if (nr_reasons_for_staying_empty[s]>0)
              WriteSquare(s);
            assert(nr_reasons_for_staying_empty[s]==0);
          }
        }

        SetPiece(roin,*bnp,king_flags);
        king_square[Black] = *bnp;
        black[index_of_king].usage = piece_is_king;
        if (goal_to_be_reached==goal_mate)
        {
          intelligent_mate_generate_checking_moves(1,0,n);
          intelligent_mate_generate_checking_moves(2,0,n);
        }
        else
          intelligent_guard_flights(n);

        e[*bnp] = vide;
        spec[*bnp] = EmptySpec;

        Nr_remaining_black_moves += time;
      }
    }
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

void IntelligentRegulargoal_types(stip_length_type n)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",n);
  TraceFunctionParamListEnd();

  testcastling =
      TSTCASTLINGFLAGMASK(0,White,q_castling&castling_flag[castlings_flags_no_castling])==q_castling
      || TSTCASTLINGFLAGMASK(0,White,k_castling&castling_flag[castlings_flags_no_castling])==k_castling
      || TSTCASTLINGFLAGMASK(0,Black,q_castling&castling_flag[castlings_flags_no_castling])==q_castling
      || TSTCASTLINGFLAGMASK(0,Black,k_castling&castling_flag[castlings_flags_no_castling])==k_castling;

  where_to_start_placing_unused_black_pieces = boardnum;

  assert(castling_supported);
  castling_supported = false;

  save_ep_1 = ep[1];
  save_ep2_1 = ep2[1];

  MaxPiece[Black] = 0;
  MaxPiece[White] = 0;

  black[index_of_king].type= e[king_square[Black]];
  black[index_of_king].flags= spec[king_square[Black]];
  black[index_of_king].diagram_square= king_square[Black];
  PieceId2index[GetPieceId(spec[king_square[Black]])] = index_of_king;
  ++MaxPiece[Black];

  if (king_square[White]==initsquare)
    white[index_of_king].usage = piece_is_missing;
  else
  {
    white[index_of_king].usage = piece_is_unused;
    white[index_of_king].type = e[king_square[White]];
    white[index_of_king].flags = spec[king_square[White]];
    white[index_of_king].diagram_square = king_square[White];
    PieceId2index[GetPieceId(spec[king_square[White]])] = index_of_king;
    assert(white[index_of_king].type==roib);
  }

  ++MaxPiece[White];

  {
    square const *bnp;
    for (bnp = boardnum; *bnp!=initsquare; ++bnp)
      if (king_square[White]!=*bnp && e[*bnp]>obs)
      {
        white[MaxPiece[White]].type = e[*bnp];
        white[MaxPiece[White]].flags = spec[*bnp];
        white[MaxPiece[White]].diagram_square = *bnp;
        white[MaxPiece[White]].usage = piece_is_unused;
        if (e[*bnp]==pb)
          moves_to_white_prom[MaxPiece[White]] = intelligent_count_moves_to_white_promotion(*bnp);
        PieceId2index[GetPieceId(spec[*bnp])] = MaxPiece[White];
        ++MaxPiece[White];
      }

    for (bnp = boardnum; *bnp!=initsquare; ++bnp)
      if (king_square[Black]!=*bnp && e[*bnp]<vide)
      {
        black[MaxPiece[Black]].type = e[*bnp];
        black[MaxPiece[Black]].flags = spec[*bnp];
        black[MaxPiece[Black]].diagram_square = *bnp;
        black[MaxPiece[Black]].usage = piece_is_unused;
        PieceId2index[GetPieceId(spec[*bnp])] = MaxPiece[Black];
        ++MaxPiece[Black];
      }
  }

  StorePosition();
  ep[1] = initsquare;
  ep[1] = initsquare;

  /* clear board */
  {
    square const *bnp;
    for (bnp= boardnum; *bnp!=initsquare; ++bnp)
      if (e[*bnp] != obs)
      {
        e[*bnp]= vide;
        spec[*bnp]= EmptySpec;
      }
  }

  {
    piece p;
    for (p = roib; p<=fb; ++p)
    {
      nbpiece[p] = 2;
      nbpiece[-p] = 2;
    }
  }

  /* generate final positions */
  GenerateBlackKing(n);

  ResetPosition();

  castling_supported = true;
  ep[1] = save_ep_1;
  ep2[1] = save_ep2_1;

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static void goal_to_be_reached_goal(slice_index si,
                                    stip_structure_traversal *st)
{
  goal_type * const goal = st->param;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  assert(*goal==no_goal);
  *goal = slices[si].u.goal_tester.goal.type;

  stip_traverse_structure_children(si,st);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* Initialise the variable holding the goal to be reached
 */
static goal_type determine_goal_to_be_reached(slice_index si)
{
  stip_structure_traversal st;
  goal_type result = no_goal;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  TraceStipulation(si);

  stip_structure_traversal_init(&st,&result);
  stip_structure_traversal_override_single(&st,
                                           STGoalReachedTester,
                                           &goal_to_be_reached_goal);
  stip_structure_traversal_override_single(&st,
                                           STTemporaryHackFork,
                                           &stip_traverse_structure_pipe);
  stip_traverse_structure(si,&st);

  TraceValue("%u",goal_to_be_reached);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Initialise a STGoalReachableGuardFilter slice
 * @return identifier of allocated slice
 */
static slice_index alloc_goalreachable_guard_filter(goal_type goal)
{
  slice_index result;
  slice_type type;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",goal);
  TraceFunctionParamListEnd();

  switch (goal)
  {
    case goal_mate:
      type = STGoalReachableGuardFilterMate;
      break;

    case goal_stale:
      type = STGoalReachableGuardFilterStalemate;
      break;

    case goal_proofgame:
    case goal_atob:
      type = proof_make_goal_reachable_type();
      break;

    default:
      assert(0);
      type = no_slice_type;
      break;
  }

  if (type!=no_slice_type)
    result = alloc_pipe(type);
  else
    result = no_slice;

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

static
void goalreachable_guards_inserter_help_move(slice_index si,
                                             stip_structure_traversal *st)
{
  goal_type const * const goal = st->param;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  stip_traverse_structure_children(si,st);

  {
    slice_index const prototype = alloc_goalreachable_guard_filter(*goal);
    if (prototype!=no_slice)
      help_branch_insert_slices(si,&prototype,1);

    if (is_max_nr_solutions_per_target_position_limited())
    {
      slice_index const prototype = alloc_intelligent_limit_nr_solutions_per_target_position_slice();
      help_branch_insert_slices(si,&prototype,1);
    }
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static
void
goalreachable_guards_duplicate_avoider_inserter(slice_index si,
                                                stip_structure_traversal *st)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  stip_traverse_structure_children(si,st);

  if (slices[si].u.goal_tester.goal.type==goal_mate
      || slices[si].u.goal_tester.goal.type==goal_stale)
  {
    slice_index const prototype = alloc_intelligent_duplicate_avoider_slice();
    leaf_branch_insert_slices(si,&prototype,1);

    if (is_max_nr_solutions_per_target_position_limited())
    {
      slice_index const prototype = alloc_intelligent_nr_solutions_per_target_position_counter_slice();
      leaf_branch_insert_slices(si,&prototype,1);
    }
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static structure_traversers_visitors goalreachable_guards_inserters[] =
{
  { STReadyForHelpMove,          &goalreachable_guards_inserter_help_move         },
  { STGoalReachedTester,         &goalreachable_guards_duplicate_avoider_inserter },
  { STGoalImmobileReachedTester, &stip_traverse_structure_pipe                    },
  { STTemporaryHackFork,         &stip_traverse_structure_pipe                    }
};

enum
{
  nr_goalreachable_guards_inserters = (sizeof goalreachable_guards_inserters
                                       / sizeof goalreachable_guards_inserters[0])
};

/* Instrument stipulation with STgoal_typereachableGuard slices
 * @param si identifies slice where to start
 */
static void stip_insert_goalreachable_guards(slice_index si, goal_type goal)
{
  stip_structure_traversal st;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",goal);
  TraceFunctionParamListEnd();

  TraceStipulation(si);

  assert(goal!=no_goal);

  stip_structure_traversal_init(&st,&goal);
  stip_structure_traversal_override(&st,
                                    goalreachable_guards_inserters,
                                    nr_goalreachable_guards_inserters);
  stip_traverse_structure(si,&st);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static void intelligent_guards_inserter(slice_index si,
                                        stip_structure_traversal *st)
{
  goal_type const * const goal = st->param;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  if (*goal==goal_proofgame || *goal==goal_atob)
  {
    slice_index const prototype = alloc_intelligent_proof();
    help_branch_insert_slices(si,&prototype,1);
  }
  else
  {
    slice_index const prototype = alloc_intelligent_filter();
    help_branch_insert_slices(si,&prototype,1);
  }

  {
    slice_index const prototype = alloc_intelligent_moves_left_initialiser();
    help_branch_insert_slices(si,&prototype,1);
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static structure_traversers_visitors intelligent_filters_inserters[] =
{
  { STHelpAdapter,       &intelligent_guards_inserter  },
  { STTemporaryHackFork, &stip_traverse_structure_pipe }
};

enum
{
  nr_intelligent_filters_inserters = (sizeof intelligent_filters_inserters
                                     / sizeof intelligent_filters_inserters[0])
};

/* Instrument stipulation with STgoal_typereachableGuard slices
 * @param si identifies slice where to start
 */
static void stip_insert_intelligent_filters(slice_index si, goal_type goal)
{
  stip_structure_traversal st;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",goal);
  TraceFunctionParamListEnd();

  TraceStipulation(si);

  stip_structure_traversal_init(&st,&goal);
  stip_structure_traversal_override(&st,
                                    intelligent_filters_inserters,
                                    nr_intelligent_filters_inserters);
  stip_traverse_structure(si,&st);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

/* How well does the stipulation support intelligent mode?
 */
typedef enum
{
  intelligent_not_supported,
  intelligent_not_active_by_default,
  intelligent_active_by_default
} support_for_intelligent_mode;

typedef struct
{
  support_for_intelligent_mode support;
  goal_type goal;
} detector_state_type;

static
void intelligent_mode_support_detector_or(slice_index si,
                                          stip_structure_traversal *st)
{
  detector_state_type * const state = st->param;
  support_for_intelligent_mode support1;
  support_for_intelligent_mode support2;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  if (state->support!=intelligent_not_supported)
  {
    stip_traverse_structure(slices[si].u.binary.op1,st);
    support1 = state->support;

    stip_traverse_structure(slices[si].u.binary.op2,st);
    support2 = state->support;

    /* enumerators are ordered so that the weakest support has the
     * lowest enumerator etc. */
    assert(intelligent_not_supported<intelligent_not_active_by_default);
    assert(intelligent_not_active_by_default<intelligent_active_by_default);

    state->support = support1<support2 ? support1 : support2;
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static void intelligent_mode_support_none(slice_index si,
                                          stip_structure_traversal *st)
{
  detector_state_type * const state = st->param;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  state->support = intelligent_not_supported;

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static void intelligent_mode_support_goal_tester(slice_index si,
                                                 stip_structure_traversal *st)
{
  detector_state_type * const state = st->param;
  goal_type const goal = slices[si].u.goal_tester.goal.type;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  if (state->goal==no_goal)
  {
    switch (goal)
    {
      case goal_mate:
      case goal_stale:
        if (state->support!=intelligent_not_supported)
          state->support = intelligent_not_active_by_default;
        break;

      case goal_proofgame:
      case goal_atob:
        if (state->support!=intelligent_not_supported)
          state->support = intelligent_active_by_default;
        break;

      default:
        state->support = intelligent_not_supported;
        break;
    }

    state->goal = goal;
  }
  else if (state->goal!=goal)
    state->support = intelligent_not_supported;

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static structure_traversers_visitors intelligent_mode_support_detectors[] =
{
  { STAnd,               &intelligent_mode_support_none        },
  { STOr,                &intelligent_mode_support_detector_or },
  { STCheckZigzagJump,   &intelligent_mode_support_detector_or },
  { STNot,               &intelligent_mode_support_none        },
  { STConstraint,        &intelligent_mode_support_none        },
  { STReadyForDefense,   &intelligent_mode_support_none        },
  { STGoalReachedTester, &intelligent_mode_support_goal_tester },
  { STTemporaryHackFork, &stip_traverse_structure_pipe         }
};

enum
{
  nr_intelligent_mode_support_detectors
  = (sizeof intelligent_mode_support_detectors
     / sizeof intelligent_mode_support_detectors[0])
};

/* Determine whether the stipulation supports intelligent mode, and
 * how much so
 * @param si identifies slice where to start
 * @return degree of support for ingelligent mode by the stipulation
 */
static support_for_intelligent_mode stip_supports_intelligent(slice_index si)
{
  detector_state_type state = { intelligent_not_active_by_default, no_goal };
  stip_structure_traversal st;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  stip_structure_traversal_init(&st,&state);
  stip_structure_traversal_override(&st,
                                    intelligent_mode_support_detectors,
                                    nr_intelligent_mode_support_detectors);
  stip_traverse_structure(si,&st);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",state.support);
  TraceFunctionResultEnd();
  return state.support;
}

/* Initialize intelligent mode if the user or the stipulation asks for
 * it
 * @param si identifies slice where to start
 * @return false iff the user asks for intelligent mode, but the
 *         stipulation doesn't support it
 */
boolean init_intelligent_mode(slice_index si)
{
  boolean result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  goal_to_be_reached = no_goal;

  switch (stip_supports_intelligent(si))
  {
    case intelligent_not_supported:
      result = !OptFlag[intelligent];
      break;

    case intelligent_not_active_by_default:
      result = true;
      if (OptFlag[intelligent])
      {
        goal_to_be_reached = determine_goal_to_be_reached(si);
        stip_insert_intelligent_filters(si,goal_to_be_reached);
        stip_insert_goalreachable_guards(si,goal_to_be_reached);
      }
      break;

    case intelligent_active_by_default:
      result = true;
      goal_to_be_reached = determine_goal_to_be_reached(si);
      stip_insert_intelligent_filters(si,goal_to_be_reached);
      stip_insert_goalreachable_guards(si,goal_to_be_reached);
      break;

    default:
      assert(0);
      result = false;
      break;
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}
