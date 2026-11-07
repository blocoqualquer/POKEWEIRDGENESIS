#include "worldgen.h"

// Plain C, no GBA dependencies: compiled both into the ROM (agbcc, C89 style
// declarations at the top of blocks) and into devtools/worldgen for testing.

#define CELLS (WORLDGEN_GRID * WORLDGEN_GRID)
#define INNER (WORLDGEN_CELL - 2 * WORLDGEN_MARGIN) // largest zone that fits one slot (58)

// ---------------------------------------------------------------------------
// Random numbers: xorshift32. Integer only, so it gives identical results on
// the GBA and on a PC.
// ---------------------------------------------------------------------------
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

// Inclusive range.
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

// ---------------------------------------------------------------------------
// Zone placement
// ---------------------------------------------------------------------------
struct TypeInfo
{
    u8 weight;  // chance of being picked for a free slot
    u8 minW;
    u8 maxW;
    u8 minH;
    u8 maxH;
    u8 canSpan; // may stretch over two slots as a long strip (a "route")
};

static const struct TypeInfo sTypeInfo[ZONE_TYPE_COUNT] =
{
    [ZONE_NONE]     = {  0,  0,  0,  0,  0, 0 },
    [ZONE_VILLAGE]  = { 10, 22, 28, 22, 28, 0 },
    [ZONE_CASTLE]   = {  8, 32, 44, 32, 44, 0 },
    [ZONE_FOREST]   = { 26, 30, 58, 24, 58, 1 },
    [ZONE_PLAINS]   = { 18, 32, 58, 28, 58, 1 },
    [ZONE_DESERT]   = { 10, 34, 58, 30, 58, 1 },
    [ZONE_MOUNTAIN] = { 12, 28, 52, 28, 56, 1 },
    [ZONE_SWAMP]    = {  8, 26, 46, 26, 46, 1 },
    [ZONE_LAKE]     = {  8, 24, 46, 24, 44, 0 },
};

#define EMPTY_SLOT_PERCENT 24
#define SPAN_PERCENT       30
#define STRIP_MIN_LEN      90
#define STRIP_MIN_THICK    16
#define STRIP_MAX_THICK    30

static u8 PickType(void)
{
    u32 roll = (Rand32() >> 8) % 100;
    u8 type;

    for (type = ZONE_VILLAGE; type < ZONE_TYPE_COUNT; type++)
    {
        if (roll < sTypeInfo[type].weight)
            return type;
        roll -= sTypeInfo[type].weight;
    }
    return ZONE_FOREST;
}

static void AddZone(struct WorldLayout *l, s32 x, s32 y, s32 w, s32 h, u8 type)
{
    struct WorldZone *z = &l->zones[l->numZones++];

    z->x = x;
    z->y = y;
    z->w = w;
    z->h = h;
    z->type = type;
    z->pad = 0;
}

