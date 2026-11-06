#ifndef GUARD_TIME_OF_DAY_TINT_H
#define GUARD_TIME_OF_DAY_TINT_H

#include "global.h"

// Screen colour filter that follows the world clock (night is blue and dark,
// the afternoon is golden, ...). It is applied while the palettes are copied
// to video memory during the overworld VBlank, so it sits on top of everything
// else (weather, screen fades, flashes) without touching the palette buffers.
//
// Only outdoor maps are tinted. The text window palettes (BG 13-15) are exempt.
//
// To tune the look, edit sTintKeyframes in src/time_of_day_tint.c.
void TimeOfDayTint_TransferPlttBuffer(void);

#endif // GUARD_TIME_OF_DAY_TINT_H
