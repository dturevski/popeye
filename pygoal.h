#if !defined(PYGOAL_H)
#define PYGOAL_H

#include "boolean.h"
#include "py.h"

/* A goal describes a property
 * - of the position reached at the end of play, or
 * - of the last move of play.
 */

typedef enum
{
  goal_mate,
  goal_stale,
  goal_dblstale,
  goal_target,
  goal_check,
  goal_capture,
  goal_steingewinn,
  goal_ep,
  goal_doublemate,
  goal_countermate,
  goal_castling,
  goal_autostale,
  goal_circuit,
  goal_exchange,
  goal_circuitB,
  goal_exchangeB,
  goal_mate_or_stale,
  goal_any,
  goal_proof,
#if !defined(DATABASE)
  /* TODO why not if DATABASE? */
  goal_atob, /* TODO remove? is there a difference to goal_proof? */
#endif

  nr_goals,
  no_goal = nr_goals
} Goal;

/* how to decorate a move that reached a goal */
extern char const *goal_end_marker[nr_goals];

/* how to determine whether a goal has been reached */
typedef boolean (*goal_function_t)(couleur);
extern goal_function_t goal_checkers[nr_goals];

/* TODO get rid of this */
extern boolean testdblmate;

void initStipCheckers();

#endif
