#include "worldgen.h"

#define WORLDGEN_SOFT_ZONES 46

// Plain C, no GBA dependencies: compiled both into the ROM (agbcc, C89 style
// declarations at the top of blocks) and into devtools/worldgen for testing.
//
// How a world is made:
//   1. Partition: the square world is cut into rectangles (guillotine/BSP).
//      Now and then a thin strip is cut off, which becomes a route zone. The
//      rectangles tile the world exactly, so neighbours touch edge to edge.
//   2. Prune: rectangles far from the centre are dropped, with some noise, so
//      the land has a ragged coast. Only what connects to the centre survives.
//   3. Types: start village near the centre; other zones get a type by shape
//      (strips -> routes, small squarish ones may be towns), neighbours tend to
//      share a biome; at least two castles; a few lakes that never split land.
//   4. Links: every pair of zones sharing enough border is linked.

static u32 sRng;

static u32 Rand32(void)
{
    u32 x = sRng;

    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    sRng = x;
    return x;
}

static s32 RandRange(s32 lo, s32 hi)
{
    if (hi <= lo)
        return lo;
    return lo + (s32)((Rand32() >> 8) % (u32)(hi - lo + 1));
}

static u8 RandPercent(u8 percent)
{
    return (Rand32() >> 8) % 100 < percent;
}

static s32 Min(s32 a, s32 b) { return a < b ? a : b; }
static s32 Max(s32 a, s32 b) { return a > b ? a : b; }
static s32 Abs(s32 a) { return a < 0 ? -a : a; }

static s32 SharedBorder(const struct WorldZone *a, const struct WorldZone *b, u8 *vertical, s32 *pos, s32 *start)
{
    s32 ax1 = a->x + a->w;
    s32 ay1 = a->y + a->h;
    s32 bx1 = b->x + b->w;
    s32 by1 = b->y + b->h;
    s32 lo;
    s32 hi;

    if (ax1 == b->x || bx1 == a->x)
    {
        lo = Max(a->y, b->y);
        hi = Min(ay1, by1);
        if (hi > lo)
        {
            *vertical = 1;
            *pos = (ax1 == b->x) ? ax1 : bx1;
            *start = lo;
            return hi - lo;
        }
    }
    if (ay1 == b->y || by1 == a->y)
    {
        lo = Max(a->x, b->x);
        hi = Min(ax1, bx1);
        if (hi > lo)
        {
            *vertical = 0;
            *pos = (ay1 == b->y) ? ay1 : by1;
            *start = lo;
            return hi - lo;
        }
    }
    return 0;
}

static u8 IsAdjacent(const struct WorldZone *a, const struct WorldZone *b)
{
    u8 vertical;
    s32 pos;
    s32 start;

    return SharedBorder(a, b, &vertical, &pos, &start) >= WORLDGEN_MIN_LINK;
}

