#include "global.h"
#include "world_clock.h"

// Month -> season. Edit this table to change the calendar's seasons.
// Default: northern-style (January is deep winter).
static const u8 sSeasonByMonth[WORLD_CLOCK_MONTHS_PER_YEAR] =
{
    [MONTH_JANUARY]   = SEASON_WINTER,
    [MONTH_FEBRUARY]  = SEASON_WINTER,
    [MONTH_MARCH]     = SEASON_SPRING,
    [MONTH_APRIL]     = SEASON_SPRING,
    [MONTH_MAY]       = SEASON_SPRING,
    [MONTH_JUNE]      = SEASON_SUMMER,
    [MONTH_JULY]      = SEASON_SUMMER,
    [MONTH_AUGUST]    = SEASON_SUMMER,
    [MONTH_SEPTEMBER] = SEASON_AUTUMN,
    [MONTH_OCTOBER]   = SEASON_AUTUMN,
    [MONTH_NOVEMBER]  = SEASON_AUTUMN,
    [MONTH_DECEMBER]  = SEASON_WINTER,
};

#define NIGHT_START_HOUR 20  // 20:00
#define NIGHT_END_HOUR    6  // 06:00

// Brand new games and saves made before the clock existed (zeroed memory)
// both end up at the starting date.
static bool8 WorldClock_IsValid(const struct WorldClock *clock)
{
    return clock->year >= 1
        && clock->month < WORLD_CLOCK_MONTHS_PER_YEAR
        && clock->day >= 1 && clock->day <= WORLD_CLOCK_DAYS_PER_MONTH
        && clock->hour < WORLD_CLOCK_HOURS_PER_DAY
        && clock->minute < WORLD_CLOCK_MINUTES_PER_HOUR
        && clock->weekday < WORLD_CLOCK_DAYS_PER_WEEK
        && clock->frames < WORLD_CLOCK_FRAMES_PER_MINUTE;
}

void WorldClock_Reset(void)
{
    struct WorldClock *clock = &gSaveBlock2Ptr->worldClock;

    clock->year = WORLD_CLOCK_START_YEAR;
    clock->month = WORLD_CLOCK_START_MONTH;
    clock->day = WORLD_CLOCK_START_DAY;
    clock->hour = WORLD_CLOCK_START_HOUR;
    clock->minute = WORLD_CLOCK_START_MINUTE;
    clock->weekday = WEEKDAY_SUNDAY;
    clock->frames = 0;
}

// Called once per frame while the play time counter is running.
void WorldClock_Tick(void)
{
    struct WorldClock *clock = &gSaveBlock2Ptr->worldClock;

    if (!WorldClock_IsValid(clock))
        WorldClock_Reset();

    if (++clock->frames < WORLD_CLOCK_FRAMES_PER_MINUTE)
        return;
    clock->frames = 0;

    if (++clock->minute < WORLD_CLOCK_MINUTES_PER_HOUR)
        return;
    clock->minute = 0;

    if (++clock->hour < WORLD_CLOCK_HOURS_PER_DAY)
        return;
    clock->hour = 0;

    // New day
    if (++clock->weekday >= WORLD_CLOCK_DAYS_PER_WEEK)
        clock->weekday = WEEKDAY_SUNDAY;

    if (++clock->day <= WORLD_CLOCK_DAYS_PER_MONTH)
        return;
    clock->day = 1;

    if (++clock->month < WORLD_CLOCK_MONTHS_PER_YEAR)
        return;
    clock->month = MONTH_JANUARY;

    if (clock->year < 0xFFFF)
        clock->year++;
}

u16 WorldClock_GetYear(void)    { return gSaveBlock2Ptr->worldClock.year; }
u8 WorldClock_GetMonth(void)    { return gSaveBlock2Ptr->worldClock.month; }
u8 WorldClock_GetDay(void)      { return gSaveBlock2Ptr->worldClock.day; }
u8 WorldClock_GetHour(void)     { return gSaveBlock2Ptr->worldClock.hour; }
u8 WorldClock_GetMinute(void)   { return gSaveBlock2Ptr->worldClock.minute; }
u8 WorldClock_GetWeekday(void)  { return gSaveBlock2Ptr->worldClock.weekday; }

u8 WorldClock_GetSeason(void)
{
    return sSeasonByMonth[gSaveBlock2Ptr->worldClock.month % WORLD_CLOCK_MONTHS_PER_YEAR];
}

bool8 WorldClock_IsNight(void)
{
    u8 hour = gSaveBlock2Ptr->worldClock.hour;

    return hour >= NIGHT_START_HOUR || hour < NIGHT_END_HOUR;
}