// Places a zone of the given type in slot (cx, cy). May stretch it over the
// slot to the right or below when `slotUsed` says that one is still free.
static void PlaceZone(struct WorldLayout *l, u8 *slotUsed, u8 cx, u8 cy, u8 type)
{
    const struct TypeInfo *info = &sTypeInfo[type];
    s32 ox = cx * WORLDGEN_CELL + WORLDGEN_MARGIN;
    s32 oy = cy * WORLDGEN_CELL + WORLDGEN_MARGIN;
    s32 w;
    s32 h;
    u8 horizontal;

    if (info->canSpan && RandPercent(SPAN_PERCENT))
    {
        horizontal = Rand32() & 1;
        // try the chosen direction, then the other one
        if (horizontal && (cx + 1 >= WORLDGEN_GRID || slotUsed[cy * WORLDGEN_GRID + cx + 1]))
            horizontal = 0;
        if (!horizontal && (cy + 1 >= WORLDGEN_GRID || slotUsed[(cy + 1) * WORLDGEN_GRID + cx]))
            horizontal = 1;
        if (horizontal && (cx + 1 >= WORLDGEN_GRID || slotUsed[cy * WORLDGEN_GRID + cx + 1]))
            goto single; // boxed in: stay a normal zone

        if (horizontal)
        {
            slotUsed[cy * WORLDGEN_GRID + cx + 1] = 1;
            w = RandRange(STRIP_MIN_LEN, 2 * WORLDGEN_CELL - 2 * WORLDGEN_MARGIN); // up to 2 slots minus margins
            h = RandRange(STRIP_MIN_THICK, STRIP_MAX_THICK);
            AddZone(l, ox + RandRange(0, 2 * WORLDGEN_CELL - 2 * WORLDGEN_MARGIN - w),
                       oy + RandRange(0, INNER - h), w, h, type);
        }
        else
        {
            slotUsed[(cy + 1) * WORLDGEN_GRID + cx] = 1;
            h = RandRange(STRIP_MIN_LEN, 2 * WORLDGEN_CELL - 2 * WORLDGEN_MARGIN);
            w = RandRange(STRIP_MIN_THICK, STRIP_MAX_THICK);
            AddZone(l, ox + RandRange(0, INNER - w),
                       oy + RandRange(0, 2 * WORLDGEN_CELL - 2 * WORLDGEN_MARGIN - h), w, h, type);
        }
        return;
    }

single:
    w = Min(RandRange(info->minW, info->maxW), INNER);
    h = Min(RandRange(info->minH, info->maxH), INNER);
    AddZone(l, ox + RandRange(0, INNER - w), oy + RandRange(0, INNER - h), w, h, type);
}

static void PlaceZones(struct WorldLayout *l)
{
    u8 slotUsed[CELLS];
    u8 order[CELLS];
    s32 i;
    s32 j;
    u8 tmp;
    u8 startSlot;
    u8 castlesToPlace;
    u8 castleSlot;
    u8 type;

    for (i = 0; i < CELLS; i++)
    {
        slotUsed[i] = 0;
        order[i] = i;
    }

    // shuffle the visiting order (Fisher-Yates)
    for (i = CELLS - 1; i > 0; i--)
    {
        j = RandRange(0, i);
        tmp = order[i];
        order[i] = order[j];
        order[j] = tmp;
    }

    // The starting village sits somewhere in the central 4x4 slots.
    startSlot = RandRange(1, WORLDGEN_GRID - 2) * WORLDGEN_GRID + RandRange(1, WORLDGEN_GRID - 2);

    // The start village is placed first so it always gets the lowest index.
    slotUsed[startSlot] = 1;
    PlaceZone(l, slotUsed, startSlot % WORLDGEN_GRID, startSlot / WORLDGEN_GRID, ZONE_VILLAGE);
    l->startZone = 0;

    // Two castle towns are guaranteed. They are placed right away, before any
    // long strip zone could swallow their slot.
    for (castlesToPlace = 0; castlesToPlace < 2;)
    {
        castleSlot = RandRange(0, CELLS - 1);
        if (!slotUsed[castleSlot])
        {
            slotUsed[castleSlot] = 1;
            PlaceZone(l, slotUsed, castleSlot % WORLDGEN_GRID, castleSlot / WORLDGEN_GRID, ZONE_CASTLE);
            castlesToPlace++;
        }
    }

    for (i = 0; i < CELLS; i++)
    {
        u8 slot = order[i];

        if (slotUsed[slot])
            continue;
        slotUsed[slot] = 1;
        if (RandPercent(EMPTY_SLOT_PERCENT))
            continue; // empty slot: an open lane for routes
        type = PickType();
        PlaceZone(l, slotUsed, slot % WORLDGEN_GRID, slot / WORLDGEN_GRID, type);
    }
}

// ---------------------------------------------------------------------------
// Connections: a minimum spanning tree (everything is reachable) plus a few
// extra links so the map has loops instead of being a pure tree.
// ---------------------------------------------------------------------------
struct Edge
{
    u8 a;
    u8 b;
};

static s32 CenterX(const struct WorldZone *z) { return z->x + z->w / 2; }
static s32 CenterY(const struct WorldZone *z) { return z->y + z->h / 2; }

static s32 ZoneDistance(const struct WorldLayout *l, u8 a, u8 b)
{
    return Abs(CenterX(&l->zones[a]) - CenterX(&l->zones[b]))
         + Abs(CenterY(&l->zones[a]) - CenterY(&l->zones[b]));
}

