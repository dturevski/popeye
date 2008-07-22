/********************* MODIFICATIONS to pyhash.c ***********************
 **
 ** Date       Who  What
 **
 ** 2003/05/12 TLi  hashing bug fixed: h= + intel did not find all solutions .
 **
 ** 2004/03/22 TLi  hashing for exact-* stipulations improved
 **
 ** 2005/02/01 TLi  function hashdefense is not used anymore...
 **
 ** 2005/02/01 TLi  in dsr_does_attacker_win and invref exchanged the inquiry into the hash
 **                 table for "white can mate" and "white cannot mate" because
 **                 it is more likely that a position has no solution
 **                 This yields an incredible speedup of .5-1%  *fg*
 **
 ** 2006/06/30 SE   New condition: BGL (invented P.Petkov)
 **
 ** 2008/02/10 SE   New condition: Cheameleon Pursuit (invented? : L.Grolman)  
 **
 **************************** End of List ******************************/

/**********************************************************************
 ** We hash.
 **
 ** SmallEncode and LargeEncode are functions to encode the current
 ** position. SmallEncode is used when the starting position contains
 ** less than or equal to eight pieces. LargeEncode is used when more
 ** pieces are present. The function TellSmallEncode and TellLargeEncode
 ** are used to decide what encoding to use. Which function to use is
 ** stored in encode.
 ** SmallEncode encodes for each piece the square where it is located.
 ** LargeEncode uses the old scheme introduced by Torsten: eight
 ** bytes at the beginning give in 64 bits the locations of the pieces
 ** coded after the eight bytes. Both functions give for each piece its
 ** type (1 byte) and specification (2 bytes). After this information
 ** about ep-captures, Duellants and Imitators are coded.
 **
 ** The hash table uses a dynamic hashing scheme which allows dynamic
 ** growth and shrinkage of the hashtable. See the relevant dht* files
 ** for more details. Two procedures are used:
 **   dhtLookupElement: This procedure delivers
 ** a nil pointer, when the given position is not in the hashtable,
 ** or a pointer to a hashelement.
 **   dhtEnterElement:  This procedure enters an encoded position
 ** with its values into the hashtable.
 **
 ** When there is no more memory, or more than MaxPositions positions
 ** are stored in the hash-table, then some positions are removed
 ** from the table. This is done in the compress procedure.
 ** This procedure uses a little improved scheme introduced by Torsten.
 ** The selection of positions to remove is based on the value of
 ** information gathered about this position. The information about
 ** a position "unsolvable in 2 moves" is less valuable than "unsolvable
 ** in 5 moves", since the former can be recomputed faster. For the other
 ** type of information ("solvable") the comparison is the other way round.
 ** The compression of the table is an expensive operation, in a lot
 ** of exeperiments it has shown to be quite effective in keeping the
 ** most valuable information, and speeds up the computation time
 ** considerably. But to be of any use, there must be enough memory to
 ** to store more than 800 positions.
 ** Now Torsten changed popeye so that all stipulations use hashing.
 ** There seems to be no real penalty in using hashing, even if the
 ** hit ratio is very small and only about 5%, it speeds up the
 ** computation time by 30%.
 ** I changed the output of hashstat, since its really informative
 ** to see the hit rate.
 **
 ** inithash()
 **   -- enters the startposition into the hash-table.
 **   -- determines which encode procedure to use
 **   -- Check's for the MaxPostion/MaxMemory settings
 **
 ** closehash()
 **   -- deletes the hashtable and gives back allocated storage.
 **
 ***********************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

/* TurboC and BorlandC  TLi */
#if defined(__TURBOC__)
# include <mem.h>
# include <alloc.h>
# include <conio.h>
#else
# include <memory.h>
#endif  /* __TURBOC__ */

#include "py.h"
#include "pyproc.h"
#include "pydata.h"
#include "pymsg.h"
#include "pyhash.h"
#include "pyint.h"
#include "DHT/dhtvalue.h"
#include "DHT/dht.h"
#include "pyproof.h"
#include "pystip.h"
#include "platform/maxtime.h"
#include "trace.h"

struct dht *pyhash;

static char    piece_nbr[PieceCount];
static boolean one_byte_hash;
static boolean flag_hashall;
static unsigned int bytes_per_spec;
static unsigned int bytes_per_piece;

#if defined(TESTHASH)
#define ifTESTHASH(x)   x
#if defined(__unix)
#include <unistd.h>
static void *OldBreak;
extern int dhtDebug;
#endif /*__unix*/
#else
#define ifTESTHASH(x)
#endif /*TESTHASH*/

#if defined(HASHRATE)
#define ifHASHRATE(x)   x
static unsigned long use_pos, use_all;
#else
#define ifHASHRATE(x)
#endif /*HASHRATE*/

/* New Version for more ply's */
enum {
  ByteMask = (1u<<CHAR_BIT)-1,
  BitsForPly = 10      /* Up to 1023 ply possible */
};

void (*encode)(HashBuffer*);

typedef struct {
	dhtValue	Key;
    struct
    {
        unsigned int dummy : BitsForPly+BitsForPly;
        unsigned int what : 3;
    } data;
} genericElement_t;

typedef struct {
	dhtValue	Key;
    struct
    {
        unsigned int notSolvableInLessThan : BitsForPly;
        unsigned int solvableIn : BitsForPly;
        unsigned int what : 3;
    } data;
} whDirElement_t;

typedef struct {
	dhtValue	Key;
    struct
    {
        unsigned int blackNotSolvableIn : BitsForPly;
        unsigned int whiteNotSolvableIn : BitsForPly;
        unsigned int what : 3;
    } data;
} helpElement_t;

typedef struct {
	dhtValue	Key;
    struct
    {
        unsigned int introNotSolvableIn : BitsForPly;
        unsigned int serNotSolvableIn : BitsForPly;
        unsigned int what : 3;
    } data;
} serElement_t;

/* how much is element *he worth to us? This information is used to
 * determine which elements to discard from the hash table if it has
 * reached its capacity. */
static int value_of_data(dhtElement const *he)
{
  genericElement_t const * const ge = (genericElement_t const *)he;
  switch (ge->data.what) {
    case SerNoSucc:
    case IntroSerNoSucc:
    {
      serElement_t const * const se = (serElement_t const *)he;
      return se->data.serNotSolvableIn + slices[0].u.composite.length*se->data.introNotSolvableIn;
    }
  
    case WhHelpNoSucc:
    case BlHelpNoSucc:
    {
      helpElement_t const * const hle = (helpElement_t const *)he;
      if (hle->data.blackNotSolvableIn > hle->data.whiteNotSolvableIn)
        return hle->data.blackNotSolvableIn;
      else
        return hle->data.whiteNotSolvableIn;
    }
  
    case WhDirSucc:
    case WhDirNoSucc:
    {
      whDirElement_t const * const wde = (whDirElement_t const *)he;
      if (wde->data.solvableIn <= slices[0].u.composite.length
          && wde->data.solvableIn+1 > wde->data.notSolvableInLessThan)
        return wde->data.solvableIn;
      else
        return wde->data.notSolvableInLessThan-1;
    }
  
    default:
      assert(0);
  }

  return 0; /* avoid compiler warning */
} /* value_of_data */

static unsigned long totalRemoveCount = 0;

static void compresshash (void) {
  dhtElement        *he;
  int       min_val,x;
  unsigned long RemoveCnt, ToDelete, runCnt;
#if defined(TESTHASH)
  unsigned long initCnt, visitCnt;
#endif
  flag_hashall= false;

  min_val= slices[0].u.composite.length;

  ifTESTHASH(printf("compressing: %ld -> ", dhtKeyCount(pyhash)));

  he= dhtGetFirstElement(pyhash);
  while (he) {
    x= value_of_data(he);
    if (x < min_val)
      min_val= x;
    he= dhtGetNextElement(pyhash);
  }
  RemoveCnt= 0;
  ToDelete= dhtKeyCount(pyhash)/16 + 1;
  if (ToDelete >= dhtKeyCount(pyhash)) {
    ToDelete= dhtKeyCount(pyhash);
    /* this is a pathological case: it may only occur, when we are so
     * low on memory, that only one or no position can be stored.
     */
  }

#if defined(TESTHASH)
  printf("\nmin_val: %d\n", min_val);
  printf("ToDelete: %ld\n", ToDelete);
  fflush(stdout);
  initCnt= dhtKeyCount(pyhash);
#endif  /* TESTHASH */

  runCnt= 0;
  while (RemoveCnt < ToDelete) {
    min_val++;

#if defined(TESTHASH)
    printf("min_val: %d\n", min_val);
    printf("RemoveCnt: %ld\n", RemoveCnt);
    fflush(stdout);
    visitCnt= 0;
#endif  /* TESTHASH */

    he= dhtGetFirstElement(pyhash);
    while (he) {
      if (value_of_data(he) <= min_val) {
        RemoveCnt++;
        totalRemoveCount++;
        dhtRemoveElement(pyhash, he->Key);
#if defined(TESTHASH)
        if (RemoveCnt + dhtKeyCount(pyhash) != initCnt) {
          fprintf(stdout,
                  "dhtRemove failed on %ld-th element of run %ld. "
                  "This was the %ld-th call to dhtRemoveElement.\n"
                  "RemoveCnt=%ld, dhtKeyCount=%ld, initCnt=%ld\n",
                  visitCnt, runCnt, totalRemoveCount,
                  RemoveCnt, dhtKeyCount(pyhash), initCnt);
          exit(1);
        }
#endif  /* TESTHASH */
      }
#if defined(TESTHASH)
      visitCnt++;
#endif  /* TESTHASH */
      he= dhtGetNextElement(pyhash);
    }
#if defined(TESTHASH)
    runCnt++;
    printf("run=%ld, RemoveCnt: %ld, missed: %ld\n", runCnt, RemoveCnt, initCnt-visitCnt);
    {
      int l, counter[16];
      int KeyCount=dhtKeyCount(pyhash);
      dhtBucketStat(pyhash, counter, 16);
      for (l=0; l< 16-1; l++) {
        fprintf(stdout, "%d %d %d\n", KeyCount, l+1, counter[l]);
      }
      fprintf(stdout, "%d %d %d\n\n", KeyCount, l+1, counter[l]);
      if (runCnt > 9) {
        fprintf(stdout, "runCnt > 9 after %ld-th call to  dhtRemoveElement\n", totalRemoveCount);
      }
      dhtDebug= runCnt == 9;
    }
    fflush(stdout);
#endif  /* TESTHASH */

  }
#if defined(TESTHASH)
  printf("%ld;", dhtKeyCount(pyhash));
  printf(" usage: %ld", use_pos);
  printf(" / %ld", use_all);
  printf(" = %ld%%", (100 * use_pos) / use_all);
#if defined(FREEMAP) && defined(FXF)
  PrintFreeMap(stdout);
#endif /*FREEMAP*/
#if defined(__TURBOC__)
  gotoxy(1, wherey());
#else
  printf("\n");
#endif /*__TURBOC__*/
#if defined(FXF)
  printf("\n after compression:\n");
  fxfInfo(stdout);
#endif /*FXF*/
#endif /*TESTHASH*/
} /* compresshash */