// ---------------------------------------------------------------------------
// 1. Partition
// ---------------------------------------------------------------------------
static u8 TrySplit(struct WorldZone *z, u8 *count, u8 i, u8 onlyForced)
{
    struct WorldZone *r = &z[i];
    struct WorldZone *n = &z[*count];
    s32 w = r->w;
    s32 h = r->h;
    s32 x0 = r->x;
    s32 y0 = r->y;
    s32 area = w * h;
    u8 forced = w > WORLDGEN_MAX_SIDE || h > WORLDGEN_MAX_SIDE || area > WORLDGEN_MAX_AREA;
    u8 canVertical = w >= 2 * WORLDGEN_MIN_SIDE;
    u8 canHorizontal = h >= 2 * WORLDGEN_MIN_SIDE;
    u8 vertical;
    u8 chance;
    s32 t;
    s32 pos;

    if (onlyForced && !forced)
        return 0;
    if (Max(w, h) >= 64 && Max(w, h) <= WORLDGEN_MAX_SIDE && Min(w, h) >= WORLDGEN_MIN_SIDE + WORLDGEN_MAX_STRIP && RandPercent(forced ? 40 : 20))
    {
        t = RandRange(WORLDGEN_MIN_STRIP, WORLDGEN_MAX_STRIP);
        *n = *r;
        if (w >= h)
        {
            n->h = t;
            r->h = h - t;
            if (Rand32() & 1)
            {
                n->y = y0;
                r->y = y0 + t;
            }
            else
            {
                n->y = y0 + h - t;
            }
        }
        else
        {
            n->w = t;
            r->w = w - t;
            if (Rand32() & 1)
            {
                n->x = x0;
                r->x = x0 + t;
            }
            else
            {
                n->x = x0 + w - t;
            }
        }
        (*count)++;
        return 1;
    }

    if (!canVertical && !canHorizontal)
        return 0;
    if (!forced)
    {
        chance = area > 3600 ? 75 : (area > 2000 ? 45 : 15);
        if (!RandPercent(chance))
            return 0;
    }

    if (canVertical && canHorizontal)
    {
        if (w * 4 >= h * 5)
            vertical = 1;
        else if (h * 4 >= w * 5)
            vertical = 0;
        else
            vertical = Rand32() & 1;
    }
    else
    {
        vertical = canVertical;
    }

    *n = *r;
    if (vertical)
    {
        pos = RandRange(WORLDGEN_MIN_SIDE, w - WORLDGEN_MIN_SIDE);
        n->x = x0 + pos;
        n->w = w - pos;
        r->w = pos;
    }
    else
    {
        pos = RandRange(WORLDGEN_MIN_SIDE, h - WORLDGEN_MIN_SIDE);
        n->y = y0 + pos;
        n->h = h - pos;
        r->h = pos;
    }
    (*count)++;
    return 1;
}

static u8 Partition(struct WorldZone *z)
{
    u8 count = 1;
    u8 i = 0;

    z[0].x = 0;
    z[0].y = 0;
    z[0].w = WORLDGEN_SIZE;
    z[0].h = WORLDGEN_SIZE;
    z[0].type = ZONE_NONE;
    z[0].pad = 0;

    while (i < count)
    {
        // Soft cap leaves room so oversized leaves can still be forced to split.
        if (count < WORLDGEN_MAX_ZONES && TrySplit(z, &count, i, count >= WORLDGEN_SOFT_ZONES))
            continue;
        i++;
    }
    return count;
}

// ---------------------------------------------------------------------------
// 2. Prune
// ---------------------------------------------------------------------------
#define COAST_NOISE     40
#define COAST_THRESHOLD 118

static u8 FloodKept(const struct WorldZone *z, u8 n, const u8 *keep, u8 *reached, u8 startIdx)
{
    u8 queue[WORLDGEN_MAX_ZONES];
    u8 head = 0;
    u8 tail = 0;
    u8 i;
    u8 j;

    for (i = 0; i < n; i++)
        reached[i] = 0;
    reached[startIdx] = 1;
    queue[tail++] = startIdx;
    while (head < tail)
    {
        i = queue[head++];
        for (j = 0; j < n; j++)
        {
            if (keep[j] && !reached[j] && IsAdjacent(&z[i], &z[j]))
            {
                reached[j] = 1;
                queue[tail++] = j;
            }
        }
    }
    return tail;
}

static void PruneToIsland(struct WorldZone *z, u8 *count)
{
    u8 n = *count;
    u8 keep[WORLDGEN_MAX_ZONES];
    u8 reached[WORLDGEN_MAX_ZONES];
    u8 dist[WORLDGEN_MAX_ZONES];
    u8 noise[WORLDGEN_MAX_ZONES];
    s32 dx;
    s32 dy;
    s32 threshold;
    u8 centre;
    u8 found;
    u8 i;
    u8 m;

    for (i = 0; i < n; i++)
    {
        dx = Abs((s32)z[i].x + z[i].w / 2 - WORLDGEN_SIZE / 2) * 200 / WORLDGEN_SIZE;
        dy = Abs((s32)z[i].y + z[i].h / 2 - WORLDGEN_SIZE / 2) * 200 / WORLDGEN_SIZE;
        dist[i] = Max(dx, dy) + Min(dx, dy) / 2;
        noise[i] = RandRange(0, COAST_NOISE);
    }

    centre = 0;
    for (i = 1; i < n; i++)
    {
        if (dist[i] < dist[centre])
            centre = i;
    }

    for (threshold = COAST_THRESHOLD; ; threshold += 8)
    {
        for (i = 0; i < n; i++)
            keep[i] = (dist[i] + noise[i] < threshold) || i == centre;
        found = FloodKept(z, n, keep, reached, centre);
        if (found >= WORLDGEN_MIN_ZONES || threshold > 400)
            break;
    }

    m = 0;
    for (i = 0; i < n; i++)
    {
        if (reached[i])
            z[m++] = z[i];
    }
    *count = m;
}