static u8 HasEdge(const struct Edge *edges, u8 numEdges, u8 a, u8 b)
{
    u8 i;

    for (i = 0; i < numEdges; i++)
    {
        if ((edges[i].a == a && edges[i].b == b) || (edges[i].a == b && edges[i].b == a))
            return 1;
    }
    return 0;
}

static u8 BuildEdges(const struct WorldLayout *l, struct Edge *edges)
{
    u8 inTree[WORLDGEN_MAX_ZONES];
    u16 bestDist[WORLDGEN_MAX_ZONES];
    u8 bestFrom[WORLDGEN_MAX_ZONES];
    u8 degree[WORLDGEN_MAX_ZONES];
    u8 n = l->numZones;
    u8 numEdges = 0;
    u8 i;
    u8 j;
    u8 next;
    s32 d;
    s32 nearest;
    u8 nearestZone;

    for (i = 0; i < n; i++)
    {
        inTree[i] = 0;
        degree[i] = 0;
        bestDist[i] = 0xFFFF;
        bestFrom[i] = 0;
    }

    // Prim's algorithm, starting from the start village.
    inTree[0] = 1;
    for (i = 1; i < n; i++)
    {
        bestDist[i] = ZoneDistance(l, 0, i);
        bestFrom[i] = 0;
    }
    for (j = 1; j < n; j++)
    {
        next = 0xFF;
        for (i = 1; i < n; i++)
        {
            if (!inTree[i] && (next == 0xFF || bestDist[i] < bestDist[next]))
                next = i;
        }
        inTree[next] = 1;
        edges[numEdges].a = bestFrom[next];
        edges[numEdges].b = next;
        numEdges++;
        degree[bestFrom[next]]++;
        degree[next]++;
        for (i = 1; i < n; i++)
        {
            if (!inTree[i])
            {
                d = ZoneDistance(l, next, i);
                if (d < bestDist[i])
                {
                    bestDist[i] = d;
                    bestFrom[i] = next;
                }
            }
        }
    }

    // Extra links: each zone may get one more route to its nearest unlinked
    // neighbour, as long as that neighbour is reasonably close.
    for (i = 0; i < n && numEdges < WORLDGEN_MAX_ROUTES; i++)
    {
        nearest = 0x7FFF;
        nearestZone = 0xFF;
        for (j = 0; j < n; j++)
        {
            if (j == i || HasEdge(edges, numEdges, i, j))
                continue;
            d = ZoneDistance(l, i, j);
            if (d < nearest)
            {
                nearest = d;
                nearestZone = j;
            }
        }
        if (nearestZone != 0xFF && nearest <= (WORLDGEN_CELL * 17) / 10
         && degree[i] < 4 && degree[nearestZone] < 4 && RandPercent(40))
        {
            edges[numEdges].a = i;
            edges[numEdges].b = nearestZone;
            numEdges++;
            degree[i]++;
            degree[nearestZone]++;
        }
    }
    return numEdges;
}

// ---------------------------------------------------------------------------
// Route routing. Each route leaves a zone from the side facing the other one and
// runs as a straight line or a Z shape. Candidates that cut through a village,
// castle or lake are penalised, so routes detour along the free lanes between
// slots whenever possible.
// ---------------------------------------------------------------------------
struct Rect
{
    s16 x0;
    s16 y0;
    s16 x1; // exclusive
    s16 y1; // exclusive
};

// With swap set, x and y are exchanged so one routine handles both directions.
static void ZoneRect(const struct WorldZone *z, u8 swap, struct Rect *r)
{
    if (!swap)
    {
        r->x0 = z->x;
        r->y0 = z->y;
        r->x1 = z->x + z->w;
        r->y1 = z->y + z->h;
    }
    else
    {
        r->x0 = z->y;
        r->y0 = z->x;
        r->x1 = z->y + z->h;
        r->y1 = z->x + z->w;
    }
}

static u8 IsBlocker(u8 type)
{
    return type == ZONE_VILLAGE || type == ZONE_CASTLE || type == ZONE_LAKE;
}

