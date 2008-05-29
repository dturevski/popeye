#include "../priority.h"
#include <windows.h>

void set_nice_priority()
{
  /* BELOW_NORMAL_PRIORITY_CLASS:
   * Process that has priority above * IDLE_PRIORITY_CLASS but below
   * NORMAL_PRIORITY_CLASS.
   *
   * NORMAL_PRIORITY_CLASS:
   * Process with no special scheduling needs.
   */
  SetPriorityClass(GetCurrentProcess(),
                   BELOW_NORMAL_PRIORITY_CLASS);
}