#if defined(HASHRATE)
/* Level = 0: No output of HashStat
 * Level = 1: Output with every trace output
 * Level = 2: Output at each table compression
 * Level = 3: Output at every 1000th hash entry
 * a call to HashStats with a value of 0 will
 * always print
 */
static unsigned int HashRateLevel = 0;

void IncHashRateLevel(void)
{
  ++HashRateLevel;
  StdString("  ");
  PrintTime();
  logIntArg(HashRateLevel);
  Message(IncrementHashRateLevel);
  HashStats(0, "\n");
}

void DecHashRateLevel(void)
{
  if (HashRateLevel>0)
    --HashRateLevel;
  StdString("  ");
  PrintTime();
  logIntArg(HashRateLevel);
  Message(DecrementHashRateLevel);
  HashStats(0, "\n");
}

#else

void IncHashRateLevel(void)
{
  /* intentionally nothing */
}

void DecHashRateLevel(void)
{
  /* intentionally nothing */
}

#endif

void HashStats(int level, char *trailer) {
#if defined(HASHRATE)
  int pos=dhtKeyCount(pyhash);
  char rate[60];

  if (level <= HashRateLevel) {
    StdString("  ");
    pos= dhtKeyCount(pyhash);
    logIntArg(pos);
    Message(HashedPositions);
    if (use_all > 0)
    {
      if (use_all < 10000)
        sprintf(rate, " %ld/%ld = %ld%%",
                use_pos, use_all, (use_pos*100) / use_all);
      else
        sprintf(rate, " %ld/%ld = %ld%%",
                use_pos, use_all, use_pos / (use_all/100));
    }
    else
      sprintf(rate, " -");
    StdString(rate);
    if (HashRateLevel > 3)
    {
      unsigned long msec;
      unsigned long Seconds;
      StopTimer(&Seconds,&msec);
      if (Seconds > 0)
      {
        sprintf(rate, ", %lu pos/s", use_all/Seconds);
        StdString(rate);
      }
    }
    if (trailer)
      StdString(trailer);
  }
#endif /*HASHRATE*/
}

static int TellCommonEncodePosLeng(int len, int nbr_p) {
  int i;

  len++; /* Castling_Flag */
  if (CondFlag[haanerchess]) {
    /*
    ** I assume an average of (nr_files_on_board*nr_rows_on_board -
    ** number of pieces)/2 additional holes per position.
    */
    /* That's far too much. In a ser-h#5 there won't be more
    ** than 5 holes in hashed positions.      TLi
    */
    int nbr_holes= (slices[0].u.composite.play==PSeries
                    ? slices[0].u.composite.length
                    : 2*slices[0].u.composite.length);
    if (nbr_holes > (nr_files_on_board*nr_rows_on_board-nbr_p)/2)
      nbr_holes= (nr_files_on_board*nr_rows_on_board-nbr_p)/2;
    len += bytes_per_piece*nbr_holes;
  }
  if (CondFlag[messigny]) {
    len+= 2;
  }
  if (CondFlag[duellist]) {
    len+= 2;
  }
  if (CondFlag[blfollow] || CondFlag[whfollow] || CondFlag[champursue]) {
    len++;
  }
  if (flag_synchron) {
    len++;
  }
  if (CondFlag[imitators]) {
    for (i = 0; i < inum[nbply]; i++) {
      len++;
    }
    /* coding of no. of imitators and average of one
       imitator-promotion assumed.
    */
    len+=2;
  }
  if (CondFlag[parrain]) {
    /*
    ** only one out of three positions with a capture
    ** assumed.
    */
    len++;
  }
  if (OptFlag[nontrivial]) {
    len++;
  }
  if (slices[0].u.composite.is_exact) {
    len++;
  }
  return len;
} /* TellCommonEncodePosLeng */

static int TellLargeEncodePosLeng(void) {
  square    *bnp;
  int       nbr_p= 0, len= 8;

  for (bnp= boardnum; *bnp; bnp++) {
    if (e[*bnp] != vide) {
      len += bytes_per_piece;
      nbr_p++;  /* count no. of pieces and holes */
    }
  }
  if (CondFlag[BGL])
    len+= sizeof BGL_white + sizeof BGL_black;

  return TellCommonEncodePosLeng(len, nbr_p);
} /* TellLargeEncodePosLeng */

static int TellSmallEncodePosLeng(void) {
  square  *bnp;
  int nbr_p= 0, len= 0;

  for (bnp= boardnum; *bnp; bnp++) {
    /* piece    p;
    ** Flags    pspec;
    */
    if (e[*bnp] != vide) {
      len += 1 + bytes_per_piece;
      nbr_p++;            /* count no. of pieces and holes */
    }
  }
  return TellCommonEncodePosLeng(len, nbr_p);
} /* TellSmallEncodePosLeng */

static byte *CommonEncode(byte *bp)
{
  if (CondFlag[messigny]) {
    if (move_generation_stack[nbcou].capture == messigny_exchange) {
      *bp++ = (byte)(move_generation_stack[nbcou].arrival - bas);
      *bp++ = (byte)(move_generation_stack[nbcou].departure - bas);
    }
    else {
      *bp++ = (byte)(0);
      *bp++ = (byte)(0);
    }
  }
  if (CondFlag[duellist]) {
    *bp++ = (byte)(whduell[nbply] - bas);
    *bp++ = (byte)(blduell[nbply] - bas);
  }

  if (CondFlag[blfollow] || CondFlag[whfollow] || CondFlag[champursue])
    *bp++ = (byte)(move_generation_stack[nbcou].departure - bas);

  if (flag_synchron)
    *bp++= (byte)(sq_num[move_generation_stack[nbcou].departure]
                  -sq_num[move_generation_stack[nbcou].arrival]
                  +64);

  if (CondFlag[imitators]) {
    int i;
    /* The number of imitators has to be coded too to
    ** avoid ambiguities.
    */
    *bp++ = (byte)inum[nbply];
    for (i = 0; i < inum[nbply]; i++) {
      *bp++ = (byte)(isquare[i] - bas);
    }
  }

  if (OptFlag[nontrivial])
    *bp++ = (byte)(max_nr_nontrivial);

  if (CondFlag[parrain]) {
    /* a piece has been captured and can be reborn */
    *bp++ = (byte)(move_generation_stack[nbcou].capture - bas);
    if (one_byte_hash) {
      *bp++ = (byte)(pprispec[nbply])
          + ((byte)(piece_nbr[abs(pprise[nbply])]) << (CHAR_BIT/2));
    }
    else {
      *bp++ = pprise[nbply];
      *bp++ = (byte)(pprispec[nbply]>>CHAR_BIT);
      *bp++ = (byte)(pprispec[nbply]&ByteMask);
    }
  }

  if (slices[0].u.composite.is_exact)
    *bp++ = (byte)(nbply);

  if (ep[nbply]!=initsquare)
    *bp++ = (byte)(ep[nbply] - bas);

  *bp++ = castling_flag[nbply];     /* Castling_Flag */

  if (CondFlag[BGL]) {
    memcpy(bp, &BGL_white, sizeof BGL_white);
    bp += sizeof BGL_white;
    memcpy(bp, &BGL_black, sizeof BGL_black);
    bp += sizeof BGL_black;
  }

  return bp;
} /* CommonEncode */

static void LargeEncode(HashBuffer *hb) {
  byte  *position= hb->cmv.Data;
  byte  *bp= position+nr_rows_on_board;
  int       row, col;
  square a_square= square_a1;

  /* clear the bits for storing the position of pieces */
  memset(position, 0, nr_rows_on_board);

  for (row=0; row<nr_rows_on_board; row++, a_square+= onerow) {
    square curr_square = a_square;
    for (col=0; col<nr_files_on_board; col++, curr_square+= dir_right) {
      piece p= e[curr_square];
      if (p!=vide) {
        Flags pspec= spec[curr_square];
        if (!TSTFLAG(pspec, Neutral))
          SETFLAG(pspec, (p < vide ? Black : White));
        p= abs(p);
        if (one_byte_hash)
          *bp++ = (byte)pspec + ((byte)piece_nbr[p] << (CHAR_BIT/2));
        else {
          unsigned int i;
          *bp++ = p;
          for (i = 0; i<bytes_per_spec; i++)
            *bp++ = (byte)((pspec>>(CHAR_BIT*i)) & ByteMask);
        }
        position[row] |= BIT(col);
      }
    }
  }

  /* Now the rest of the party */
  bp= CommonEncode(bp);

  hb->cmv.Leng= bp - hb->cmv.Data;
} /* LargeEncode */

