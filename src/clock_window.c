#include "global.h"
#include "gflib.h"
#include "characters.h"
#include "menu.h"
#include "new_menu_helpers.h"
#include "world_clock.h"
#include "clock_window.h"

// VRAM tile budget (start menu screen): safari stats 0x008-0x02F,
// this window 0x030-0x05F, help message window starts at 0x08F.
static const struct WindowTemplate sClockWindowTemplate = {
    .bg = 0,
    .tilemapLeft = 1,
    .tilemapTop = 1,
    .width = 8,
    .height = 6,
    .paletteNum = 15,
    .baseBlock = 0x030
};

static const u8 sText_Am[] = _("Am");
static const u8 sText_Pm[] = _("Pm");

static const u8 sWeekdayNames[WORLD_CLOCK_DAYS_PER_WEEK][4] =
{
    [WEEKDAY_SUNDAY]    = _("SUN"),
    [WEEKDAY_MONDAY]    = _("MON"),
    [WEEKDAY_TUESDAY]   = _("TUE"),
    [WEEKDAY_WEDNESDAY] = _("WED"),
    [WEEKDAY_THURSDAY]  = _("THU"),
    [WEEKDAY_FRIDAY]    = _("FRI"),
    [WEEKDAY_SATURDAY]  = _("SAT"),
};

static const u8 sMonthNames[WORLD_CLOCK_MONTHS_PER_YEAR][4] =
{
    [MONTH_JANUARY]   = _("JAN"),
    [MONTH_FEBRUARY]  = _("FEB"),
    [MONTH_MARCH]     = _("MAR"),
    [MONTH_APRIL]     = _("APR"),
    [MONTH_MAY]       = _("MAY"),
    [MONTH_JUNE]      = _("JUN"),
    [MONTH_JULY]      = _("JUL"),
    [MONTH_AUGUST]    = _("AUG"),
    [MONTH_SEPTEMBER] = _("SEP"),
    [MONTH_OCTOBER]   = _("OCT"),
    [MONTH_NOVEMBER]  = _("NOV"),
    [MONTH_DECEMBER]  = _("DEC"),
};

// EWRAM starts zeroed, so "not active" is the zero value.
static EWRAM_DATA bool8 sClockWindowActive = FALSE;
static EWRAM_DATA u8 sClockWindowId = 0;
static EWRAM_DATA u8 sClockShownMinute = 0;
static EWRAM_DATA u8 sClockShownHour = 0;

static void PrintClockWindow(void)
{
    u8 buf[16];
    u8 *ptr;
    u8 hour = WorldClock_GetHour();
    u8 hour12 = hour % 12;

    if (hour12 == 0)
        hour12 = 12;

    FillWindowPixelBuffer(sClockWindowId, PIXEL_FILL(1));

    // Line 1: HH:MMAm
    ptr = ConvertIntToDecimalStringN(buf, hour12, STR_CONV_MODE_LEADING_ZEROS, 2);
    *ptr++ = CHAR_COLON;
    ptr = ConvertIntToDecimalStringN(ptr, WorldClock_GetMinute(), STR_CONV_MODE_LEADING_ZEROS, 2);
    StringCopy(ptr, hour < 12 ? sText_Am : sText_Pm);
    AddTextPrinterParameterized(sClockWindowId, FONT_NORMAL, buf, 4, 2, 0xFF, NULL);

    // Line 2: weekday
    AddTextPrinterParameterized(sClockWindowId, FONT_NORMAL, sWeekdayNames[WorldClock_GetWeekday() % WORLD_CLOCK_DAYS_PER_WEEK], 4, 16, 0xFF, NULL);

    // Line 3: MON DD
    ptr = StringCopy(buf, sMonthNames[WorldClock_GetMonth() % WORLD_CLOCK_MONTHS_PER_YEAR]);
    *ptr++ = CHAR_SPACE;
    ConvertIntToDecimalStringN(ptr, WorldClock_GetDay(), STR_CONV_MODE_LEADING_ZEROS, 2);
    AddTextPrinterParameterized(sClockWindowId, FONT_NORMAL, buf, 4, 30, 0xFF, NULL);

    sClockShownMinute = WorldClock_GetMinute();
    sClockShownHour = hour;
    CopyWindowToVram(sClockWindowId, COPYWIN_GFX);
}

void ClockWindow_Draw(void)
{
    if (sClockWindowActive)
        return;

    sClockWindowId = AddWindow(&sClockWindowTemplate);
    sClockWindowActive = TRUE;
    PutWindowTilemap(sClockWindowId);
    DrawStdWindowFrame(sClockWindowId, FALSE);
    PrintClockWindow();
}

void ClockWindow_Update(void)
{
    if (!sClockWindowActive)
        return;

#if WORLD_CLOCK_DEBUG_KEYS
    if (JOY_NEW(L_BUTTON))
        WorldClock_DebugAdvanceMinutes(60);
    if (JOY_NEW(R_BUTTON))
        WorldClock_DebugAdvanceMinutes(15);
#endif

    // Compare hour too, so a full 24h wrap on the same minute still refreshes.
    if (sClockShownMinute != WorldClock_GetMinute() || sClockShownHour != WorldClock_GetHour())
        PrintClockWindow();
}

void ClockWindow_Destroy(void)
{
    if (!sClockWindowActive)
        return;

    ClearStdWindowAndFrameToTransparent(sClockWindowId, FALSE);
    CopyWindowToVram(sClockWindowId, COPYWIN_GFX);
    RemoveWindow(sClockWindowId);
    sClockWindowActive = FALSE;
}
