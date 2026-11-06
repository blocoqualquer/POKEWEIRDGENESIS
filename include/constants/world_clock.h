#ifndef GUARD_CONSTANTS_WORLD_CLOCK_H
#define GUARD_CONSTANTS_WORLD_CLOCK_H

// Phases of the day. Plain #defines so event scripts can use them too, e.g.
//     specialvar VAR_RESULT, GetWorldClockPhase
//     goto_if_eq VAR_RESULT, WORLD_CLOCK_PHASE_NIGHT, EventScript_Foo
#define WORLD_CLOCK_PHASE_MORNING    0
#define WORLD_CLOCK_PHASE_DAY        1
#define WORLD_CLOCK_PHASE_AFTERNOON  2
#define WORLD_CLOCK_PHASE_NIGHT      3
#define WORLD_CLOCK_PHASE_COUNT      4

// First hour (0-23) of each phase. Night wraps around midnight:
// it runs from WORLD_CLOCK_NIGHT_START_HOUR until the next morning starts.
#define WORLD_CLOCK_MORNING_START_HOUR    6   // 06:00 - 10:59
#define WORLD_CLOCK_DAY_START_HOUR       11   // 11:00 - 15:59
#define WORLD_CLOCK_AFTERNOON_START_HOUR 16   // 16:00 - 19:59
#define WORLD_CLOCK_NIGHT_START_HOUR     20   // 20:00 - 05:59

#endif // GUARD_CONSTANTS_WORLD_CLOCK_H