static void SmallEncode(HashBuffer *hb)
{
  byte   *bp= hb->cmv.Data;
  int    row, col;
  square a_square= square_a1;

  for (row=0; row<nr_rows_on_board; row++, a_square+= onerow) {
    square curr_square= a_square;
    for (col=0; col<nr_files_on_board; col++, curr_square+= dir_right) {
      piece p= e[curr_square];
      if (p!=vide) {
        Flags pspec= spec[curr_square];
        if (!TSTFLAG(pspec, Neutral))
          SETFLAG(pspec, (p < vide ? Black : White));
        p= abs(p);
        *bp++= (byte)((row<<(CHAR_BIT/2))+col);
        if (one_byte_hash)
          *bp++ = (byte)pspec + ((byte)piece_nbr[p] << (CHAR_BIT/2));
        else {
          unsigned int i;
          *bp++ = p;
          for (i = 0; i<bytes_per_spec; i++)
            *bp++ = (byte)((pspec>>(CHAR_BIT*i)) & ByteMask);
        }
      }
    }
  }

  /* Now the rest of the party */
  bp= CommonEncode(bp);

  hb->cmv.Leng= bp - hb->cmv.Data;
} /* SmallEncode */

boolean inhash(hashwhat what, int val, HashBuffer *hb)
{
  dhtElement const * const he= dhtLookupElement(pyhash, (dhtValue)hb);

  ifHASHRATE(use_all++);

  if (he==dhtNilElement)
    return False;
  else
    switch (what)
    {
      case SerNoSucc:
      {
        serElement_t const * const sere = (serElement_t*)he;
        boolean const ret = slices[0].u.composite.is_exact
            ? sere->data.serNotSolvableIn == (unsigned)val
            : sere->data.serNotSolvableIn >= (unsigned)val;
        assert(sere->data.what==SerNoSucc || sere->data.what==IntroSerNoSucc);
        if (ret) {
          ifHASHRATE(use_pos++);
          return True;
        }
        else
          return False;
      }
      case IntroSerNoSucc:
      {
        serElement_t const * const sere = (serElement_t*)he;
        boolean const ret = slices[0].u.composite.is_exact
            ? sere->data.introNotSolvableIn == (unsigned)val
            : sere->data.introNotSolvableIn >= (unsigned)val;
        assert(sere->data.what==SerNoSucc || sere->data.what==IntroSerNoSucc);
        if (ret) {
          ifHASHRATE(use_pos++);
          return True;
        } else
          return False;
      }
      case WhHelpNoSucc:
      {
        helpElement_t const * const hlpe = (helpElement_t*)he;
        boolean const ret = slices[0].u.composite.is_exact
            ? hlpe->data.whiteNotSolvableIn == (unsigned)val
            : hlpe->data.whiteNotSolvableIn >= (unsigned)val;
        assert(hlpe->data.what==WhHelpNoSucc || hlpe->data.what==BlHelpNoSucc);
        if (ret) {
          ifHASHRATE(use_pos++);
          return True;
        }
        else
          return False;
      }
      case BlHelpNoSucc:
      {
        helpElement_t const * const hlpe = (helpElement_t*)he;
        boolean const ret = slices[0].u.composite.is_exact
            ? hlpe->data.blackNotSolvableIn == (unsigned)val
            : hlpe->data.blackNotSolvableIn >= (unsigned)val;
        assert(hlpe->data.what==WhHelpNoSucc || hlpe->data.what==BlHelpNoSucc);
        if (ret) {
          ifHASHRATE(use_pos++);
          return True;
        } else
          return False;
      }
      case WhDirNoSucc:
      {
        whDirElement_t const * const wde = (whDirElement_t*)he;
        boolean const ret = slices[0].u.composite.is_exact
            ? wde->data.notSolvableInLessThan == (unsigned)val+1
            : wde->data.notSolvableInLessThan >= (unsigned)val+1;
        assert(wde->data.what==WhDirNoSucc || wde->data.what==WhDirSucc);
        if (ret) {
          ifHASHRATE(use_pos++);
          return True;
        } else
          return False;
      }
      case WhDirSucc:
      {
        whDirElement_t const * const wde = (whDirElement_t*)he;
        boolean const ret = slices[0].u.composite.is_exact
            ? wde->data.solvableIn == (unsigned)val
            : wde->data.solvableIn <= (unsigned)val;
        assert(wde->data.what==WhDirNoSucc || wde->data.what==WhDirSucc);
        if (ret) {
          ifHASHRATE(use_pos++);
          return True;
        } else
          return False;
      }
      default:
        assert(0);
    }

  return False; /* avoid compiler warning */
} /* inhash */

void addtohash(hashwhat what, int val, HashBuffer *hb)
{
  dhtElement *he = dhtLookupElement(pyhash, (dhtValue)hb);

  if (he == dhtNilElement) { /* the position is new */
    he= dhtEnterElement(pyhash, (dhtValue)hb, 0);
    if (he==dhtNilElement
        || dhtKeyCount(pyhash) > MaxPositions) {
      compresshash();
      he= dhtEnterElement(pyhash, (dhtValue)hb, 0);
      if (he==dhtNilElement
          || dhtKeyCount(pyhash) > MaxPositions) {
#if defined(FXF)
        ifTESTHASH(
            printf("make new hashtable, due to trashing\n"));
        inithash();
        he= dhtEnterElement(pyhash, (dhtValue)hb, 0);
        if (he==dhtNilElement
            || dhtKeyCount(pyhash) > MaxPositions) {
          fprintf(stderr,
                  "Sorry, cannot enter more hashelements "
                  "despite compression\n");
          exit(-2);
        }
#else
        fprintf(stderr,
                "Sorry, cannot enter more hashelements "
                "despite compression\n");
        exit(-2);
#endif /*FXF*/
      }
    }
    switch (what) {
      case IntroSerNoSucc:
      {
        serElement_t * const sere = (serElement_t*)he;
        sere->data.what = what;
        sere->data.introNotSolvableIn = val;
        sere->data.serNotSolvableIn = 0;
        break;
      }
      case SerNoSucc:
      {
        serElement_t * const sere = (serElement_t*)he;
        sere->data.what = what;
        sere->data.serNotSolvableIn = val;
        sere->data.introNotSolvableIn = 0;
        break;
      }
      case WhHelpNoSucc:
      {
        helpElement_t * const hlpe = (helpElement_t*)he;
        hlpe->data.what = what;
        hlpe->data.whiteNotSolvableIn = val;
        hlpe->data.blackNotSolvableIn = 0;
        break;
      }
      case BlHelpNoSucc:
      {
        helpElement_t * const hlpe = (helpElement_t*)he;
        hlpe->data.what = what;
        hlpe->data.whiteNotSolvableIn = 0;
        hlpe->data.blackNotSolvableIn = val;
        break;
      }
      case WhDirSucc:
      {
        whDirElement_t * const wde = (whDirElement_t*)he;
        wde->data.what = what;
        wde->data.solvableIn = val;
        wde->data.notSolvableInLessThan = 0;
        break;
      }
      case WhDirNoSucc:
      {
        whDirElement_t * const wde = (whDirElement_t*)he;
        wde->data.what = what;
        wde->data.solvableIn = slices[0].u.composite.length+1;
        wde->data.notSolvableInLessThan = val+1;
        break;
      }
    }
  }
  else
  {
    switch (what) {
      case IntroSerNoSucc:
      {
        serElement_t * const sere = (serElement_t*)he;
        assert(sere->data.what==SerNoSucc || sere->data.what==IntroSerNoSucc);
        if (sere->data.introNotSolvableIn < val)
          sere->data.introNotSolvableIn = val;
        break;
      }
      case SerNoSucc:
      {
        serElement_t * const sere = (serElement_t*)he;
        assert(sere->data.what==SerNoSucc || sere->data.what==IntroSerNoSucc);
        if (sere->data.serNotSolvableIn < val)
          sere->data.serNotSolvableIn = val;
        break;
      }
      case WhHelpNoSucc:
      {
        helpElement_t * const hlpe = (helpElement_t*)he;
        assert(hlpe->data.what==WhHelpNoSucc || hlpe->data.what==BlHelpNoSucc);
        if (hlpe->data.whiteNotSolvableIn < val)
          hlpe->data.whiteNotSolvableIn = val;
        break;
      }
      case BlHelpNoSucc:
      {
        helpElement_t * const hlpe = (helpElement_t*)he;
        assert(hlpe->data.what==WhHelpNoSucc || hlpe->data.what==BlHelpNoSucc);
        if (hlpe->data.blackNotSolvableIn < val)
          hlpe->data.blackNotSolvableIn = val;
        break;
      }
      case WhDirSucc:
      {
        whDirElement_t * const wde = (whDirElement_t*)he;
        assert(wde->data.what==WhDirNoSucc || wde->data.what==WhDirSucc);
        if (wde->data.solvableIn > val)
          wde->data.solvableIn = val;
        break;
      }
      case WhDirNoSucc:
      {
        whDirElement_t * const wde = (whDirElement_t*)he;
        assert(wde->data.what==WhDirNoSucc || wde->data.what==WhDirSucc);
        if (wde->data.notSolvableInLessThan < val+1)
          wde->data.notSolvableInLessThan = val+1;
        break;
      }
    }
  }
#if defined(HASHRATE)
  if (dhtKeyCount(pyhash)%1000 == 0)
    HashStats(3, "\n");
#endif /*HASHRATE*/
} /* addtohash */

EXTERN int WhMovesLeft, BlMovesLeft;

