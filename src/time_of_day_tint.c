#include "global.h"
#include "palette.h"
#include "world_clock.h"
#include "time_of_day_tint.h"
#include "constants/map_types.h"

// Each channel is multiplied by (value / 32), so 32 leaves a channel alone and
// anything above 32 boosts it (clamped to the 5-bit maximum of 31).
#define TINT_NEUTRAL 32
#define TINT_MINUTE(hour, minute) ((hour) * 60 + (minute))

struct TintKeyframe
{
    u16 minute; // minute of the day, 0-1440
    u8 r;
    u8 g;
    u8 b;
};

// The filter is interpolated linearly between consecutive keyframes. Dawn and
// dusk are long ramps centred around the phase boundaries (see
// constants/world_clock.h), so there is never a sudden jump.
static const struct TintKeyframe sTintKeyframes[] =
{
    { TINT_MINUTE( 0,  0), 16, 19, 28 }, // night: dark and blue
    { TINT_MINUTE( 4, 30), 16, 19, 28 },
    { TINT_MINUTE( 6, 30), 33, 32, 31 }, // morning: crisp, very slightly warm
    { TINT_MINUTE(10, 30), 33, 32, 31 },
    { TINT_MINUTE(12,  0), 32, 32, 32 }, // day: no filter at all
    { TINT_MINUTE(15,  0), 32, 32, 32 },
    { TINT_MINUTE(17,  0), 36, 31, 24 }, // afternoon: golden hour
    { TINT_MINUTE(19,  0), 36, 31, 24 },
    { TINT_MINUTE(21,  0), 16, 19, 28 }, // night again
    { TINT_MINUTE(24,  0), 16, 19, 28 },
};

// Per-channel lookup tables, rebuilt only when the minute or the map type changes.
static EWRAM_DATA u8 sTintTable[3][32] = {};
static EWRAM_DATA bool8 sTintActive = FALSE;
static EWRAM_DATA u16 sTintStamp = 0;

#define TINT_COLOR(c) (sTintTable[0][(c) & 31] \
                     | (sTintTable[1][((c) >> 5) & 31] << 5) \
                     | (sTintTable[2][((c) >> 10) & 31] << 10))

static bool8 IsTintedMapType(u8 mapType)
{
    return mapType == MAP_TYPE_TOWN
        || mapType == MAP_TYPE_CITY
        || mapType == MAP_TYPE_ROUTE
        || mapType == MAP_TYPE_OCEAN_ROUTE;
}

static u8 Lerp(u8 from, u8 to, u16 elapsed, u16 span)
{
    return from + ((s32)to - (s32)from) * elapsed / span;
}

static void GetTintMultipliers(u16 now, u8 *mult)
{
    u8 i;
    const struct TintKeyframe *prev;
    const struct TintKeyframe *next;
    u16 span;
    u16 elapsed;

    for (i = 1; i < ARRAY_COUNT(sTintKeyframes) - 1; i++)
    {
        if (now <= sTintKeyframes[i].minute)
            break;
    }
    prev = &sTintKeyframes[i - 1];
    next = &sTintKeyframes[i];
    span = next->minute - prev->minute;
    elapsed = now - prev->minute;
    if (elapsed > span)
        elapsed = span;

    mult[0] = Lerp(prev->r, next->r, elapsed, span);
    mult[1] = Lerp(prev->g, next->g, elapsed, span);
    mult[2] = Lerp(prev->b, next->b, elapsed, span);
}

static void RebuildTintTables(u16 now, u8 mapType)
{
    u8 mult[3];
    u8 channel;
    u8 value;
    u16 out;

    sTintActive = FALSE;
    if (!IsTintedMapType(mapType))
        return;

    GetTintMultipliers(now, mult);
    if (mult[0] == TINT_NEUTRAL && mult[1] == TINT_NEUTRAL && mult[2] == TINT_NEUTRAL)
        return;

    for (channel = 0; channel < 3; channel++)
    {
        for (value = 0; value < 32; value++)
        {
            out = (value * mult[channel] + 16) >> 5;
            if (out > 31)
                out = 31;
            sTintTable[channel][value] = out;
        }
    }
    sTintActive = TRUE;
}

// Copies the faded palette buffer to palette RAM, filtering the colours.
// BG palettes 13-15 hold the text windows and are copied untouched.
static void TintedPaletteCopy(const u16 *src, u16 *dest)
{
    u16 i;
    u16 color;

    for (i = 0; i < 13 * 16; i++)
    {
        color = src[i];
        dest[i] = TINT_COLOR(color);
    }
    for (; i < 16 * 16; i++)
        dest[i] = src[i];
    for (; i < 32 * 16; i++)
    {
        color = src[i];
        dest[i] = TINT_COLOR(color);
    }
}

// Drop-in replacement for TransferPlttBuffer() in the overworld VBlank callback.
void TimeOfDayTint_TransferPlttBuffer(void)
{
    u16 now = WorldClock_GetHour() * 60 + WorldClock_GetMinute();
    u8 mapType = gMapHeader.mapType;
    u16 stamp = (mapType << 11) | now;

    if (stamp != sTintStamp)
    {
        sTintStamp = stamp;
        RebuildTintTables(now, mapType);
    }

    gPlttBufferTransferHook = sTintActive ? TintedPaletteCopy : NULL;
    TransferPlttBuffer();
    gPlttBufferTransferHook = NULL;
}
