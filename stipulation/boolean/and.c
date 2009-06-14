#include "pyrecipr.h"
#include "pyslice.h"
#include "pyproc.h"
#include "trace.h"

#include <assert.h>


/* Allocate a reciprocal slice.
 * @param op1 1st operand
 * @param op2 2nd operand
 * @return index of allocated slice
 */
slice_index alloc_reciprocal_slice(slice_index op1, slice_index op2)
{
  slice_index const result = alloc_slice_index();

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",op1);
  TraceFunctionParam("%u",op2);
  TraceFunctionParamListEnd();

  assert(op1!=no_slice);
  assert(op2!=no_slice);

  slices[result].type = STReciprocal; 
  slices[result].u.reciprocal.op1 = op1;
  slices[result].u.reciprocal.op2 = op2;

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Is there no chance left for the starting side at the move to win?
 * E.g. did the defender just capture that attacker's last potential
 * mating piece?
 * Tests do not rely on the current position being hash-encoded.
 * @param si slice index
 * @return true iff starter must resign
 */
boolean reci_must_starter_resign(slice_index si)
{
  boolean result;
  slice_index const op1 = slices[si].u.reciprocal.op1;
  slice_index const op2 = slices[si].u.reciprocal.op2;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  TraceValue("%u",op1);
  TraceValue("%u\n",op2);

  result = (slice_must_starter_resign(op1)
            || slice_must_starter_resign(op2));

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Is there no chance left for the starting side at the move to win?
 * E.g. did the defender just capture that attacker's last potential
 * mating piece?
 * Tests may rely on the current position being hash-encoded.
 * @param si slice index
 * @return true iff starter must resign
 */
boolean reci_must_starter_resign_hashed(slice_index si)
{
  boolean result;
  slice_index const op1 = slices[si].u.reciprocal.op1;
  slice_index const op2 = slices[si].u.reciprocal.op2;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  TraceValue("%u",op1);
  TraceValue("%u\n",op2);

  result = (slice_must_starter_resign_hashed(op1)
            || slice_must_starter_resign_hashed(op2));

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Determine whether there is a solution at the end of a quodlibet
 * slice. 
 * @param si slice index
 * @return true iff slice si has a solution
 */
boolean reci_has_solution(slice_index si)
{
  slice_index const op1 = slices[si].u.reciprocal.op1;
  slice_index const op2 = slices[si].u.reciprocal.op2;
  boolean result;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  result = slice_has_solution(op1) && slice_has_solution(op2);

  TraceFunctionExit(__func__);
  TraceFunctionParam("%u",result);
  TraceFunctionParamListEnd();
  return result;
}

/* Determine whether a reciprocal slice.has just been solved with the
 * just played move by the non-starter
 * @param si slice identifier
 * @return true iff the non-starting side has just solved
 */
boolean reci_has_non_starter_solved(slice_index si)
{
  boolean result;
  slice_index const op1 = slices[si].u.reciprocal.op1;
  slice_index const op2 = slices[si].u.reciprocal.op2;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();
  TraceValue("%u",op1);
  TraceValue("%u\n",op2);

  result = (slice_has_non_starter_solved(op1)
            && slice_has_non_starter_solved(op2));

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Determine whether the starting side has made such a bad move that
 * it is clear without playing further that it is not going to win.
 * E.g. in s# or r#, has it taken the last potential mating piece of
 * the defender?
 * @param si slice identifier
 * @return true iff starter has lost
 */
boolean reci_has_starter_apriori_lost(slice_index si)
{
  boolean result;
  slice_index const op1 = slices[si].u.reciprocal.op1;
  slice_index const op2 = slices[si].u.reciprocal.op2;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();
  TraceValue("%u",op1);
  TraceValue("%u\n",op2);

  result = (slice_has_starter_apriori_lost(op1)
            || slice_has_starter_apriori_lost(op2));

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Determine whether the attacker has won with his move just played
 * independently of the non-starter's possible further play during the
 * current slice.
 * @param si slice identifier
 * @return true iff the starter has won
 */
boolean reci_has_starter_won(slice_index si)
{
  boolean result;
  slice_index const op1 = slices[si].u.reciprocal.op1;
  slice_index const op2 = slices[si].u.reciprocal.op2;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();
  TraceValue("%u",op1);
  TraceValue("%u\n",op2);

  result = slice_has_starter_won(op1) && slice_has_starter_won(op2);

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Determine whether the attacker has reached slice si's goal with his
 * move just played.
 * @param si slice identifier
 * @return true iff the starter reached the goal
 */
boolean reci_has_starter_reached_goal(slice_index si)
{
  boolean result;
  slice_index const op1 = slices[si].u.reciprocal.op1;
  slice_index const op2 = slices[si].u.reciprocal.op2;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  result = (slice_has_starter_reached_goal(op1)
            && slice_has_starter_reached_goal(op2));

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Write a priori unsolvability (if any) of a slice (e.g. forced
 * reflex mates).
 * Assumes slice_must_starter_resign(si)
 * @param si slice index
 */
void reci_write_unsolvability(slice_index si)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  TraceValue("%u",slices[si].u.reciprocal.op1);
  TraceValue("%u\n",slices[si].u.reciprocal.op2);

  slice_write_unsolvability(slices[si].u.reciprocal.op1);
  slice_write_unsolvability(slices[si].u.reciprocal.op2);

  TraceFunctionExit(__func__);
  TraceText("\n");
}

/* Find and write post key play
 * @param si slice index
 */
void reci_solve_postkey(slice_index si)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  slice_solve_postkey(slices[si].u.reciprocal.op1);
  slice_solve_postkey(slices[si].u.reciprocal.op2);

  TraceFunctionExit(__func__);
}

/* Determine and write continuations at end of reciprocal slice
 * @param continuations table where to store continuing moves (i.e. threats)
 * @param si index of quodlibet slice
 */
void reci_solve_continuations(table continuations, slice_index si)
{
  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  slice_solve_continuations(continuations,slices[si].u.reciprocal.op1);
  slice_solve_continuations(continuations,slices[si].u.reciprocal.op2);

  TraceFunctionExit(__func__);
  TraceText("\n");
}

/* Spin off a set play slice at root level
 * @param si slice index
 * @return set play slice spun off; no_slice if not applicable
 */
slice_index reci_root_make_setplay_slice(slice_index si)
{
  slice_index const op1 = slices[si].u.reciprocal.op1;
  slice_index const op2 = slices[si].u.reciprocal.op2;
  slice_index op1_set;
  slice_index result = no_slice;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  op1_set = slice_root_make_setplay_slice(op1);
  if (op1_set!=no_slice)
  {
    slice_index const op2_set = slice_root_make_setplay_slice(op2);
    if (op2_set==no_slice)
      dealloc_slice_index(op1_set);
    else
      result = alloc_reciprocal_slice(op1_set,op2_set);
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Solve at root level at the end of a reciprocal slice
 * @param si slice index
 * @return true iff >=1 solution was found
 */
boolean reci_root_solve(slice_index si)
{
  boolean result = false;

  slice_index const op1 = slices[si].u.reciprocal.op1;
  slice_index const op2 = slices[si].u.reciprocal.op2;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  TraceValue("%u",op1);
  TraceValue("%u\n",op2);

  if (slice_has_solution(op2) && slice_root_solve(op1))
  {
    boolean const result2 = slice_root_solve(op2);
    assert(result2);
    result = true;
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Write the key just played
 * @param si slice index
 * @param type type of attack
 */
void reci_root_write_key(slice_index si, attack_type type)
{
  /* TODO does this make sense? */
  slice_root_write_key(slices[si].u.reciprocal.op1,type);
  slice_root_write_key(slices[si].u.reciprocal.op2,type);
}

/* Continue solving at the end of a reciprocal slice
 * @param si slice index
 * @return true iff >=1 solution was found
 */
boolean reci_solve(slice_index si)
{
  boolean result = false;
  slice_index const op1 = slices[si].u.reciprocal.op1;
  slice_index const op2 = slices[si].u.reciprocal.op2;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParamListEnd();

  TraceValue("%u",op1);
  TraceValue("%u\n",op2);

  if (slice_has_solution(op2) && slice_solve(op1))
  {
    boolean const result2 = slice_solve(op2);
    assert(result2);
    result = true;
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Detect starter field with the starting side if possible. 
 * @param si identifies slice
 * @param same_side_as_root does si start with the same side as root?
 * @return does the leaf decide on the starter?
 */
who_decides_on_starter reci_detect_starter(slice_index si,
                                           boolean same_side_as_root)
{
  slice_index const op1 = slices[si].u.reciprocal.op1;
  slice_index const op2 = slices[si].u.reciprocal.op2;
  who_decides_on_starter result;
  who_decides_on_starter result1;
  who_decides_on_starter result2;

  TraceFunctionEntry(__func__);
  TraceFunctionParam("%u",si);
  TraceFunctionParam("%u",same_side_as_root);
  TraceFunctionParamListEnd();

  result1 = slice_detect_starter(op1,same_side_as_root);
  result2 = slice_detect_starter(op2,same_side_as_root);

  if (slice_get_starter(op1)==no_side)
    /* op1 can't tell - let's tell him */
    slice_impose_starter(op1,slice_get_starter(op2));
  else if (slice_get_starter(op2)==no_side)
    /* op2 can't tell - let's tell him */
    slice_impose_starter(op2,slice_get_starter(op1));

  if (result1==dont_know_who_decides_on_starter)
    result = result2;
  else
  {
    assert(result2==dont_know_who_decides_on_starter);
    result = result1;
  }

  TraceFunctionExit(__func__);
  TraceFunctionResult("%u",result);
  TraceFunctionResultEnd();
  return result;
}

/* Impose the starting side on a slice.
 * @param si identifies slice
 * @param s starting side of leaf
 */
void reci_impose_starter(slice_index si, Side s)
{
  slice_impose_starter(slices[si].u.reciprocal.op1,s);
  slice_impose_starter(slices[si].u.reciprocal.op2,s);
}