boolean introseries(couleur introside,
                    int n,
                    boolean restartenabled,
                    slice_index si)
{
  couleur continuingside = advers(introside);
  boolean flag1 = false, flag2 = false;

  /* set play */
  if (OptFlag[solapparent]
      || (slices[si].u.composite.is_exact ? n==0 : n<introenonce))
  {
    boolean is_exact = slices[si].u.composite.is_exact;
    int i;

    SatzFlag = True;
    for (i = slices[si].u.composite.is_exact ? slices[si].u.composite.length : 1; i<=slices[si].u.composite.length; i++)
      if (ser_find_write_solutions(continuingside,i,False,si))
      {
        flag1= true;
        slices[si].u.composite.is_exact = true;
        if (OptFlag[stoponshort] && i<slices[si].u.composite.length)
        {
          FlagShortSolsReached= true;
          break;
        }
      }

    if (n==introenonce)
      Message(NewLine);

    SatzFlag= False;
    if (!is_exact)
      slices[si].u.composite.is_exact = false;
  }

  if (n>0 && !echecc(continuingside))
  {
    /* generate a single move */
    genmove(introside);
    while (encore())
    {
      if (jouecoup()
          && !echecc(introside)
          && !(restartenabled && MoveNbr < RestartNbr))
      {
        HashBuffer hb;
        (*encode)(&hb);
        if (!inhash(IntroSerNoSucc, n, &hb))
        {
          if (introseries(introside,n-1,False,si))
            flag2= true;
          else
            addtohash(IntroSerNoSucc,n,&hb);
        }
      }
      if (restartenabled)
        IncrementMoveNbr();
      repcoup();
      if (maxtime_status==MAXTIME_TIMEOUT
          || FlagShortSolsReached)
        break;
    }
    finply();
  }

  return flag1 || flag2;
}

#if !defined(DATABASE)
/* Determine and find final moves in a help stipulation
 * @param side_at_move side to perform the final move
 */
boolean h_find_write_final_move(couleur side_at_move, slice_index si)
{
  boolean final_move_found = false;

  GenMatingMove(side_at_move);

  if (side_at_move==blanc)
    WhMovesLeft--;
  else
    BlMovesLeft--;

  while (encore())
  {
    if (jouecoup()
        && (!OptFlag[intelligent] || MatePossible()))
    {
      if (goal_checkers[slices[si].u.leaf.goal](side_at_move))
      {
        final_move_found = true;
        linesolution(si);
      }
    }
    repcoup();
  }

  if (side_at_move==blanc)
    WhMovesLeft++;
  else
    BlMovesLeft++;

  finply();

  return final_move_found;
}
#endif

/* Determine whether the mating side still has a piece that could
 * deliver the mate.
 * @return true iff the mating side has such a piece
 */
static boolean is_a_mating_piece_left()
{
  boolean const is_white_mating = maincamp==blanc;

  piece p = roib+1;
  while (p<derbla && nbpiece[is_white_mating ? p : -p]==0)
    p++;

  return p<derbla;
}

/* Determine whether attacker has an end in 1 move in reflex or
 * reciprocal play.
 * This is different from d_leaf_does_attacker_win() in that
 * is_there_end_in_1() doesn't write to the hash table.
 * @param side_at_move
 * @return true iff side_at_move can end in 1 move
 */
boolean is_there_end_in_1(couleur side_at_move, slice_index si)
{
  boolean end_found = false;
  Goal const goal = slices[si].u.leaf.goal;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%d\n",si);

  if (OptFlag[keepmating] && !is_a_mating_piece_left())
    return false;

  if (goal==goal_mate)
    GenMatingMove(side_at_move);
  else
    genmove(side_at_move);

  while (encore() && !end_found)
  {
    TraceCurrentMove();
    if (jouecoup())
    {
      if (goal_checkers[goal](side_at_move))
      {
        TraceText("goal reached\n");
        end_found = true;
        coupfort();
      }
    }

    repcoup();

    if (maxtime_status==MAXTIME_TIMEOUT)
      break;
  }

  finply();

  TraceFunctionExit(__func__);
  TraceValue("%d",si);
  TraceFunctionResult("%d\n",end_found);
  return end_found;
}

/* Determine and write the final move / move pair in a reciprocal help
 * stipulation.
 * @param side_at_move side at move, which is going to end itself or
 *                     allow the other side to end
 * @return true iff final moves were found (and written)
 */
boolean reci_h_find_write_final_move(couleur side_at_move,
                                     slice_index si)
{
  couleur other_side = advers(side_at_move);
  boolean found_solution = false;
  slice_index const op1 = slices[si].u.composite.op1;
  slice_index const op2 = slices[si].u.composite.op2;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%d",si);
  TraceFunctionParam("%d",op1);
  TraceFunctionParam("%d\n",op2);

  if (!is_there_end_in_1(side_at_move,op2))
    return false;

  genmove(side_at_move);

  if (side_at_move==blanc)
    WhMovesLeft--;
  else
    BlMovesLeft--;

  while (encore())
  {
    TraceCurrentMove();
    if (jouecoup()
        && !echecc(side_at_move)
        && h_find_write_final_move(other_side,op1))
      found_solution = true;

    repcoup();

    if ((OptFlag[maxsols] && solutions>=maxsolutions)
        || maxtime_status==MAXTIME_TIMEOUT)
      break;
  }

  if (side_at_move==blanc)
    WhMovesLeft++;
  else
    BlMovesLeft++;

  finply();

  if (found_solution)
  {
    h_find_write_final_move(side_at_move,op2);
    return true;
  }
  else
    return false;
} /* reci_h_find_write_final_move */

/* Determine and write the final move pair in a helpself
 * stipulation.
 * @param side_at_move side at the move
 * @return true iff >=1 move pair was found
 */
boolean hs_find_write_final_move_pair(couleur side_at_move, slice_index si)
{
  boolean found_solution = false;
  couleur other_side = advers(side_at_move);

  genmove(side_at_move);

  while (encore())
  {
    if (jouecoup()
        && !echecc(side_at_move)
        && !sr_does_defender_win_in_0(other_side,si))
    {
      GenMatingMove(other_side);
      while (encore())
      {
        if (jouecoup()
            && goal_checkers[slices[si].u.leaf.goal](other_side))
        {
          found_solution = true;
          linesolution(si);
        }

        repcoup();
      }

      finply();
    }

    repcoup();
  }

  finply();

  return found_solution;
}

/* Determine and write the final move pair in a helpreflex
 * stipulation.
 * @param side_at_move side at the move
 * @return true iff >=1 move pair was found
 */
boolean hr_find_write_final_move_pair(couleur side_at_move,
                                      slice_index si)
{
  if (slices[si].u.leaf.end==EReflex
      && is_there_end_in_1(side_at_move,si))
    return false;
  else
  {
    boolean found_solution = false;
    couleur other_side = advers(side_at_move);

    genmove(side_at_move);

    while (encore())
    {
      if (jouecoup()
          && !echecc(side_at_move)
          && !sr_does_defender_win_in_0(other_side,si))
      {
        GenMatingMove(other_side);
        while (encore())
        {
          if (jouecoup()
              && goal_checkers[slices[si].u.leaf.goal](other_side))
          {
            found_solution = true;
            linesolution(si);
          }

          repcoup();
        }

        finply();
      }

      repcoup();
    }

    finply();

    return found_solution;
  }
}

/* Determine and write the final move pair in help countermate.
 * @param side_at_move side at the move
 * @param no_succ_hash_category hash category for storing failures
 * @param restartenabled true iff option movenum is activated
 * @return true iff >=1 move pair was found
 */
boolean
h_goal_cmate_find_write_final_move_pair(couleur side_at_move,
                                        hashwhat no_succ_hash_category,
                                        boolean restartenabled,
                                        slice_index si)
{
  boolean found_solution = false;
  couleur other_side = advers(side_at_move);

  GenMatingMove(side_at_move);

  while (encore())
  {
    if (jouecoup()
        && !echecc(side_at_move)
        && !(restartenabled && MoveNbr<RestartNbr))
    {
      HashBuffer hb;
      (*encode)(&hb);
      if (!inhash(no_succ_hash_category,1,&hb))
      {
        if (goal_checkers[goal_mate](side_at_move))
        {
          GenMatingMove(other_side);

          while (encore())
          {
            if (jouecoup()
                && goal_checkers[slices[si].u.leaf.goal](other_side))
            {
              found_solution = true;
              linesolution(si);
            }
            repcoup();
          }

          finply();
        }

        if (!found_solution)
          addtohash(no_succ_hash_category,1,&hb);
      }
    }

    if (restartenabled)
      IncrementMoveNbr();

    repcoup();
  }

  finply();

  return found_solution;
}

/* Determine and write the final move pair in help doublemate.
 * @param side_at_move side at the move
 * @param no_succ_hash_category hash category for storing failures
 * @param restartenabled true iff option movenum is activated
 * @return true iff >=1 move pair was found
 */
boolean
h_goal_dmate_find_write_final_move_pair(couleur side_at_move,
                                        hashwhat no_succ_hash_category,
                                        boolean restartenabled,
                                        slice_index si)
{
  boolean found_solution = false;
  couleur other_side = advers(side_at_move);

  genmove(side_at_move);

  while (encore())
  {
    if (jouecoup()
        && !echecc(side_at_move)
        && !(restartenabled && MoveNbr<RestartNbr))
    {
      HashBuffer hb;
      (*encode)(&hb);
      if (!inhash(no_succ_hash_category,1,&hb))
      {
        if (!immobile(other_side))
        {
          GenMatingMove(other_side);

          while (encore())
          {
            if (jouecoup()
                && goal_checkers[slices[si].u.leaf.goal](other_side))
            {
              found_solution = true;
              linesolution(si);
            }

            repcoup();
          }

          finply();
        }

        if (!found_solution)
          addtohash(no_succ_hash_category,1,&hb);
      }
    }

    if (restartenabled)
      IncrementMoveNbr();

    repcoup();

    if ((OptFlag[maxsols] && solutions>=maxsolutions)
        || maxtime_status==MAXTIME_TIMEOUT)
      break;
  }

  finply();

  return found_solution;
}

/* Determine and write the final move pair in help stipulation with
 * "regular" goal. 
 * @param side_at_move side at the move
 * @param no_succ_hash_category hash category for storing failures
 * @param restartenabled true iff option movenum is activated
 * @param slice_index identifies current slice
 * @return true iff >=1 move pair was found
 */