// ---------------------------------------------------------------------------
// 3. Types
// ---------------------------------------------------------------------------
static u8 IsWild(u8 type)
{
    return type == ZONE_FOREST || type == ZONE_PLAINS || type == ZONE_DESERT
        || type == ZONE_MOUNTAIN || type == ZONE_SWAMP;
}

static u8 PickWild(void)
{
    u32 roll = (Rand32() >> 8) % 100;

    if (roll < 30)
        return ZONE_FOREST;
    if (roll < 54)
        return ZONE_PLAINS;
    if (roll < 68)
        return ZONE_DESERT;
    if (roll < 86)
        return ZONE_MOUNTAIN;
    return ZONE_SWAMP;
}

static u8 NeighbourWildType(const struct WorldZone *z, u8 n, u8 i)
{
    u8 candidates[16];
    u8 count = 0;
    u8 j;

    for (j = 0; j < n && count < 16; j++)
    {
        if (j != i && IsWild(z[j].type) && IsAdjacent(&z[i], &z[j]))
            candidates[count++] = z[j].type;
    }
    if (count == 0)
        return ZONE_NONE;
    return candidates[RandRange(0, count - 1)];
}

static u8 StaysConnected(const struct WorldZone *z, u8 n, u8 skip)
{
    u8 reached[WORLDGEN_MAX_ZONES];
    u8 queue[WORLDGEN_MAX_ZONES];
    u8 head = 0;
    u8 tail = 0;
    u8 total = 0;
    u8 i;
    u8 j;

    for (i = 0; i < n; i++)
    {
        reached[i] = 0;
        if (i != skip && z[i].type != ZONE_LAKE)
            total++;
    }
    reached[0] = 1;
    queue[tail++] = 0;
    while (head < tail)
    {
        i = queue[head++];
        for (j = 0; j < n; j++)
        {
            if (!reached[j] && j != skip && z[j].type != ZONE_LAKE && IsAdjacent(&z[i], &z[j]))
            {
                reached[j] = 1;
                queue[tail++] = j;
            }
        }
    }
    return tail == total;
}

static u8 IsSquarish(const struct WorldZone *zone, s32 maxRatioTenths)
{
    return Max(zone->w, zone->h) * 10 <= Min(zone->w, zone->h) * maxRatioTenths;
}

static void ChooseStart(struct WorldZone *z, u8 n)
{
    struct WorldZone tmp;
    s32 best = -1;
    s32 bestDist = 0x7FFFFFFF;
    s32 d;
    u8 i;

    for (i = 0; i < n; i++)
    {
        d = Abs((s32)z[i].x + z[i].w / 2 - WORLDGEN_SIZE / 2) + Abs((s32)z[i].y + z[i].h / 2 - WORLDGEN_SIZE / 2);
        if (Min(z[i].w, z[i].h) >= 24 && IsSquarish(&z[i], 18) && (s32)z[i].w * z[i].h <= 2800 && d < bestDist)
        {
            bestDist = d;
            best = i;
        }
    }
    if (best < 0)
    {
        for (i = 0; i < n; i++)
        {
            d = Abs((s32)z[i].x + z[i].w / 2 - WORLDGEN_SIZE / 2) + Abs((s32)z[i].y + z[i].h / 2 - WORLDGEN_SIZE / 2);
            if (d < bestDist)
            {
                bestDist = d;
                best = i;
            }
        }
    }
    tmp = z[0];
    z[0] = z[best];
    z[best] = tmp;
    z[0].type = ZONE_VILLAGE;
}