#define ROUTE_ATTEMPTS    4
#define MAX_LANES         5
#define BLOCKED_COST      1000 // cutting through a village, castle or lake
#define WILD_OVERLAP_COST 60   // cutting through a forest, desert, ...

// Cost of a corridor segment (axis-aligned, `halfWidth` tiles to each side of
// its centre line) for the zones it would overlap, other than the two it joins.
// ext0/ext1: the (x0,y0) / (x1,y1) end is an inner corner, so the corridor
// extends halfWidth further to cover it. Ends at a zone border do not extend.
static s32 SegmentCost(const struct WorldLayout *l, u8 swap, u8 skipA, u8 skipB, s32 halfWidth,
                       s32 x0, s32 y0, s32 x1, s32 y1, u8 ext0, u8 ext1)
{
    s32 minX;
    s32 maxX;
    s32 minY;
    s32 maxY;
    s32 cost = 0;
    struct Rect r;
    u8 i;

    if (y0 == y1) // horizontal
    {
        minY = y0 - halfWidth;
        maxY = y0 + halfWidth;
        if (x0 <= x1)
        {
            minX = x0 - (ext0 ? halfWidth : 0);
            maxX = x1 + (ext1 ? halfWidth : 0);
        }
        else
        {
            minX = x1 - (ext1 ? halfWidth : 0);
            maxX = x0 + (ext0 ? halfWidth : 0);
        }
    }
    else // vertical
    {
        minX = x0 - halfWidth;
        maxX = x0 + halfWidth;
        if (y0 <= y1)
        {
            minY = y0 - (ext0 ? halfWidth : 0);
            maxY = y1 + (ext1 ? halfWidth : 0);
        }
        else
        {
            minY = y1 - (ext1 ? halfWidth : 0);
            maxY = y0 + (ext0 ? halfWidth : 0);
        }
    }

    for (i = 0; i < l->numZones; i++)
    {
        if (i == skipA || i == skipB)
            continue;
        ZoneRect(&l->zones[i], swap, &r);
        if (r.x0 <= maxX && r.x1 > minX && r.y0 <= maxY && r.y1 > minY)
            cost += IsBlocker(l->zones[i].type) ? BLOCKED_COST : WILD_OVERLAP_COST;
    }
    return cost;
}