boolean h_regular_find_write_final_move_pair(couleur side_at_move,
                                             hashwhat no_succ_hash_category,
                                             boolean restartenabled,
                                             slice_index si)
{
  boolean found_solution = false;
  couleur other_side = advers(side_at_move);

  genmove(side_at_move);

  if (side_at_move==noir)
    BlMovesLeft--;
  else
    WhMovesLeft--;

  while (encore())
  {
    if (jouecoup()
        && (!OptFlag[intelligent] || MatePossible())
        && !echecc(side_at_move)
        && !(restartenabled && MoveNbr<RestartNbr))
    {
      HashBuffer hb;
      (*encode)(&hb);
      if (!inhash(no_succ_hash_category,1,&hb))
      {
        if (h_find_write_final_move(other_side,si))
          found_solution = true;
        else
          addtohash(no_succ_hash_category,1,&hb);
      }
    }

    if (restartenabled)
      IncrementMoveNbr();

    repcoup();

    if ((OptFlag[maxsols] && solutions>=maxsolutions)
        || maxtime_status==MAXTIME_TIMEOUT)
      break;
  }
    
  if (side_at_move==noir)
    BlMovesLeft++;
  else
    WhMovesLeft++;

  finply();

  return found_solution;
}

/* Determine and write the solution of a help leaf slice in help play.
 * @param side_at_move side at the move
 * @param no_succ_hash_category hash category for storing failures
 * @param restartenabled true iff option movenum is activated
 * @param slice_index identifies current slice
 * @return true iff >=1 move pair was found
 */
boolean leaf_h_find_write_final_move_pair(couleur side_at_move,
                                          hashwhat no_succ_hash_category,
                                          boolean restartenabled,
                                          slice_index si)
{
  switch (slices[si].u.leaf.goal)
  {
    case goal_countermate:
      return h_goal_cmate_find_write_final_move_pair(side_at_move,
                                                     no_succ_hash_category,
                                                     restartenabled,
                                                     si);
    case goal_doublemate:
      return h_goal_dmate_find_write_final_move_pair(side_at_move,
                                                     no_succ_hash_category,
                                                     restartenabled,
                                                     si);

    default:
      return h_regular_find_write_final_move_pair(side_at_move,
                                                  no_succ_hash_category,
                                                  restartenabled,
                                                  si);
  }
}

/* Determine and write the solution of a leaf slice in help play.
 * @param side_at_move side at the move
 * @param no_succ_hash_category hash category for storing failures
 * @param restartenabled true iff option movenum is activated
 * @param slice_index identifies current slice
 * @return true iff >=1 move pair was found
 */
boolean h_leaf_find_write_final_move_pair(couleur side_at_move,
                                          hashwhat no_succ_hash_category,
                                          boolean restartenabled,
                                          slice_index si)
{
  switch (slices[si].u.leaf.end)
  {
    case ESelf:
      return hs_find_write_final_move_pair(side_at_move,si);

    case EReflex:
    case ESemireflex:
      return hr_find_write_final_move_pair(side_at_move,si);

    case EHelp:
      return leaf_h_find_write_final_move_pair(side_at_move,
                                               no_succ_hash_category,
                                               restartenabled,
                                               si);
    default:
      assert(0);
      return false;
  }
}

/* Determine and write the final move pair in a help stipulation.
 * @param side_at_move side at the move
 * @param no_succ_hash_category hash category for storing failures
 * @param restartenabled true iff option movenum is activated
 * @return true iff >=1 move pair was found
 */
boolean h_find_write_solutions_in_2(couleur side_at_move,
                                    hashwhat no_succ_hash_category,
                                    boolean restartenabled,
                                    slice_index si)
{
  switch (slices[si].type)
  {
    case STReciprocal:
      return reci_h_find_write_final_move(side_at_move,si);

    case STSequence:
    {
      slice_index const op1 = slices[si].u.composite.op1;
      switch (slices[op1].type)
      {
        case STLeaf:
          return h_leaf_find_write_final_move_pair(side_at_move,
                                                   no_succ_hash_category,
                                                   restartenabled,
                                                   op1);

        default:
          assert(0);
          return false;
      }
    }

    default:
      assert(0);
      return false;
  }
}

/* Determine and write the solution(s) in a help stipulation.
 *
 * This is a recursive function.
 * Recursion works with decreasing parameter n; recursion stops at
 * n==2 (e.g. h#1). For solving help play problems in 0.5, call
 * h_find_write_final_move() instead.
 *
 * @param side_at_move side at the move
 * @param n number of half moves until end state has to be reached
 * @param restartenabled true iff option movenum is activated
 */
boolean h_find_write_solutions(couleur side_at_move,
                               int n,
                               boolean restartenabled,
                               slice_index si)
{
  boolean found_solution = false;
  hashwhat next_no_succ = side_at_move==blanc ? BlHelpNoSucc : WhHelpNoSucc;

  assert(n>=2);

  if (OptFlag[keepmating] && !is_a_mating_piece_left())
    return false;

  if (n==2)
    found_solution = h_find_write_solutions_in_2(side_at_move,
                                                 next_no_succ,
                                                 restartenabled,
                                                 si);
  else
  {
    couleur next_side = advers(side_at_move);

    genmove(side_at_move);

    if (side_at_move==noir)
      BlMovesLeft--;
    else
      WhMovesLeft--;

    while (encore())
    {
      if (jouecoup()
          && (!OptFlag[intelligent] || MatePossible())
          && !echecc(side_at_move)
          && !(restartenabled && MoveNbr<RestartNbr))
      {
        if (flag_hashall)
        {
          HashBuffer hb;
          (*encode)(&hb);
          if (!inhash(next_no_succ,n-1,&hb))
          {
            if (h_find_write_solutions(next_side,n-1,False,si))
              found_solution = true;
            else
              addtohash(next_no_succ,n-1,&hb);
          }
        } else
          if (h_find_write_solutions(next_side,n-1,False,si))
            found_solution = true;
      }

      if (restartenabled)
        IncrementMoveNbr();

      repcoup();

      /* Stop solving if a given number of solutions was encountered */
      if ((OptFlag[maxsols] && solutions>=maxsolutions)
          || maxtime_status==MAXTIME_TIMEOUT)
        break;
    }
    
    if (side_at_move==noir)
      BlMovesLeft++;
    else
      WhMovesLeft++;

    finply();
  }

  return found_solution;
} /* h_find_write_solutions */

/* Determine and write final move of the attacker in a series
 * direct stipulation.
 * This is different from non-series play because series solutions are
 * written one 1 line each, while non-series solutions are written in
 * tree form.
 * @param attacker attacking side
 * @return true iff >= 1 final move (sequence) was found
 */
boolean ser_d_find_write_final_move(couleur attacker, slice_index si)
{
  boolean solution_found = false;

  GenMatingMove(attacker);

  while (encore())
  {
    if (jouecoup()
        && goal_checkers[slices[si].u.leaf.goal](attacker))
    {
      linesolution(si);
      solution_found = true;
    }

    repcoup();
  }

  finply();

  return solution_found;
} /* ser_d_find_write_final_move */

/* Determine and write final move of the attacker in a series
 * self/reflex stipulation, plus the (subsequent) final move of the
 * defender.
 * This is different from non-series play because series solutions are
 * written one 1 line each, while non-series solutions are written in
 * tree form.
 * @param attacker attacking side
 * @return true iff >= 1 final move (sequence) was found
 */
boolean ser_sr_find_write_final_attacker_move(couleur attacker,
                                              slice_index si)
{
  couleur defender = advers(attacker);
  boolean solution_found = false;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%d\n",si);

  genmove(attacker);

  while (encore())
  {
    TraceCurrentMove();
    if (jouecoup()
        && !echecc(attacker)
        && !sr_does_defender_win_in_0(defender,si))
    {
      TraceText("solution found\n");
      solution_found = true;
      sr_find_write_final_move(defender,si);
    }

    repcoup();
  }

  finply();

  return solution_found;
} /* ser_sr_find_write_final_attacker_move */

/* Determine and write solutions in a series stipulation.
 * @param series_side side doing the series
 * @param n number of moves to reach the end state
 * @param restartenabled true iff option movenum is active
 */
boolean ser_find_write_solutions(couleur series_side,
                                 int n,
                                 boolean restartenabled,
                                 slice_index si)
{
  slice_index const op1 = slices[si].u.composite.op1;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%d",n);
  TraceFunctionParam("%d\n",si);

  if (n==1)
  {
    TraceValue("%d\n",slices[si].type);
    switch (slices[si].type)
    {
      case STSequence:
        switch (slices[op1].type)
        {
          case STLeaf:
            switch (slices[op1].u.leaf.end)
            {
              case EHelp:
                return leaf_h_find_write_final_move_pair(series_side,
                                                         SerNoSucc,
                                                         restartenabled,
                                                         op1);
              case EDirect:
                return ser_d_find_write_final_move(series_side,op1);

              case EReflex:
                if (is_there_end_in_1(series_side,op1))
                  return false;
                else
                  return ser_sr_find_write_final_attacker_move(series_side,
                                                               op1);

              case ESelf:
              case ESemireflex:
                return ser_sr_find_write_final_attacker_move(series_side,
                                                             op1);

              default:
                assert(0);
                return false;
            }

          default:
            assert(0);
            return false;
        }

      case STReciprocal:
        return reci_h_find_write_final_move(series_side,si);

      default:
        assert(0);
        return false;
    }
  }
  else
  {
    boolean solution_found = false;
    couleur other_side = advers(series_side);

    if (slices[op1].type==STLeaf
        && slices[op1].u.leaf.end==EReflex
        && is_there_end_in_1(series_side,op1))
      return false;

    genmove(series_side);

    if (series_side==blanc)
      WhMovesLeft--;
    else
      BlMovesLeft--;

    while (encore())
    {
      TraceValue("%d\n",n);
      TraceCurrentMove();
      if (jouecoup()
          && !echecc(series_side)
          && !(restartenabled && MoveNbr<RestartNbr)
          && (!OptFlag[intelligent] || MatePossible())
          && !echecc(other_side))
      {
        HashBuffer hb;
        (*encode)(&hb);
        if (!inhash(SerNoSucc,n,&hb))
        {
          if (ser_find_write_solutions(series_side,n-1,False,si))
            solution_found = true;
          else
            addtohash(SerNoSucc,n,&hb);
        }
      }

      if (restartenabled)
        IncrementMoveNbr();

      repcoup();

      if ((OptFlag[maxsols] && solutions>=maxsolutions)
          || maxtime_status==MAXTIME_TIMEOUT)
        break;
    }

    if (series_side==blanc)
      WhMovesLeft++;
    else
      BlMovesLeft++;

    finply();

    return solution_found;
  }
} /* ser_find_write_solutions */

