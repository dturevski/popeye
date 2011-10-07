#include "optimisations/intelligent/mate/pin_black_piece.h"
#include "pyint.h"
#include "pydata.h"
#include "optimisations/intelligent/count_nr_of_moves.h"
#include "optimisations/intelligent/mate/finish.h"
#include "trace.h"

#include <assert.h>

static void by_officer(unsigned int nr_remaining_black_moves,
                       unsigned int nr_remaining_white_moves,
                       unsigned int max_nr_allowed_captures_by_black_pieces,
                       unsigned int max_nr_allowed_captures_by_white_pieces,
                       stip_length_type n,
                       piece pinner_orig_type,
                       piece pinner_type,
                       Flags pinner_flags,
                       square pinner_comes_from,
                       square pin_from)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",nr_remaining_black_moves);
  TraceFunctionParam("%u",nr_remaining_white_moves);
  TraceFunctionParam("%u",max_nr_allowed_captures_by_black_pieces);
  TraceFunctionParam("%u",max_nr_allowed_captures_by_white_pieces);
  TraceFunctionParam("%u",n);
  TracePiece(pinner_orig_type);
  TracePiece(pinner_type);
  TraceSquare(pinner_comes_from);
  TraceSquare(pin_from);
  TraceFunctionParamListEnd();

  {
    unsigned int const time = intelligent_count_nr_of_moves_from_to_no_check(pinner_orig_type,
                                                                             pinner_comes_from,
                                                                             pinner_type,
                                                                             pin_from);
    if (time<=nr_remaining_white_moves)
    {
      SetPiece(pinner_type,pin_from,pinner_flags);
      intelligent_mate_test_target_position(nr_remaining_black_moves,
                                            nr_remaining_white_moves-time,
                                            max_nr_allowed_captures_by_black_pieces,
                                            max_nr_allowed_captures_by_white_pieces,
                                            n);
    }
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static void by_promoted_pawn(unsigned int nr_remaining_black_moves,
                             unsigned int nr_remaining_white_moves,
                             unsigned int max_nr_allowed_captures_by_black_pieces,
                             unsigned int max_nr_allowed_captures_by_white_pieces,
                             stip_length_type n,
                             Flags pinner_flags,
                             square pinner_comes_from,
                             square pin_from,
                             boolean diagonal)
{
  piece const minor_pinner_type = diagonal ? fb : tb;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",nr_remaining_black_moves);
  TraceFunctionParam("%u",nr_remaining_white_moves);
  TraceFunctionParam("%u",max_nr_allowed_captures_by_black_pieces);
  TraceFunctionParam("%u",max_nr_allowed_captures_by_white_pieces);
  TraceFunctionParam("%u",n);
  TraceSquare(pinner_comes_from);
  TraceSquare(pin_from);
  TraceFunctionParam("%u",diagonal);
  TraceFunctionParamListEnd();

  by_officer(nr_remaining_black_moves,
             nr_remaining_white_moves,
             max_nr_allowed_captures_by_black_pieces,
             max_nr_allowed_captures_by_white_pieces,
             n,
             pb,minor_pinner_type,pinner_flags,
             pinner_comes_from,
             pin_from);
  by_officer(nr_remaining_black_moves,
             nr_remaining_white_moves,
             max_nr_allowed_captures_by_black_pieces,
             max_nr_allowed_captures_by_white_pieces,
             n,
             pb,db,pinner_flags,
             pinner_comes_from,
             pin_from);

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

static void one_pin(unsigned int nr_remaining_black_moves,
                    unsigned int nr_remaining_white_moves,
                    unsigned int max_nr_allowed_captures_by_black_pieces,
                    unsigned int max_nr_allowed_captures_by_white_pieces,
                    stip_length_type n,
                    square sq_to_be_pinned,
                    square pin_on,
                    unsigned int pinner_index,
                    boolean diagonal)
{
  piece const pinner_type = white[pinner_index].type;
  Flags const pinner_flags = white[pinner_index].flags;
  square const pinner_comes_from = white[pinner_index].diagram_square;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",nr_remaining_black_moves);
  TraceFunctionParam("%u",nr_remaining_white_moves);
  TraceFunctionParam("%u",max_nr_allowed_captures_by_black_pieces);
  TraceFunctionParam("%u",max_nr_allowed_captures_by_white_pieces);
  TraceFunctionParam("%u",n);
  TraceSquare(sq_to_be_pinned);
  TraceSquare(pin_on);
  TraceFunctionParam("%u",pinner_index);
  TraceFunctionParam("%u",diagonal);
  TraceFunctionParamListEnd();

  switch (pinner_type)
  {
    case db:
      by_officer(nr_remaining_black_moves,
                 nr_remaining_white_moves,
                 max_nr_allowed_captures_by_black_pieces,
                 max_nr_allowed_captures_by_white_pieces,
                 n,
                 db,db,pinner_flags,
                 pinner_comes_from,
                 pin_on);
      break;

    case tb:
      if (!diagonal)
        by_officer(nr_remaining_black_moves,
                   nr_remaining_white_moves,
                   max_nr_allowed_captures_by_black_pieces,
                   max_nr_allowed_captures_by_white_pieces,
                   n,
                   tb,tb,pinner_flags,
                   pinner_comes_from,
                   pin_on);
      break;

    case fb:
      if (diagonal)
        by_officer(nr_remaining_black_moves,
                   nr_remaining_white_moves,
                   max_nr_allowed_captures_by_black_pieces,
                   max_nr_allowed_captures_by_white_pieces,
                   n,
                   fb,fb,pinner_flags,
                   pinner_comes_from,
                   pin_on);
      break;

    case cb:
      break;

    case pb:
      by_promoted_pawn(nr_remaining_black_moves,
                       nr_remaining_white_moves,
                       max_nr_allowed_captures_by_black_pieces,
                       max_nr_allowed_captures_by_white_pieces,
                       n,
                       pinner_flags,
                       pinner_comes_from,
                       pin_on,
                       diagonal);
      break;

    default:
      assert(0);
      break;
  }

  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}

void intelligent_mate_pin_black_piece(unsigned int nr_remaining_black_moves,
                                      unsigned int nr_remaining_white_moves,
                                      unsigned int max_nr_allowed_captures_by_black_pieces,
                                      unsigned int max_nr_allowed_captures_by_white_pieces,
                                      stip_length_type n,
                                      square sq_to_be_pinned)
{
  int const dir = sq_to_be_pinned-king_square[Black];
  boolean const diagonal = SquareCol(king_square[Black]+dir)==SquareCol(king_square[Black]);
  square pin_on;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",nr_remaining_black_moves);
  TraceFunctionParam("%u",nr_remaining_white_moves);
  TraceFunctionParam("%u",max_nr_allowed_captures_by_black_pieces);
  TraceFunctionParam("%u",max_nr_allowed_captures_by_white_pieces);
  TraceFunctionParam("%u",n);
  TraceSquare(sq_to_be_pinned);
  TraceFunctionParamListEnd();

  for (pin_on = sq_to_be_pinned+dir; e[pin_on]==vide && nr_reasons_for_staying_empty[pin_on]==0; pin_on += dir)
  {
    if (nr_reasons_for_staying_empty[pin_on]==0)
    {
      unsigned int pinner_index;
      for (pinner_index = 1; pinner_index<MaxPiece[White]; ++pinner_index)
        if (white[pinner_index].usage==piece_is_unused)
        {
          white[pinner_index].usage = piece_pins;

          one_pin(nr_remaining_black_moves,
                  nr_remaining_white_moves,
                  max_nr_allowed_captures_by_black_pieces,
                  max_nr_allowed_captures_by_white_pieces,
                  n,
                  sq_to_be_pinned,
                  pin_on,
                  pinner_index,
                  diagonal);

          white[pinner_index].usage = piece_is_unused;
        }

      e[pin_on] = vide;
      spec[pin_on] = EmptySpec;
    }

    ++nr_reasons_for_staying_empty[pin_on];
  }

  for (pin_on -= dir; pin_on!=sq_to_be_pinned; pin_on -= dir)
    --nr_reasons_for_staying_empty[pin_on];


  TraceFunctionExit(__func__);
  TraceFunctionResultEnd();
}