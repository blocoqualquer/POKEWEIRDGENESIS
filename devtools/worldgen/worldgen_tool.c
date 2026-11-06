// PC-side tool for the world layout generator. It compiles the very same
// src/worldgen.c that goes into the ROM.
//
//   worldgen_tool test <numSeeds>            check invariants over many seeds
//   worldgen_tool render <out.ppm> <scale> <seed>   draw one world
//
// Build: see devtools/worldgen/README.md
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "worldgen.h"

#define CELLS (WORLDGEN_GRID * WORLDGEN_GRID)

static int Overlap(const struct WorldZone *a, const struct WorldZone *b)
{
    return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

static int Find(int *parent, int i)
{
    while (parent[i] != i)
        i = parent[i] = parent[parent[i]];
    return i;
}

static int PointInZone(const struct WorldZone *z, int x, int y, int grow)
{
    return x >= z->x - grow && x < z->x + z->w + grow && y >= z->y - grow && y < z->y + z->h + grow;
}

static int SegmentHitsBlocker(const struct WorldLayout *l, const struct WorldRoad *r, int seg)
{
    int x0 = r->x[seg], y0 = r->y[seg], x1 = r->x[seg + 1], y1 = r->y[seg + 1];
    int minX = (x0 < x1 ? x0 : x1) - 1, maxX = (x0 > x1 ? x0 : x1) + 1;
    int minY = (y0 < y1 ? y0 : y1) - 1, maxY = (y0 > y1 ? y0 : y1) + 1;
    int i;

    for (i = 0; i < l->numZones; i++)
    {
        const struct WorldZone *z = &l->zones[i];
        if (i == r->from || i == r->to)
            continue;
        if (z->type != ZONE_VILLAGE && z->type != ZONE_CASTLE && z->type != ZONE_LAKE)
            continue;
        if (z->x <= maxX && z->x + z->w > minX && z->y <= maxY && z->y + z->h > minY)
            return 1;
    }
    return 0;
}

static int Check(const struct WorldLayout *l, unsigned seed, int *blockedRoads)
{
    int parent[WORLDGEN_MAX_ZONES];
    int i, j, k, castles = 0;

    *blockedRoads = 0;
    if (l->numZones < WORLDGEN_MIN_ZONES || l->numZones > WORLDGEN_MAX_ZONES)
        return printf("seed %u: bad zone count %d\n", seed, l->numZones), 1;
    if (l->zones[0].type != ZONE_VILLAGE || l->startZone != 0)
        return printf("seed %u: zone 0 is not the start village\n", seed), 1;
    for (i = 0; i < l->numZones; i++)
    {
        const struct WorldZone *z = &l->zones[i];
        if (z->type == ZONE_CASTLE)
            castles++;
        if (z->type == ZONE_NONE || z->type >= ZONE_TYPE_COUNT)
            return printf("seed %u: zone %d bad type\n", seed, i), 1;
        if (z->w < 12 || z->h < 12)
            return printf("seed %u: zone %d too small (%dx%d)\n", seed, i, z->w, z->h), 1;
        if (z->x < 1 || z->y < 1 || z->x + z->w >= WORLDGEN_SIZE || z->y + z->h >= WORLDGEN_SIZE)
            return printf("seed %u: zone %d out of bounds\n", seed, i), 1;
        for (j = i + 1; j < l->numZones; j++)
            if (Overlap(z, &l->zones[j]))
                return printf("seed %u: zones %d and %d overlap\n", seed, i, j), 1;
    }
    if (castles < 2)
        return printf("seed %u: only %d castles\n", seed, castles), 1;
    if (l->numRoads < l->numZones - 1)
        return printf("seed %u: too few roads\n", seed), 1;

    for (i = 0; i < l->numZones; i++)
        parent[i] = i;
    for (i = 0; i < l->numRoads; i++)
    {
        const struct WorldRoad *r = &l->roads[i];
        int blocked = 0;

        if (r->numPoints < 2 || r->numPoints > 4)
            return printf("seed %u: road %d bad point count\n", seed, i), 1;
        for (k = 0; k < r->numPoints; k++)
            if (r->x[k] < 2 || r->y[k] < 2 || r->x[k] >= WORLDGEN_SIZE - 2 || r->y[k] >= WORLDGEN_SIZE - 2)
                return printf("seed %u: road %d point %d out of bounds\n", seed, i, k), 1;
        for (k = 0; k + 1 < r->numPoints; k++)
        {
            if (r->x[k] != r->x[k + 1] && r->y[k] != r->y[k + 1])
                return printf("seed %u: road %d has a diagonal segment\n", seed, i), 1;
            blocked |= SegmentHitsBlocker(l, r, k);
        }
        // endpoints must touch the zones they join
        if (!PointInZone(&l->zones[r->from], r->x[0], r->y[0], 1) && !PointInZone(&l->zones[r->from], r->x[r->numPoints - 1], r->y[r->numPoints - 1], 1))
            return printf("seed %u: road %d does not touch zone %d\n", seed, i, r->from), 1;
        if (!PointInZone(&l->zones[r->to], r->x[0], r->y[0], 1) && !PointInZone(&l->zones[r->to], r->x[r->numPoints - 1], r->y[r->numPoints - 1], 1))
            return printf("seed %u: road %d does not touch zone %d\n", seed, i, r->to), 1;
        *blockedRoads += blocked;
        parent[Find(parent, r->from)] = Find(parent, r->to);
    }
    for (i = 1; i < l->numZones; i++)
        if (Find(parent, i) != Find(parent, 0))
            return printf("seed %u: zone %d is not connected\n", seed, i), 1;
    return 0;
}

static int Test(int numSeeds)
{
    static struct WorldLayout a, b;
    int minZ = 99, maxZ = 0, minR = 99, maxR = 0, failures = 0, blockedTotal = 0, roadsTotal = 0;
    long zonesTotal = 0;
    int typeCount[ZONE_TYPE_COUNT] = {0};
    int s, i;

    for (s = 0; s < numSeeds; s++)
    {
        unsigned seed = s * 2654435761u + 12345u;
        int blocked;

        WorldGen_Generate(&a, seed);
        WorldGen_Generate(&b, seed);
        if (a.numZones != b.numZones || a.numRoads != b.numRoads
         || memcmp(a.zones, b.zones, a.numZones * sizeof(a.zones[0])) != 0
         || memcmp(a.roads, b.roads, a.numRoads * sizeof(a.roads[0])) != 0)
        {
            printf("seed %u: NOT deterministic\n", seed);
            failures++;
            continue;
        }
        if (Check(&a, seed, &blocked))
        {
            if (++failures >= 10)
                break;
            continue;
        }
        if (a.numZones < minZ) minZ = a.numZones;
        if (a.numZones > maxZ) maxZ = a.numZones;
        if (a.numRoads < minR) minR = a.numRoads;
        if (a.numRoads > maxR) maxR = a.numRoads;
        zonesTotal += a.numZones;
        roadsTotal += a.numRoads;
        blockedTotal += blocked;
        for (i = 0; i < a.numZones; i++)
            typeCount[a.zones[i].type]++;
    }
    printf("seeds tested: %d, failures: %d\n", numSeeds, failures);
    if (numSeeds > failures)
    {
        static const char *names[] = {"-", "village", "castle", "forest", "plains", "desert", "mountain", "swamp", "lake"};
        printf("zones per world: min %d, avg %.1f, max %d\n", minZ, (double)zonesTotal / (numSeeds - failures), maxZ);
        printf("roads per world: min %d, max %d\n", minR, maxR);
        printf("roads cutting through a village/castle/lake: %d of %d (%.2f%%)\n", blockedTotal, roadsTotal, 100.0 * blockedTotal / roadsTotal);
        for (i = 1; i < ZONE_TYPE_COUNT; i++)
            printf("  %-9s %.2f per world\n", names[i], (double)typeCount[i] / (numSeeds - failures));
    }
    return failures != 0;
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------
static unsigned char *sImg;
static int sW;

static void Put(int x, int y, int r, int g, int b)
{
    unsigned char *p;
    if (x < 0 || y < 0 || x >= sW || y >= sW)
        return;
    p = sImg + (y * sW + x) * 3;
    p[0] = r; p[1] = g; p[2] = b;
}

static unsigned Hash(unsigned x, unsigned y, unsigned salt)
{
    unsigned h = x * 374761393u + y * 668265263u + salt * 2147483647u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static void Tile(int tx, int ty, int scale, int r, int g, int b)
{
    int x, y;
    for (y = 0; y < scale; y++)
        for (x = 0; x < scale; x++)
            Put(tx * scale + x, ty * scale + y, r, g, b);
}

static void DrawZone(const struct WorldZone *z, int scale, int isStart)
{
    int x, y;

    for (y = z->y; y < z->y + z->h; y++)
    {
        for (x = z->x; x < z->x + z->w; x++)
        {
            unsigned h = Hash(x, y, z->type) & 255;
            int edge = x == z->x || y == z->y || x == z->x + z->w - 1 || y == z->y + z->h - 1;
            int r = 0, g = 0, b = 0;

            switch (z->type)
            {
            case ZONE_FOREST:   r = 70;  g = 160; b = 140; if (h < 70) { r = 30; g = 95; b = 45; } break;
            case ZONE_PLAINS:   r = 124; g = 200; b = 90;  if (h < 30) { r = 100; g = 180; b = 75; } break;
            case ZONE_DESERT:   r = 226; g = 196; b = 128; if (h < 25) { r = 190; g = 150; b = 90; } break;
            case ZONE_MOUNTAIN: r = 140; g = 108; b = 84;  if (h < 60) { r = 190; g = 160; b = 130; } break;
            case ZONE_SWAMP:    r = 92;  g = 122; b = 80;  if (h < 50) { r = 70; g = 110; b = 130; } break;
            case ZONE_LAKE:     r = 70;  g = 120; b = 210; if (h < 20) { r = 110; g = 160; b = 235; } break;
            case ZONE_VILLAGE:
            case ZONE_CASTLE:
            {
                int bx = (x - z->x) % 6, by = (y - z->y) % 6;
                int inner = x > z->x + 1 && y > z->y + 1 && x < z->x + z->w - 2 && y < z->y + z->h - 2;
                int house = inner && bx >= 1 && bx <= 3 && by >= 1 && by <= 3 && (Hash((x - z->x) / 6, (y - z->y) / 6, 7) & 3) != 0;

                if (z->type == ZONE_VILLAGE) { r = 150; g = 205; b = 110; }
                else { r = 168; g = 166; b = 176; }
                if (house) { if (z->type == ZONE_VILLAGE) { r = 190; g = 90; b = 70; } else { r = 96; g = 96; b = 120; } }
                break;
            }
            }
            if (edge) { r = r * 6 / 10; g = g * 6 / 10; b = b * 6 / 10; }
            Tile(x, y, scale, r, g, b);
        }
    }
    if (z->type == ZONE_CASTLE) // the keep
    {
        int cx = z->x + z->w / 2, cy = z->y + z->h / 2, x, y;
        for (y = cy - 3; y <= cy + 3; y++)
            for (x = cx - 3; x <= cx + 3; x++)
                Tile(x, y, scale, 70, 60, 110);
    }
    if (isStart) // yellow marker with a red core
    {
        int cx = z->x + z->w / 2, cy = z->y + z->h / 2, x, y;
        for (y = cy - 3; y <= cy + 3; y++)
            for (x = cx - 3; x <= cx + 3; x++)
                Tile(x, y, scale, 255, 220, 40);
        for (y = cy - 1; y <= cy + 1; y++)
            for (x = cx - 1; x <= cx + 1; x++)
                Tile(x, y, scale, 220, 40, 40);
    }
}

static void DrawRoad(const struct WorldRoad *r, int scale)
{
    int k, x, y;

    for (k = 0; k + 1 < r->numPoints; k++)
    {
        int x0 = r->x[k], y0 = r->y[k], x1 = r->x[k + 1], y1 = r->y[k + 1];
        int minX = (x0 < x1 ? x0 : x1) - 1, maxX = (x0 > x1 ? x0 : x1) + 1;
        int minY = (y0 < y1 ? y0 : y1) - 1, maxY = (y0 > y1 ? y0 : y1) + 1;
        for (y = minY; y <= maxY; y++)
            for (x = minX; x <= maxX; x++)
                Tile(x, y, scale, 215, 185, 130);
    }
}

static int Render(const char *path, int scale, unsigned seed)
{
    static struct WorldLayout l;
    FILE *f;
    int i, x, y;

    WorldGen_Generate(&l, seed);
    sW = WORLDGEN_SIZE * scale;
    sImg = malloc(sW * sW * 3);
    for (y = 0; y < sW; y++)
        for (x = 0; x < sW; x++)
            Put(x, y, 22, 32, 58); // void: not playable
    // roads first, then zones, so a road never paints over a zone
    for (i = 0; i < l.numRoads; i++)
        DrawRoad(&l.roads[i], scale);
    for (i = 0; i < l.numZones; i++)
        if (l.zones[i].type != ZONE_FOREST && l.zones[i].type != ZONE_PLAINS && l.zones[i].type != ZONE_DESERT
         && l.zones[i].type != ZONE_MOUNTAIN && l.zones[i].type != ZONE_SWAMP)
            continue;
        else
            DrawZone(&l.zones[i], scale, 0);
    for (i = 0; i < l.numRoads; i++) // roads over wild zones, but under towns and lakes
        DrawRoad(&l.roads[i], scale);
    for (i = 0; i < l.numZones; i++)
        if (l.zones[i].type == ZONE_VILLAGE || l.zones[i].type == ZONE_CASTLE || l.zones[i].type == ZONE_LAKE)
            DrawZone(&l.zones[i], scale, 0);
    DrawZone(&l.zones[l.startZone], scale, 1);

    f = fopen(path, "wb");
    fprintf(f, "P6\n%d %d\n255\n", sW, sW);
    fwrite(sImg, 1, sW * sW * 3, f);
    fclose(f);
    free(sImg);
    printf("seed %u: %d zones, %d roads\n", seed, l.numZones, l.numRoads);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[1], "test") == 0)
        return Test(atoi(argv[2]));
    if (argc >= 5 && strcmp(argv[1], "render") == 0)
        return Render(argv[2], atoi(argv[3]), (unsigned)strtoul(argv[4], NULL, 0));
    fprintf(stderr, "usage: %s test <numSeeds> | render <out.ppm> <scale> <seed>\n", argv[0]);
    return 2;
}