void inithash(void)
{
  int Small, Large;
  int i, j;

  ifTESTHASH(
      sprintf(GlobalStr, "calling inithash\n");
      StdString(GlobalStr)
      );

#if defined(__unix) && defined(TESTHASH)
  OldBreak= sbrk(0);
#endif /*__unix,TESTHASH*/

#if defined(FXF)
  if (fxfInit(MaxMemory) == -1) /* we didn't get hashmemory ... */
    FtlMsg(NoMemory);
  ifTESTHASH(fxfInfo(stdout));
#endif /*FXF*/

  flag_hashall= true;

  PositionCnt= 0;
  dhtRegisterValue(dhtBCMemValue, 0, &dhtBCMemoryProcs);
  dhtRegisterValue(dhtSimpleValue, 0, &dhtSimpleProcs);
  pyhash= dhtCreate(dhtBCMemValue, dhtCopy, dhtSimpleValue, dhtNoCopy);

  ifHASHRATE(use_pos = use_all = 0);

  /* check whether a piece can be coded in a single byte */
  j = 0;
  for (i = PieceCount; Empty < i; i--) {
    if (exist[i])
      piece_nbr[i] = j++;
  }
  if (CondFlag[haanerchess])
    piece_nbr[obs]= j++;

  one_byte_hash = j<(1<<(CHAR_BIT/2)) && PieSpExFlags<(1<<(CHAR_BIT/2));

  bytes_per_spec= 1;
  if ((PieSpExFlags >> CHAR_BIT) != 0)
    bytes_per_spec++;
  if ((PieSpExFlags >> 2*CHAR_BIT) != 0)
    bytes_per_spec++;

  bytes_per_piece= one_byte_hash ? 1 : 1+bytes_per_spec;

  if (OptFlag[intelligent]) {
    one_byte_hash = false;
    bytes_per_spec= 5; /* TODO why so high??? */
  }

  if (slices[1].u.leaf.goal==goal_proof
      || slices[1].u.leaf.goal==goal_atob)
  {
    encode = ProofEncode;
    if (MaxMemory>0 && MaxPositions==0)
      MaxPositions= MaxMemory/(24+sizeof(char *)+1);
  }
  else
  {
    Small= TellSmallEncodePosLeng();
    Large= TellLargeEncodePosLeng();
    if (Small <= Large) {
      encode= SmallEncode;
      if (MaxMemory>0 && MaxPositions==0)
        MaxPositions= MaxMemory/(Small+sizeof(char *)+1);
    }
    else
    {
      encode= LargeEncode;
      if (MaxMemory>0 && MaxPositions==0)
        MaxPositions= MaxMemory/(Large+sizeof(char *)+1);
    }
  }

#if defined(FXF)
  ifTESTHASH(printf("MaxPositions: %7lu\n", MaxPositions));
  ifTESTHASH(printf("MaxMemory:    %7lu KB\n", MaxMemory/1024));
#else
  ifTESTHASH(
      printf("room for up to %lu positions in hash table\n", MaxPositions));
#endif /*FXF*/
} /* inithash */

void    closehash(void)
{
#if defined(TESTHASH)
  sprintf(GlobalStr, "calling closehash\n");
  StdString(GlobalStr);

  sprintf(GlobalStr, "%ld enquiries out of %ld successful. ",
          use_pos, use_all);
  StdString(GlobalStr);
  if (use_all) {
    sprintf(GlobalStr, "Makes %ld%%\n", (100 * use_pos) / use_all);
    StdString(GlobalStr);
  }
#if defined(__unix)
  {
    unsigned long HashCount, HashMem, BytePerPos;
#if defined(FXF)
    HashMem= fxfTotal();
#else
    HashMem= sbrk(0)-OldBreak;
#endif /*FXF*/
    if ((HashCount=dhtKeyCount(pyhash))>0) {
      BytePerPos= (HashMem*100)/HashCount;
      sprintf(GlobalStr,
              "Memory for hash-table: %ld, "
              "gives %ld.%02ld bytes per position\n",
              HashMem, BytePerPos/100, BytePerPos%100);
    }
    else {
      sprintf(GlobalStr, "Nothing in hashtable\n");
    }
    StdString(GlobalStr);
#endif /*__unix*/
  }
#endif /*TESTHASH*/

  dhtDestroy(pyhash);
#if defined(TESTHASH) && defined(FXF)
  fxfInfo(stdout);
#endif /*TESTHASH,FXF*/

} /* closehash */

/* Determine whether attacker can end in 1 move in direct play.
 * @param attacker attacking side (i.e. side attempting to reach the
 * end)
 * @return true iff attacker can end in 1 move
 */
boolean d_leaf_does_attacker_win(couleur attacker, slice_index si)
{
  boolean end_found = false;
  HashBuffer hb;

  /* In move orientated stipulations (%, z, x etc.) it's less
   * expensive to compute an end in 1. TLi
   */
  if (!FlagMoveOrientatedStip)
  {
    /* It is more likely that a position has no solution. 
     * Therefore let's check for "no solution" first.  TLi
     */
    (*encode)(&hb);
    if (inhash(WhDirNoSucc,1,&hb))
      return false;
    if (inhash(WhDirSucc,1,&hb))
      return true;
  }

  if (OptFlag[keepmating] && !is_a_mating_piece_left())
    return false;

  GenMatingMove(attacker);

  while (encore() && !end_found)
  {
    if (jouecoup())
    {
      end_found = goal_checkers[slices[si].u.leaf.goal](attacker);
      if (end_found)
        coupfort();
    }

    repcoup();

    if (maxtime_status==MAXTIME_TIMEOUT)
      break;
  }

  finply();

  if (!FlagMoveOrientatedStip)
    addtohash(end_found ? WhDirSucc : WhDirNoSucc, 1, &hb);

  return end_found;
}

/* Determine whether defender can avoid being defeated in 1 move of
 * direct play; defender is at move
 * @param defender defending side (i.e. side to be defeated)
 * @return true iff the end state can be forced and defender is not
 *              immobile currently
 * TODO determine usefulness of this immobility check
 */
boolean d_does_defender_win_in_1(couleur defender, slice_index si)
{
  boolean refutation_found = false;
  boolean is_defender_immobile = true;
  couleur attacker = advers(defender);
  int ntcount = 0;

  if (max_len_threat==0  && !echecc(defender))
    return true;

  /* Check whether defender has more non trivial moves than he is
  ** allowed to have. The number of such moves allowed
  ** (max_nr_nontrivial) is entered using the nontrivial option. */
  if (min_length_nontrivial==0)
  {
    ntcount = count_non_trivial(defender,si);
    if (max_nr_nontrivial<ntcount)
      return true;
    else
      max_nr_nontrivial -= ntcount;
  } /* nontrivial */

  genmove(defender);

  while (!refutation_found && encore())
  {
    if (jouecoup() && !echecc(defender))
    {
      is_defender_immobile = false;
      if (!d_leaf_does_attacker_win(attacker,si))
      {
        refutation_found = true;
        coupfort();
      }
    }

    repcoup();
  }

  finply();

  if (min_length_nontrivial==0)
    max_nr_nontrivial += ntcount;

  return is_defender_immobile || refutation_found;
}

/* Determine whether the attacker wins in a self/reflex stipulation in 1.
 * @param attacker attacking side (at move)
 * @param si slice index of leaf slice
 * @param parent_is_exact true iff parent of slice si has exact length
 * @return true iff attacker wins
 */
boolean sr_leaf_does_attacker_win(couleur attacker,
                                  slice_index si,
                                  boolean parent_is_exact,
                                  boolean should_hash)
{
  boolean win_found = false;
  couleur defender = advers(attacker);
  HashBuffer hb;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%d\n",si);

  if (should_hash)
  {
    /* It is more likely that a position has no solution. */
    /*    Therefore let's check for "no solution" first. TLi */
    (*encode)(&hb);
    if (inhash(WhDirNoSucc,1,&hb))
    {
      assert(!inhash(WhDirSucc,1,&hb));
      return false;
    }
    if (inhash(WhDirSucc,1,&hb))
      return true;
  }

  if (!parent_is_exact)
    if (goal_checkers[slices[si].u.leaf.goal](defender))
    {
      addtohash(WhDirSucc,1,&hb);
      assert(!inhash(WhDirNoSucc,1,&hb));
      return true;
    }

  if (slices[si].u.leaf.end==EReflex
      && is_there_end_in_1(attacker,si))
    return false;

  genmove(attacker);

  while (!win_found && encore())
  {
    TraceCurrentMove();
    if (jouecoup()
        && !echecc(attacker))
    {
      if (!sr_does_defender_win_in_0(defender,si))
      {
        TraceText("wins\n");
        win_found = true;
        coupfort();
      }
    }

    repcoup();

    if (maxtime_status==MAXTIME_TIMEOUT)
      break;
  }

  finply();

  if (should_hash)
    addtohash(win_found ? WhDirSucc : WhDirNoSucc, 1, &hb);

  return win_found;
}