static void BuildRoute(struct WorldLayout *l, u8 a, u8 b, struct WorldRoute *route)
{
    const struct WorldZone *za = &l->zones[a];
    const struct WorldZone *zb = &l->zones[b];
    struct Rect ra;
    struct Rect rb;
    struct Rect tmpRect;
    u8 swap;
    u8 skipA = a;
    u8 skipB = b;
    s32 gapX;
    s32 gapY;
    s32 aX;
    s32 bX;
    s32 loA, hiA, loB, hiB, loC, hiC;
    s32 halfWidth;
    s32 laneLo;
    s32 laneHi;
    s32 bestScore = 0x7FFFFFFF;
    s32 best[4][2];
    s32 bestPoints = 2;
    s32 attempt;
    s32 ay;
    s32 by;
    s32 lanes[MAX_LANES];
    s32 numLanes;
    s32 lane;
    s32 k;
    s32 score;
    s32 len;
    s32 cost;

    halfWidth = RandRange(WORLDGEN_ROUTE_MIN_HALF_WIDTH, WORLDGEN_ROUTE_MAX_HALF_WIDTH);

    gapX = Max(zb->x - (za->x + za->w), za->x - (zb->x + zb->w));
    gapY = Max(zb->y - (za->y + za->h), za->y - (zb->y + zb->h));
    swap = gapY > gapX;

    ZoneRect(za, swap, &ra);
    ZoneRect(zb, swap, &rb);
    if (ra.x0 > rb.x0) // make "ra" the zone on the near (low) side
    {
        tmpRect = ra;
        ra = rb;
        rb = tmpRect;
    }

    aX = ra.x1;     // just outside the right edge of the near zone
    bX = rb.x0 - 1; // just outside the left edge of the far zone
    if (bX < aX)
        bX = aX;

    // The corridor must fit entirely inside the side of the zone it leaves from.
    loA = ra.y0 + halfWidth;
    hiA = ra.y1 - 1 - halfWidth;
    if (hiA < loA)
        loA = hiA = (ra.y0 + ra.y1) / 2;
    loB = rb.y0 + halfWidth;
    hiB = rb.y1 - 1 - halfWidth;
    if (hiB < loB)
        loB = hiB = (rb.y0 + rb.y1) / 2;
    loC = Max(loA, loB);
    hiC = Min(hiA, hiB);

    // The sideways jog must fit between the two zones too.
    laneLo = aX + halfWidth;
    laneHi = bX - halfWidth;
    if (laneLo > laneHi)
        laneLo = laneHi = (aX + bX) / 2;

    for (attempt = 0; attempt < ROUTE_ATTEMPTS; attempt++)
    {
        // The first attempts try a perfectly straight route when the zones overlap in y.
        if (loC <= hiC && attempt < 2)
        {
            ay = RandRange(loC, hiC);
            by = ay;
        }
        else
        {
            ay = RandRange(loA, hiA);
            by = RandRange(loB, hiB);
        }

        numLanes = 0;
        if (ay == by)
        {
            lanes[numLanes++] = aX; // unused for straight routes
        }
        else
        {
            lanes[numLanes++] = (laneLo + laneHi) / 2;
            // the free lanes between slots, where no zone ever stands
            for (lane = ((laneLo + WORLDGEN_CELL - 1) / WORLDGEN_CELL) * WORLDGEN_CELL; lane <= laneHi && numLanes < MAX_LANES - 1; lane += WORLDGEN_CELL)
                lanes[numLanes++] = lane;
            lanes[numLanes++] = RandRange(laneLo, laneHi);
        }

        for (k = 0; k < numLanes; k++)
        {
            lane = lanes[k];
            if (ay == by)
            {
                cost = SegmentCost(l, swap, skipA, skipB, halfWidth, aX, ay, bX, by, 0, 0);
                len = bX - aX;
            }
            else
            {
                cost = SegmentCost(l, swap, skipA, skipB, halfWidth, aX, ay, lane, ay, 0, 1)
                     + SegmentCost(l, swap, skipA, skipB, halfWidth, lane, ay, lane, by, 1, 1)
                     + SegmentCost(l, swap, skipA, skipB, halfWidth, lane, by, bX, by, 1, 0);
                len = Abs(lane - aX) + Abs(ay - by) + Abs(bX - lane);
            }
            score = len + cost;
            if (score < bestScore)
            {
                bestScore = score;
                if (ay == by)
                {
                    bestPoints = 2;
                    best[0][0] = aX; best[0][1] = ay;
                    best[1][0] = bX; best[1][1] = by;
                }
                else
                {
                    bestPoints = 4;
                    best[0][0] = aX;   best[0][1] = ay;
                    best[1][0] = lane; best[1][1] = ay;
                    best[2][0] = lane; best[2][1] = by;
                    best[3][0] = bX;   best[3][1] = by;
                }
            }
        }
    }

    route->from = a;
    route->to = b;
    route->numPoints = bestPoints;
    route->halfWidth = halfWidth;
    for (k = 0; k < 4; k++)
    {
        if (k < bestPoints)
        {
            route->x[k] = swap ? best[k][1] : best[k][0];
            route->y[k] = swap ? best[k][0] : best[k][1];
        }
        else
        {
            route->x[k] = 0;
            route->y[k] = 0;
        }
    }
}

// ---------------------------------------------------------------------------
void WorldGen_Generate(struct WorldLayout *l, u32 seed)
{
    struct Edge edges[WORLDGEN_MAX_ROUTES];
    u8 numEdges;
    u8 i;

    sRng = seed ^ 0xA5C3D2B1;
    if (sRng == 0)
        sRng = 1;
    for (i = 0; i < 8; i++) // warm up: nearby seeds should not look alike
        Rand32();

    l->seed = seed;
    l->numZones = 0;
    l->numRoutes = 0;
    l->startZone = 0;
    l->pad = 0;

    PlaceZones(l);
    numEdges = BuildEdges(l, edges);
    for (i = 0; i < numEdges; i++)
        BuildRoute(l, edges[i].a, edges[i].b, &l->routes[l->numRoutes++]);
}
