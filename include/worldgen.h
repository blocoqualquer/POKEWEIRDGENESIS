#ifndef GUARD_WORLDGEN_H
#define GUARD_WORLDGEN_H

#include "gba/types.h"

// ---------------------------------------------------------------------------
// POKEWEIRDGENESIS world layout generator.
//
// The playable world is a set of rectangular ZONES (villages, castle towns,
// forests, deserts, ...) joined by ROUTES. Everything outside the zones and
// routes is not playable (on the minimap it is the sea around the island).
//
// The layout is fully determined by a 32-bit seed (taken from the trainer ID),
// so only the seed has to be stored in the save file. This file is plain C with
// no GBA dependencies, so the very same code is tested on a PC (devtools/worldgen).
//
// Units are metatiles (16x16 px): the world is WORLDGEN_SIZE x WORLDGEN_SIZE.
// ---------------------------------------------------------------------------

#define WORLDGEN_GRID       6                                   // zone slots per side
#define WORLDGEN_CELL       72                                  // metatiles per slot
#define WORLDGEN_SIZE       (WORLDGEN_GRID * WORLDGEN_CELL)     // 432
#define WORLDGEN_MARGIN     7                                   // free border inside each slot (routes run there)
#define WORLDGEN_MAX_ZONES  (WORLDGEN_GRID * WORLDGEN_GRID)
#define WORLDGEN_MAX_ROUTES 48
#define WORLDGEN_MIN_ZONES  12

// Routes are real corridors (like Route 1 in the original game), not thin
// paths: 11 or 13 tiles wide, almost a full GBA screen (15 tiles).
#define WORLDGEN_ROUTE_MIN_HALF_WIDTH 5
#define WORLDGEN_ROUTE_MAX_HALF_WIDTH 6

enum WorldZoneType
{
    ZONE_NONE,
    ZONE_VILLAGE,   // aldeia (the player starts in one)
    ZONE_CASTLE,    // cidade-castelo
    ZONE_FOREST,
    ZONE_PLAINS,    // campos abertos, good for farming
    ZONE_DESERT,
    ZONE_MOUNTAIN,
    ZONE_SWAMP,
    ZONE_LAKE,      // water, routes never cross it
    ZONE_TYPE_COUNT,
};

struct WorldZone
{
    u16 x;      // top-left corner, in metatiles
    u16 y;
    u8 w;       // size in metatiles
    u8 h;
    u8 type;    // enum WorldZoneType
    u8 pad;
};

// A route is a polyline of axis-aligned segments. Each segment is a corridor
// covering the points within `halfWidth` tiles of its centre line, so it is
// 2 * halfWidth + 1 tiles wide. The first and last points are just outside the
// border of the two zones joined.
struct WorldRoute
{
    u8 from;        // zone indexes
    u8 to;
    u8 numPoints;   // 2 (straight) to 4
    u8 halfWidth;
    u16 x[4];
    u16 y[4];
};

struct WorldLayout
{
    u32 seed;
    u8 numZones;
    u8 numRoutes;
    u8 startZone;   // index of the starting village
    u8 pad;
    struct WorldZone zones[WORLDGEN_MAX_ZONES];
    struct WorldRoute routes[WORLDGEN_MAX_ROUTES];
};

// Always produces the same layout for the same seed.
void WorldGen_Generate(struct WorldLayout *layout, u32 seed);

#endif // GUARD_WORLDGEN_H