/* Determine whether the attacker wins in a direct/self/reflex
 * stipulation in 1. 
 * @param attacker attacking side (at move)
 * @param si slice index of leaf slice
 * @param parent_is_exact true iff parent of slice si has exact length
 * @return true iff attacker wins
 */
boolean dsr_leaf_does_attacker_win(couleur attacker,
                                   slice_index si,
                                   boolean parent_is_exact,
                                   boolean should_hash)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%d\n",si);
  switch (slices[si].u.leaf.end)
  {
    case EDirect:
      return d_leaf_does_attacker_win(attacker,si);

    case ESelf:
    case ESemireflex:
    case EReflex:
      return sr_leaf_does_attacker_win(attacker,
                                       si,
                                       parent_is_exact,
                                       should_hash);

    default:
      assert(0);
      return false;
  }
}

/* Determine whether the attacker wins in a quodlibet
 * @param attacker attacking side (at move)
 * @param si slice index of leaf slice
 * @param parent_is_exact true iff parent of slice si has exact length
 * @return true iff attacker wins
 */
boolean dsr_quodlibet_does_attacker_win_in_1(couleur attacker,
                                             slice_index si)
{
  boolean const is_exact = slices[si].u.composite.is_exact;
  slice_index const op1 = slices[si].u.composite.op1;
  slice_index const op2 = slices[si].u.composite.op2;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%d\n",si);

  switch (slices[op1].type)
  {
    case STLeaf:
    {
      boolean const should_hash = true;
      if (dsr_leaf_does_attacker_win(attacker,op1,is_exact,should_hash))
        return true;
      else
        break;
    }

    default:
      assert(0);
      return false;
  }

  switch (slices[op2].type)
  {
    case STLeaf:
    {
      /* avoid conflict in hash table between op1 and op2 */
      boolean const should_hash = false;
      if (dsr_leaf_does_attacker_win(attacker,op2,is_exact,should_hash))
        return true;
      else
        break;
    }

    default:
      assert(0);
      return false;
  }

  return false;
}

/* Determine whether the attacking side wins after we have played a
 * direct sequence.
 * @param attacker attacking side
 * @param si slice identifier
 */
boolean dsr_sequence_does_attacker_win(couleur attacker, slice_index si)
{
  boolean result = false;
  slice_index const op1 = slices[si].u.composite.op1;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%d\n",si);
  switch (slices[op1].type)
  {
    case STLeaf:
    {
      boolean const should_hash = true;
      result = dsr_leaf_does_attacker_win(attacker,
                                          op1,
                                          slices[si].u.composite.is_exact,
                                          should_hash);
    }

    default:
      assert(0);
  }

  TraceFunctionExit(__func__);
  TraceValue("%d",si);
  TraceFunctionResult("%d\n",result);
  return result;
}


typedef enum
{
  short_win,
  short_loss,
  no_short_result
} short_result_type;

/* Determine whether there is a short win/loss for the attacking side in
 * direct play.
 * @param attacker attacking side
 * @param si slice identifier
 * @return whether there is a short win or loss
 */
/* TODO make one function for win and one for loss */
short_result_type dsr_does_attacker_win_short(couleur attacker,
                                              slice_index si)
{
  couleur const defender = advers(attacker);
  slice_index const op1 = slices[si].u.composite.op1;
  switch (slices[op1].type)
  {
    case STLeaf:
      switch (slices[op1].u.leaf.end)
      {
        case EDirect:
          if (OptFlag[keepmating] && !is_a_mating_piece_left())
            return short_loss;
          break;

        case ESelf:
        case ESemireflex:
          if (!slices[si].u.composite.is_exact)
          {
            if (goal_checkers[slices[op1].u.leaf.goal](defender))
              return short_win;
          }
          break;

        case EReflex:
          if (!slices[si].u.composite.is_exact)
          {
            if (goal_checkers[slices[op1].u.leaf.goal](defender))
              return short_win;
          }

          if (is_there_end_in_1(attacker,op1))
            return short_loss;

          break;

        default:
          assert(0);
          return no_short_result;
      }

    default:
      assert(0);
      return no_short_result;
  }

  return no_short_result;
}

/* Determine whether the attacking side wins in 1 in direct play.
 * @param attacker attacking side
 * @param si slice identifier
 */
boolean dsr_does_attacker_win_in_1(couleur attacker, slice_index si)
{
  boolean result = false;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%d",si);
  TraceFunctionParam("%d\n",slices[si].type);
  switch (slices[si].type)
  {
    case STQuodlibet:
      result = dsr_quodlibet_does_attacker_win_in_1(attacker,si);
      break;

    case STSequence:
      result = dsr_sequence_does_attacker_win(attacker,si);
      break;

    default:
      assert(0);
  }

  TraceFunctionExit(__func__);
  TraceValue("si:%d",si);
  TraceFunctionResult("%d\n",result);
  return result;
}

/* Determine whether the attacking side wins in n (>1) in direct play.
 * @param attacker attacking side
 * @param si slice identifier
 */
boolean dsr_does_attacker_win_in_n(couleur attacker, int n, slice_index si)
{
  couleur const defender = advers(attacker);
  boolean win_found = false;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%d",n);
  TraceFunctionParam("%d\n",si);

  genmove(attacker);

  while (!win_found && encore())
  {
    TraceValue("%d ",n);
    TraceCurrentMove();
    if (jouecoup()
        && !echecc(attacker)
        && !dsr_does_defender_win(defender,n-1,si))
    {
      TraceValue("%d",n);
      TraceText("wins\n");
      win_found = true;
      coupfort();
    }

    repcoup();

    if (maxtime_status==MAXTIME_TIMEOUT)
      break;
  }

  finply();

  TraceFunctionExit(__func__);
  TraceValue("n:%d",n);
  TraceFunctionResult("%d\n",win_found);
  return win_found;
}

/* Determine whether attacker can end in n moves of direct play.
 * This is a recursive function.
 * @param attacker attacking side (i.e. side attempting to reach the
 * end)
 * @param n number of moves left until the end state has to be reached
 * @return true iff attacker can end in n moves
 */
boolean dsr_does_attacker_win(couleur attacker, int n, slice_index si)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%d",n);
  TraceFunctionParam("%d\n",si);
  if (n==1)
    return dsr_does_attacker_win_in_1(attacker,si);
  else
  {
    int i;
    boolean end_found = false;
    HashBuffer hb;

    /* It is more likely that a position has no solution.           */
    /* Therefore let's check for "no solution" first.  TLi */
    (*encode)(&hb);
    if (inhash(WhDirNoSucc,n,&hb))
    {
      assert(!inhash(WhDirSucc,n,&hb));
      return false;
    }
    if (inhash(WhDirSucc,n,&hb))
      return true;

    switch (dsr_does_attacker_win_short(attacker,si))
    {
      case short_win:
        addtohash(WhDirSucc,n,&hb);
        assert(!inhash(WhDirNoSucc,n,&hb));
        return true;

      case short_loss:
        return false;

      default:
        ; /* nothing */
    }

    for (i = slices[si].u.composite.is_exact ? n : 1;
         !end_found && i<=n;
         i++)
    {
      if (i==1)
      {
        if (dsr_does_attacker_win_in_1(attacker,si))
          return true;
      }
      else
      {
        if (i-1>max_len_threat || i>min_length_nontrivial)
          i = n;

        end_found = dsr_does_attacker_win_in_n(attacker,i,si);
      }

      if (maxtime_status==MAXTIME_TIMEOUT)
        break;
    }

    addtohash(end_found ? WhDirSucc : WhDirNoSucc, n, &hb);

    return end_found;
  }
} /* dsr_does_attacker_win */

/* Generate (piece by piece) candidate moves for the last move of a s#
 * or r#. Do *not* generate moves for the piece on square
 * initiallygenerated; this piece has already been taken care of.
 * @param camp 
 */
/* TODO Find out whether selflastencore() is really more efficient
 * than the usual genmove() */
static boolean selflastencore(couleur camp,
                              square const **selfbnp,
                              square initiallygenerated)
{
  if (encore())
    return true;
  else
  {
    square curr_square = **selfbnp;

    if (TSTFLAG(PieSpExFlags,Neutral))
      initneutre(advers(camp));

    while (curr_square!=initsquare)
    {
      if (curr_square!=initiallygenerated)
      {
        piece p= e[curr_square];
        if (p!=vide)
        {
          if (TSTFLAG(spec[curr_square],Neutral))
            p = -p;
          if (camp==blanc)
          {
            if (p>obs)
              gen_wh_piece(curr_square,p);
          }
          else
          {
            if (p<vide)
              gen_bl_piece(curr_square,p);
          }
        }
      }
    
      ++*selfbnp;
      curr_square = **selfbnp;

      if (encore())
        return true;
    }

    return false;
  }
} /* selflastencore */

/* Determine whether the defender is not forced to end in 1 in a
 * self/reflex.
 * @param defender defending side (at move)
 * @return true iff defender wins
 */
