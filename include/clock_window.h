#ifndef GUARD_CLOCK_WINDOW_H
#define GUARD_CLOCK_WINDOW_H

#include "global.h"

// Clock window shown in the top-left corner while the start menu is open.
// Layout (3 lines):   06:00Am
//                     MON
//                     JAN 03
void ClockWindow_Draw(void);
void ClockWindow_Update(void); // call every frame while the menu is open; reprints when the minute changes
void ClockWindow_Destroy(void);

#endif // GUARD_CLOCK_WINDOW_H
