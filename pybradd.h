#if !defined(PYBRADD_H)
#define PYBRADD_H

#include "boolean.h"
#include "pystip.h"
#include "pytable.h"

/* This module provides functionality dealing with the defending side
 * in STBranchDirect stipulation slices.
 */

/* Is there no chance left for the starting side at the move to win?
 * E.g. did the defender just capture that attacker's last potential
 * mating piece?
 * @param si identifies slice
 * @return true iff starter must resign
 */
boolean branch_d_defender_must_starter_resign(slice_index si);

/* Determine whether the starting side has made such a bad move that
 * it is clear without playing further that it is not going to win.
 * E.g. in s# or r#, has it taken the last potential mating piece of
 * the defender?
 * @param si slice identifier
 * @return true iff starter has lost
 */
boolean branch_d_defender_has_starter_apriori_lost(slice_index si);

/* Determine whether the attacker has won with his move just played
 * @param si slice index
 * @param n (odd) number of moves until goal (after the move just
 *          played)
 * @return true iff attacker has won
 */
boolean branch_d_defender_has_starter_won_in_n(slice_index si,
                                               stip_length_type n);

/* Determine whether the attacker has won with his move just played
 * independently of the non-starter's possible further play during the
 * current slice.
 * @param si slice identifier
 * @return true iff the starter has won
 */
boolean branch_d_defender_has_starter_won(slice_index si);

/* Determine whether the attacker has reached slice si's goal with his
 * move just played.
 * @param si slice identifier
 * @return true iff the starter reached the goal
 */
boolean branch_d_defender_has_starter_reached_goal(slice_index si);

/* The enumerators in the following enumeration type are sorted in
 * descending possibilities of the defender.
 */
typedef enum
{
  already_won,
  win,
  loss,
  already_lost
} d_defender_win_type;

/* Determine whether the defender wins after a move by the attacker
 * @param si slice index
 * @param n (odd) number of half moves until goal
 * @return whether the defender wins or loses, and how fast
 */
d_defender_win_type branch_d_defender_does_defender_win(slice_index si,
                                                        stip_length_type n);

/* Determine whether a slice.has just been solved with the just played
 * move by the non-starter
 * @param si slice identifier
 * @return true iff the non-starting side has just solved
 */
boolean branch_d_defender_has_non_starter_solved(slice_index si);

/* Determine and write the threats after the move that has just been
 * played in the current ply.
 * We have already determined that this move doesn't have more
 * refutations than allowed.
 * @param threats table where to add threats
 * @param si slice index
 * @param n (even) number of half moves until goal
 * @return the length of the shortest threat(s)
 */
int branch_d_defender_solve_threats(table threats,
                                    slice_index si,
                                    stip_length_type n);

/* Determine and write and variations after the move that has just
 * been played in the current ply.
 * We have already determined that this move doesn't have refutations
 * @param len_threat length of threats
 * @param threats table containing threats
 * @param si slice index
 * @param n (odd) number of half moves until goal
 */
void branch_d_defender_solve_variations_in_n(int len_threat,
                                             table threats,
                                             slice_index si,
                                             stip_length_type n);
/* Solve in a certain number of moves
 * @param si slice index
 * @param n (odd) number of half moves until goal
 */
void branch_d_defender_solve_in_n(slice_index si, stip_length_type n);

/* Solve at non-root level.
 * @param si slice index
 */
void branch_d_defender_solve(slice_index si);

/* Solve at root level.
 * @param refutations table containing the refutations (if any)
 * @param si slice index
 */
void branch_d_defender_root_solve(table refutations, slice_index si);

#endif