boolean sr_does_defender_win_in_0(couleur defender, slice_index si)
{
  boolean is_defender_immobile = true;
  boolean win_found = false;
  couleur attacker = advers(defender);
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%d",si);
  TraceFunctionParam("%d\n",slices[si].u.leaf.goal);

  if (slices[si].u.leaf.end==EReflex
      || slices[si].u.leaf.end==ESemireflex)
    return !is_there_end_in_1(defender,si);

  if (OptFlag[keepmating] && !is_a_mating_piece_left())
    return true;

  if (defender==noir ? flagblackmummer : flagwhitemummer)
  {
    move_generation_mode = move_generation_optimized_by_killer_move;
    genmove(defender);
    move_generation_mode = move_generation_optimized_by_killer_move;

    while (!win_found && encore())
    {
      if (jouecoup() && !echecc(defender))
      {
        is_defender_immobile = false;
        win_found = !goal_checkers[slices[si].u.leaf.goal](defender);
        if (win_found)
          coupfort();
      }
      repcoup();
    }
    finply();
  }
  else if (slices[si].u.leaf.goal==goal_ep
           && ep[nbply]==initsquare
           && ep2[nbply]==initsquare)
  {
    /* a little optimization if end "state" is en passant capture,
     * but no en passant capture is possible */
    /* TODO Should we play the same trick for castling? Other end
     * states? */
  }
  else
  {
    piece p;
    square const *selfbnp = boardnum;
    square initiallygenerated = initsquare;

    nextply();
    init_move_generation_optimizer();
    trait[nbply]= defender;
    if (TSTFLAG(PieSpExFlags,Neutral))
      initneutre(attacker);

    p = e[current_killer_state.move.departure];
    if (p!=vide)
    {
      if (TSTFLAG(spec[current_killer_state.move.departure], Neutral))
        p = -p;
      if (defender==blanc)
      {
        if (p>obs)
        {
          initiallygenerated = current_killer_state.move.departure;
          gen_wh_piece(initiallygenerated,p);
        }
      }
      else
      {
        if (p<-obs)
        {
          initiallygenerated = current_killer_state.move.departure;
          gen_bl_piece(initiallygenerated,p);
        }
      }
    }
    finish_move_generation_optimizer();
    while (!win_found
           && selflastencore(defender,&selfbnp,initiallygenerated))
    {
      TraceCurrentMove();
      if (jouecoup() && !echecc(defender))
      {
        is_defender_immobile = false;
        if (!goal_checkers[slices[si].u.leaf.goal](defender))
        {
          TraceText("win_found\n");
          win_found = true;
          coupfort();
        }
      }
      repcoup();
    }
    finply();
  }

  TraceFunctionExit(__func__);
  TraceValue("%d",win_found);
  TraceValue("%d",is_defender_immobile);
  TraceFunctionResult("%d\n", win_found || is_defender_immobile);
  return win_found || is_defender_immobile;
}


/* Determine whether there is a short win/loss for the defending side in
 * direct play.
 * @param defender defending side
 * @param si slice identifier
 * @return whether there is a short win or loss
 */
/* TODO make one function for win and one for loss? */
short_result_type dsr_does_defender_win_short(couleur defender,
                                              slice_index si)
{
  short_result_type result = no_short_result;
  slice_index const op1 = slices[si].u.composite.op1;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%d",si);
  TraceFunctionParam("%d\n",slices[op1].type);
  switch (slices[op1].type)
  {
    case STLeaf:
      switch (slices[op1].u.leaf.end)
      {
        case EDirect:
          break;

        case ESelf:
          if (OptFlag[keepmating] && !is_a_mating_piece_left())
            result = short_win;
          break;

        case EReflex:
        case ESemireflex:
          if (is_there_end_in_1(defender,op1))
            result = short_loss;
          else if (OptFlag[keepmating] && !is_a_mating_piece_left())
            result = short_win;
          break;

        default:
          assert(0);
          result = short_win;
          break;
      }

      break;

    default:
      assert(0);
      result = short_win;
      break;
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%d\n",result);
  return result;
}

/* Determine whether the defending side wins in n (>1) in direct play.
 * @param defender defending side
 * @param si slice identifier
 */
boolean dsr_does_defender_win_in_n(couleur defender, int n, slice_index si)
{
  couleur const attacker = advers(defender);
  boolean is_defender_immobile = true;
  boolean refutation_found = false;
  int ntcount = 0;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%d",n);
  TraceFunctionParam("%d\n",si);

  /* check whether `black' can reach a position that is already
  ** marked unsolvable for white in the hash table. */
  /* TODO should we? i.e. do it or remove comment */

  switch (dsr_does_defender_win_short(defender,si))
  {
    case short_win:
      return true;

    case short_loss:
      return false;

    default:
      ; /* nothing */
  }

  if (n>max_len_threat
	  && !echecc(defender)
	  && !dsr_does_attacker_win(attacker,max_len_threat,si))
	return true;

  if (OptFlag[solflights] && has_too_many_flights(defender))
	return true;

  /* Check whether defender has more non trivial moves than he is
	 allowed to have. The number of such moves allowed
	 (max_nr_nontrivial) is entered using the nontrivial option.
  */
  if (n>min_length_nontrivial)
  {
	ntcount = count_non_trivial(defender,si);
	if (max_nr_nontrivial<ntcount)
	  return true;
	else
	  max_nr_nontrivial -= ntcount;
  } /* nontrivial */

  move_generation_mode=
      n>1
      ? move_generation_mode_opti_per_couleur[defender]
      : move_generation_optimized_by_killer_move;
  genmove(defender);
  move_generation_mode= move_generation_optimized_by_killer_move;

  while (!refutation_found && encore())
  {
    TraceValue("%d\n",n);
    TraceCurrentMove();
	if (jouecoup()
        && !echecc(defender))
	{
	  is_defender_immobile = false;
	  if (!dsr_does_attacker_win(attacker,n,si))
	  {
        TraceValue("%d",n);
        TraceText(" refutes\n");
		refutation_found = true;
		coupfort();
	  }
	}
	repcoup();
  }
  finply();

  if (n>min_length_nontrivial)
	max_nr_nontrivial += ntcount;

  return refutation_found || is_defender_immobile;
}

/* Determine whether the defending side wins in 0 (its final half
 * move) in direct play.
 * @param defender defending side
 * @param si slice identifier
 */
boolean dsr_leaf_does_defender_win_in_0(couleur defender, slice_index si)
{
  couleur const attacker = advers(defender);
  switch (slices[si].u.leaf.end)
  {
    case EDirect:
      return !goal_checkers[slices[si].u.leaf.goal](attacker);

    case ESelf:
    case EReflex:
    case ESemireflex:
      return sr_does_defender_win_in_0(defender,si);

    default:
      assert(0);
      return true;
  }
}

/* Determine whether the defending side wins in 0 (its final half
 * move) in direct play.
 * @param defender defending side
 * @param si slice identifier
 */
boolean dsr_sequence_does_defender_win_in_0(couleur defender,
                                            slice_index si)
{
  slice_index const op1 = slices[si].u.composite.op1;
  switch (slices[op1].type)
  {
    case STLeaf:
      return dsr_leaf_does_defender_win_in_0(defender,op1);

    default:
      assert(0);
      return true;
  }
}

/* Determine whether the defending side wins in a quodlibet in direct play.
 * @param defender defending side
 * @param si slice identifier
 */
boolean dsr_quodlibet_does_defender_win_in_0(couleur defender,
                                             slice_index si)
{
  slice_index const op1 = slices[si].u.composite.op1;
  slice_index const op2 = slices[si].u.composite.op2;

  switch (slices[op1].type)
  {
    case STLeaf:
      if (!dsr_leaf_does_defender_win_in_0(defender,op1))
        return false;
      else
        break;

    default:
      assert(0);
      return true;
  }

  switch (slices[op2].type)
  {
    case STLeaf:
      if (!dsr_leaf_does_defender_win_in_0(defender,op2))
        return false;
      else
        break;

    default:
      assert(0);
      return true;
  }

  return true;
}

/* Determine whether the defending side wins in 0  in direct play.
 * @param defender defending side
 * @param si slice identifier
 */
boolean dsr_does_defender_win_in_0(couleur defender, slice_index si)
{
  switch (slices[si].type)
  {
    case STQuodlibet:
      return dsr_quodlibet_does_defender_win_in_0(defender,si);

    case STSequence:
      return dsr_sequence_does_defender_win_in_0(defender,si);

    default:
      assert(0);
      return true;
  }
}

/* Determine whether the defender wins in a self/reflex stipulation in
 * n.
 * @param defender defending side (at move)
 * @param n number of moves until end state has to be reached
 * @return true iff defender wins
 */
boolean dsr_does_defender_win(couleur defender, int n, slice_index si)
{
  boolean result;
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%d",n);
  TraceFunctionParam("%d\n",si);
  if (n==0)
    result = dsr_does_defender_win_in_0(defender,si);
  else if (!slices[si].u.composite.is_exact
           && !dsr_does_defender_win_in_0(defender,si))
    result = false;
  else
    result = dsr_does_defender_win_in_n(defender,n,si);

  TraceFunctionExit(__func__);
  TraceValue("n:%d",n);
  TraceValue("%d",si);
  TraceFunctionResult("%d\n",result);
  return result;
} /* dsr_does_defender_win */

/* assert()s below this line must remain active even in "productive"
 * executables. */
#undef NDEBUG
#include <assert.h>

/* Check assumptions made in the hashing module. Abort if one of them
 * isn't met.
 * This is called from checkGlobalAssumptions() once at program start.
 */
void check_hash_assumptions(void)
{
  /* SmallEncode uses 1 byte for both row and file of a square */
  assert(nr_rows_on_board<1<<(CHAR_BIT/2));
  assert(nr_files_on_board<1<<(CHAR_BIT/2));

  /* LargeEncode() uses 1 bit per square */
  assert(nr_files_on_board<=CHAR_BIT);

  /* the encoding functions encode Flags as 2 bytes */
  assert(PieSpCount<=2*CHAR_BIT);
  
  assert(sizeof(dhtElement)==sizeof(genericElement_t));
  assert(sizeof(dhtElement)==sizeof(whDirElement_t));
  assert(sizeof(dhtElement)==sizeof(helpElement_t));
  assert(sizeof(dhtElement)==sizeof(serElement_t));

  {
    genericElement_t ge;
    
    /* using this rather than &ge further down keeps gcc quiet: */
    genericElement_t *pge = &ge;

    ge.data.what = WhDirSucc;
    assert(WhDirSucc==((whDirElement_t*)pge)->data.what);
    assert(WhDirSucc==((helpElement_t*)pge)->data.what);
    assert(WhDirSucc==((serElement_t*)pge)->data.what);
  }
}
