#ifndef GUARD_WORLDGEN_H
#define GUARD_WORLDGEN_H

#include "gba/types.h"

// ---------------------------------------------------------------------------
// POKEWEIRDGENESIS world layout generator.
//
// The playable world is a mosaic of rectangular ZONES (villages, castle towns,
// forests, deserts, long strips that act as routes, ...) that touch each other
// edge to edge, like the maps of the original game. There are no corridors
// between zones: you walk from one zone straight into the next one across the
// border they share (a LINK). Everything outside the zones is not playable (on
// the minimap it is the sea around the island).
//
// The layout is fully determined by a 32-bit seed (taken from the trainer ID),
// so only the seed has to be stored in the save file. This file is plain C with
// no GBA dependencies, so the very same code is tested on a PC (devtools/worldgen).
//
// Units are metatiles (16x16 px): the world is WORLDGEN_SIZE x WORLDGEN_SIZE.
// ---------------------------------------------------------------------------

#define WORLDGEN_SIZE        384
#define WORLDGEN_MAX_ZONES   80
#define WORLDGEN_MAX_LINKS   255
#define WORLDGEN_MIN_ZONES   16

#define WORLDGEN_MIN_SIDE    24     // smallest side of a normal zone
#define WORLDGEN_MIN_STRIP   14     // a strip (route) zone is 14-20 tiles thick
#define WORLDGEN_MAX_STRIP   20
#define WORLDGEN_MAX_SIDE    96     // longest side of any zone
#define WORLDGEN_MAX_AREA    8000   // keeps each zone map within the engine's map size limit
#define WORLDGEN_MIN_LINK    14     // two zones are connected when they share at least this much border

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
    ZONE_LAKE,      // water: nobody can cross it, so it has no links
    ZONE_ROUTE,     // rota: a long thin strip of open ground between its neighbours
    ZONE_TYPE_COUNT,
};

struct WorldZone
{
    u16 x;      // top-left corner, in metatiles
    u16 y;
    u16 w;      // size in metatiles
    u16 h;
    u8 type;    // enum WorldZoneType
    u8 pad;
};

// The border two zones share. You cross it to get from one zone to the other.
struct WorldLink
{
    u8 a;           // zone on the low side (left of / above the border)
    u8 b;           // zone on the high side
    u8 vertical;    // 1: the border is a vertical line at x = pos, running along y; 0: horizontal at y = pos, along x
    u8 len;         // length of the shared border (>= WORLDGEN_MIN_LINK)
    u16 pos;
    u16 start;      // where the shared part begins along the border
};

struct WorldLayout
{
    u32 seed;
    u8 numZones;
    u8 numLinks;
    u8 startZone;   // always 0: the starting village
    u8 pad;
    struct WorldZone zones[WORLDGEN_MAX_ZONES];
    struct WorldLink links[WORLDGEN_MAX_LINKS];
};

// Always produces the same layout for the same seed.
void WorldGen_Generate(struct WorldLayout *layout, u32 seed);

#endif // GUARD_WORLDGEN_H
