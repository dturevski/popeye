#include "optimisations/intelligent/count_nr_of_moves.h"
#include "pydata.h"
#include "pyint.h"
#include "pyproof.h"
#include "optimisations/intelligent/moves_left.h"
#include "trace.h"

#include <assert.h>
#include <stdlib.h>


unsigned int intelligent_count_nr_of_moves_from_to_no_check(piece from_piece,
                                                            square from_square,
                                                            piece to_piece,
                                                            square to_square)
{
  unsigned int result;

  TraceFunctionEntry(__func__);
  TracePiece(from_piece);
  TraceSquare(from_square);
  TracePiece(to_piece);
  TraceSquare(to_square);
  TraceFunctionParamListEnd();

  if (from_square==to_square && from_piece==to_piece)
    result = 0;
  else
  {
    switch (abs(from_piece))
    {
      case Knight:
        result = ProofKnightMoves[abs(from_square-to_square)];
        break;

      case Rook:
        result = CheckDirRook[from_square-to_square]==0 ? 2 : 1;
        break;

      case Queen:
        result = ((CheckDirRook[from_square-to_square]==0
                   && CheckDirBishop[from_square-to_square]==0)
                  ? 2
                  : 1);
        break;

      case Bishop:
        if (SquareCol(from_square)==SquareCol(to_square))
          result = CheckDirBishop[from_square-to_square]==0 ? 2 : 1;
        else
          result = maxply+1;
        break;

      case King:
        result = intelligent_count_nr_of_moves_from_to_king(from_piece,
                                                            from_square,
                                                            to_square);
        break;

      case Pawn:
        if (from_piece==to_piece)
          result = intelligent_count_nr_of_moves_from_to_pawn_no_promotion(from_piece,
                                                                           from_square,
                                                                           to_square);
        else
          result = intelligent_count_nr_of_moves_from_to_pawn_promotion(from_square,
                                                                        to_piece,
                                                                        to_square);
        break;

      default:
        assert(0);
        result = UINT_MAX;
        break;
    }
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

unsigned int intelligent_count_nr_of_moves_from_to_pawn_promotion(square from_square,
                                                                  piece to_piece,
                                                                  square to_square)
{
  unsigned int result = maxply+1;
  square const start = to_piece<vide ? square_a1 : square_a8;
  piece const pawn = to_piece<vide ? pn : pb;
  square v_sq;

  TraceFunctionEntry(__func__);
  TraceSquare(from_square);
  TracePiece(to_piece);
  TraceSquare(to_square);
  TraceFunctionParamListEnd();

  for (v_sq = start; v_sq<start+nr_files_on_board; ++v_sq)
  {
    unsigned int const curmoves = (intelligent_count_nr_of_moves_from_to_no_check(pawn,
                                                                                  from_square,
                                                                                  pawn,
                                                                                  v_sq)
                                   + intelligent_count_nr_of_moves_from_to_no_check(to_piece,
                                                                                    v_sq,
                                                                                    to_piece,
                                                                                    to_square));
    if (curmoves<result)
      result = curmoves;
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

unsigned int intelligent_count_nr_of_moves_from_to_pawn_no_promotion(piece pawn,
                                                                     square from_square,
                                                                     square to_square)
{
  unsigned int result;
  int const diffcol = abs(from_square%onerow - to_square%onerow);
  int const diffrow = from_square/onerow - to_square/onerow;

  TraceFunctionEntry(__func__);
  TraceSquare(from_square);
  TraceSquare(to_square);
  TraceFunctionParamListEnd();

  if (pawn<vide)
  {
    /* black pawn */
    if (diffrow<diffcol)
      /* if diffrow<=0 then this test is true, since diffcol is always
       * non-negative
       */
      result = maxply+1;

    else if (from_square>=square_a7 && diffrow-2 >= diffcol)
      /* double step */
      result = diffrow-1;

    else
      result = diffrow;
  }
  else
  {
    /* white pawn */
    if (-diffrow<diffcol)
      result = maxply+1;

    else  if (from_square<=square_h2 && -diffrow-2 >= diffcol)
      /* double step */
      result = -diffrow-1;

    else
      result = -diffrow;
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

static unsigned int count_nr_black_moves_to_square_with_promoted_pawn(square pawn_comes_from,
                                                                      square to_be_blocked,
                                                                      unsigned int nr_remaining_black_moves)
{
  unsigned int result = maxply+1;

  TraceFunctionEntry(__func__);
  TraceSquare(pawn_comes_from);
  TraceSquare(to_be_blocked);
  TraceFunctionParam("%u",nr_remaining_black_moves);
  TraceFunctionParamListEnd();

  {
    /* A rough check whether it is worth thinking about promotions */
    unsigned int moves = (pawn_comes_from>=square_a7
                          ? 5
                          : pawn_comes_from/onerow - nr_of_slack_rows_below_board);
    assert(moves<=5);

    if (to_be_blocked>=square_a2)
      /* square is not on 8th rank -- 1 move necessary to get there */
      ++moves;

    if (nr_remaining_black_moves>=moves)
    {
      piece pp;
      for (pp = -getprompiece[vide]; pp!=vide; pp = -getprompiece[-pp])
      {
        unsigned int const time = intelligent_count_nr_of_moves_from_to_pawn_promotion(pawn_comes_from,
                                                                                       pp,
                                                                                       to_be_blocked);
        if (time<result)
          result = time;
      }
    }
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

unsigned int intelligent_count_nr_black_moves_to_square(square to_be_blocked,
                                                        unsigned int nr_remaining_black_moves)
{
  unsigned int result = maxply+1;
  unsigned int i;

  TraceFunctionEntry(__func__);
  TraceSquare(to_be_blocked);
  TraceFunctionParam("%u",nr_remaining_black_moves);
  TraceFunctionParamListEnd();

  for (i = 1; i<MaxPiece[Black]; ++i)
  {
    piece const blocker_type = black[i].type;
    square const blocker_comes_from = black[i].diagram_square;

    if (blocker_type==pn)
    {
      if (to_be_blocked>=square_a2)
      {
        unsigned int const time = intelligent_count_nr_of_moves_from_to_pawn_no_promotion(pn,
                                                                                          blocker_comes_from,
                                                                                          to_be_blocked);
        if (time<result)
          result = time;
      }

      {
        unsigned int const time_prom = count_nr_black_moves_to_square_with_promoted_pawn(blocker_comes_from,
                                                                                         to_be_blocked,
                                                                                         nr_remaining_black_moves);
        if (time_prom<result)
          result = time_prom;
      }
    }
    else
    {
      unsigned int const time = intelligent_count_nr_of_moves_from_to_no_check(blocker_type,
                                                                               blocker_comes_from,
                                                                               blocker_type,
                                                                               to_be_blocked);
      if (time<result)
        result = time;
    }
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

unsigned int intelligent_count_nr_of_moves_from_to_checking(piece from_piece,
                                                square from_square,
                                                piece to_piece,
                                                square to_square)
{
  if (from_square==to_square && from_piece==to_piece)
  {
    if (from_piece==pb)
      return maxply+1;

    else if (from_piece==cb)
      return 2;

    /* it's a rider */
    else if (move_diff_code[abs(king_square[Black]-to_square)]<3)
      return 2;
  }

  return intelligent_count_nr_of_moves_from_to_no_check(from_piece,from_square,
                                                        to_piece,to_square);
}

static unsigned int count_nr_of_moves_from_to_king_no_castling(square from, square to)
{
  unsigned int const diffcol = abs(from%onerow - to%onerow);
  unsigned int const diffrow = abs(from/onerow - to/onerow);

  return diffcol>diffrow ? diffcol : diffrow;
}

unsigned int intelligent_count_nr_of_moves_from_to_king(piece piece,
                                                        square from_square,
                                                        square to_square)
{
  unsigned int result;

  TraceFunctionEntry(__func__);
  TraceSquare(from_square);
  TraceSquare(to_square);
  TraceFunctionParamListEnd();

  result = count_nr_of_moves_from_to_king_no_castling(from_square,to_square);

  if (testcastling)
  {
    if (piece==roib)
    {
      if (from_square==square_e1)
      {
        if (TSTCASTLINGFLAGMASK(nbply,White,ra_cancastle&castling_flag[castlings_flags_no_castling]))
        {
          unsigned int const withcast = count_nr_of_moves_from_to_king_no_castling(square_c1,to_square);
          if (withcast<result)
            result = withcast;
        }
        if (TSTCASTLINGFLAGMASK(nbply,White,rh_cancastle&castling_flag[castlings_flags_no_castling]))
        {
          unsigned int const withcast = count_nr_of_moves_from_to_king_no_castling(square_g1,to_square);
          if (withcast<result)
            result = withcast;
        }
      }
    }
    else {
      if (from_square==square_e8)
      {
        if (TSTCASTLINGFLAGMASK(nbply,Black,ra_cancastle&castling_flag[castlings_flags_no_castling]))
        {
          unsigned int const withcast = count_nr_of_moves_from_to_king_no_castling(square_c8,to_square);
          if (withcast<result)
            result = withcast;
        }
        if (TSTCASTLINGFLAGMASK(nbply,Black,rh_cancastle&castling_flag[castlings_flags_no_castling]))
        {
          unsigned int const withcast = count_nr_of_moves_from_to_king_no_castling(square_g8,to_square);
          if (withcast<result)
            result = withcast;
        }
      }
    }
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

unsigned int intelligent_count_moves_to_white_promotion(square from_square)
{
  unsigned int result;

  TraceFunctionEntry(__func__);
  TraceSquare(from_square);
  TraceFunctionParamListEnd();

  if (MovesLeft[White]==5
      && from_square<=square_h2
      && (e[from_square+dir_up]>vide || e[from_square+2*dir_up]>vide))
    /* pawn can't reach the promotion square */
    result = maxply+1;
  else
  {
    unsigned int const rank = from_square/onerow - nr_of_slack_rows_below_board;
    result = 7-rank;

    if (result==6)
    {
      --result; /* double step! */

      if (MovesLeft[White]<=6)
      {
        /* immediate double step is required if this pawn is to promote */
        if (e[from_square+dir_up]==pn
            && (e[from_square+dir_left]<=roib
                && e[from_square+dir_right]<=roib))
          /* Black can't immediately get rid of block on 3th row
           * -> no immediate double step possible */
          ++result;

        else if (e[from_square+2*dir_up]==pn
                 && (e[from_square+dir_up+dir_left]<=roib
                     && e[from_square+dir_up+dir_right]<=roib
                     && ep[1]!=from_square+dir_up+dir_left
                     && ep[1]!=from_square+dir_up+dir_right))
          /* Black can't immediately get rid of block on 4th row
           * -> no immediate double step possible */
          ++result;
      }
    }
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}