static void AssignTypes(struct WorldZone *z, u8 n)
{
    u8 order[WORLDGEN_MAX_ZONES];
    u8 castles = 0;
    u8 k;
    u8 i;
    u8 type;
    u8 count = n - 1;
    s32 j;
    s32 area;
    s32 shortSide;
    u8 tmp;

    for (i = 1; i < n; i++)
    {
        z[i].type = ZONE_NONE;
        order[i - 1] = i;
    }
    for (j = count - 1; j > 0; j--)
    {
        k = RandRange(0, j);
        tmp = order[j];
        order[j] = order[k];
        order[k] = tmp;
    }

    for (k = 0; k < count; k++)
    {
        i = order[k];
        shortSide = Min(z[i].w, z[i].h);
        area = (s32)z[i].w * z[i].h;

        if (shortSide <= WORLDGEN_MAX_STRIP)
        {
            type = RandPercent(65) ? ZONE_ROUTE : PickWild();
        }
        else if (area <= 2000 && IsSquarish(&z[i], 20) && RandPercent(44))
        {
            type = RandPercent(70) ? ZONE_VILLAGE : ZONE_CASTLE;
        }
        else
        {
            type = ZONE_NONE;
            if (RandPercent(50))
                type = NeighbourWildType(z, n, i);
            if (type == ZONE_NONE)
                type = PickWild();
        }
        z[i].type = type;
    }

    for (i = 0; i < n; i++)
    {
        if (z[i].type == ZONE_CASTLE)
            castles++;
    }
    for (k = 0; k < count && castles < 2; k++)
    {
        i = order[k];
        if (IsWild(z[i].type) && Min(z[i].w, z[i].h) >= 28 && IsSquarish(&z[i], 20))
        {
            z[i].type = ZONE_CASTLE;
            castles++;
        }
    }
    for (k = 0; k < count && castles < 2; k++)
    {
        i = order[k];
        if (IsWild(z[i].type))
        {
            z[i].type = ZONE_CASTLE;
            castles++;
        }
    }

    for (k = 0; k < count; k++)
    {
        i = order[k];
        if (IsWild(z[i].type) && Min(z[i].w, z[i].h) > WORLDGEN_MAX_STRIP && (s32)z[i].w * z[i].h >= 1500
         && RandPercent(14) && StaysConnected(z, n, i))
            z[i].type = ZONE_LAKE;
    }
}

// ---------------------------------------------------------------------------
// 4. Links
// ---------------------------------------------------------------------------
static void BuildLinks(struct WorldLayout *l)
{
    const struct WorldZone *z = l->zones;
    struct WorldLink *link;
    u8 i;
    u8 j;
    u8 vertical;
    s32 pos;
    s32 start;
    s32 len;

    l->numLinks = 0;
    for (i = 0; i < l->numZones; i++)
    {
        for (j = i + 1; j < l->numZones; j++)
        {
            if (z[i].type == ZONE_LAKE || z[j].type == ZONE_LAKE)
                continue;
            len = SharedBorder(&z[i], &z[j], &vertical, &pos, &start);
            if (len < WORLDGEN_MIN_LINK || l->numLinks >= WORLDGEN_MAX_LINKS)
                continue;
            link = &l->links[l->numLinks++];
            if (vertical ? (z[i].x + z[i].w == pos) : (z[i].y + z[i].h == pos))
            {
                link->a = i;
                link->b = j;
            }
            else
            {
                link->a = j;
                link->b = i;
            }
            link->vertical = vertical;
            link->len = len;
            link->pos = pos;
            link->start = start;
        }
    }
}

void WorldGen_Generate(struct WorldLayout *l, u32 seed)
{
    u8 i;

    sRng = seed ^ 0xA5C3D2B1;
    if (sRng == 0)
        sRng = 1;
    for (i = 0; i < 8; i++)
        Rand32();

    l->seed = seed;
    l->numZones = Partition(l->zones);
    PruneToIsland(l->zones, &l->numZones);
    ChooseStart(l->zones, l->numZones);
    l->startZone = 0;
    l->pad = 0;
    AssignTypes(l->zones, l->numZones);
    BuildLinks(l);
}
