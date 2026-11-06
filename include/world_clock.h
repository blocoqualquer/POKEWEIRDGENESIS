#ifndef GUARD_WORLD_CLOCK_H
#define GUARD_WORLD_CLOCK_H

#include "global.h"

// ---------------------------------------------------------------------------
// POKEWEIRDGENESIS world clock
//
// Time scale: 1 in-game minute = 60 frames (1 real second at 60 fps),
// so one in-game day (1440 minutes) lasts 24 real minutes.
//
// Calendar: 12 months of 28 days (4 exact weeks). Because every month is a
// whole number of weeks, day 1 of every month is always a Sunday.
// The game always starts on Sunday, January 1st, 06:00 (year 1).
// ---------------------------------------------------------------------------

#define WORLD_CLOCK_FRAMES_PER_MINUTE   60
#define WORLD_CLOCK_MINUTES_PER_HOUR    60
#define WORLD_CLOCK_HOURS_PER_DAY       24
#define WORLD_CLOCK_DAYS_PER_WEEK        7
#define WORLD_CLOCK_DAYS_PER_MONTH      28
#define WORLD_CLOCK_MONTHS_PER_YEAR     12

#define WORLD_CLOCK_START_YEAR           1
#define WORLD_CLOCK_START_MONTH          MONTH_JANUARY
#define WORLD_CLOCK_START_DAY            1   // days are 1-based
#define WORLD_CLOCK_START_HOUR           6
#define WORLD_CLOCK_START_MINUTE         0

enum WorldClockWeekday
{
    WEEKDAY_SUNDAY,
    WEEKDAY_MONDAY,
    WEEKDAY_TUESDAY,
    WEEKDAY_WEDNESDAY,
    WEEKDAY_THURSDAY,
    WEEKDAY_FRIDAY,
    WEEKDAY_SATURDAY,
};

enum WorldClockMonth
{
    MONTH_JANUARY,
    MONTH_FEBRUARY,
    MONTH_MARCH,
    MONTH_APRIL,
    MONTH_MAY,
    MONTH_JUNE,
    MONTH_JULY,
    MONTH_AUGUST,
    MONTH_SEPTEMBER,
    MONTH_OCTOBER,
    MONTH_NOVEMBER,
    MONTH_DECEMBER,
};

enum WorldClockSeason
{
    SEASON_SPRING,
    SEASON_SUMMER,
    SEASON_AUTUMN,
    SEASON_WINTER,
    SEASON_COUNT,
};

// struct WorldClock is defined in global.h (it lives inside SaveBlock2).

void WorldClock_Reset(void);
void WorldClock_Tick(void);
bool8 WorldClock_IsPaused(void);

u16 WorldClock_GetYear(void);
u8 WorldClock_GetMonth(void);
u8 WorldClock_GetDay(void);
u8 WorldClock_GetHour(void);
u8 WorldClock_GetMinute(void);
u8 WorldClock_GetWeekday(void);
u8 WorldClock_GetSeason(void);
bool8 WorldClock_IsNight(void);

#endif // GUARD_WORLD_CLOCK_H
