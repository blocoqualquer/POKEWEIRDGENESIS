// PC-side tool for the world layout generator. It compiles the very same
// src/worldgen.c that goes into the ROM.
//
//   worldgen_tool test <numSeeds>                   check invariants over many seeds
//   worldgen_tool render <out.ppm> <scale> <seed>   draw one world
//
// Build: see devtools/worldgen/README.md
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "worldgen.h"

static int Overlap(const struct WorldZone *a, const struct WorldZone *b)
{
    return a->x < b->x + b->w && b->x < a->x + a->w && a->y < b->y + b->h && b->y < a->y + a->h;
}

static int IsLinkedConnected(const struct WorldLayout *l)
{
    int reached[WORLDGEN_MAX_ZONES] = {0};
    int queue[WORLDGEN_MAX_ZONES];
    int head = 0, tail = 0, total = 0, i, k;

    for (i = 0; i < l->numZones; i++)
        if (l->zones[i].type != ZONE_LAKE)
            total++;
    reached[0] = 1;
    queue[tail++] = 0;
    while (head < tail)
    {
        i = queue[head++];
        for (k = 0; k < l->numLinks; k++)
        {
            int o = -1;
            if (l->links[k].a == i) o = l->links[k].b;
            else if (l->links[k].b == i) o = l->links[k].a;
            if (o >= 0 && !reached[o])
            {
                reached[o] = 1;
                queue[tail++] = o;
            }
        }
    }
    return tail == total;
}

static int CheckLayout(const struct WorldLayout *l, unsigned seed)
{
    int i, j, k, castles = 0;

    if (l->numZones < WORLDGEN_MIN_ZONES || l->numZones > WORLDGEN_MAX_ZONES)
        return printf("seed %u: bad zone count %d\n", seed, l->numZones), 1;
    if (l->zones[0].type != ZONE_VILLAGE || l->startZone != 0)
        return printf("seed %u: zone 0 is not the start village\n", seed), 1;
    for (i = 0; i < l->numZones; i++)
    {
        const struct WorldZone *z = &l->zones[i];
        int mn = z->w < z->h ? z->w : z->h;
        int mx = z->w > z->h ? z->w : z->h;

        if (z->x + z->w > WORLDGEN_SIZE || z->y + z->h > WORLDGEN_SIZE)
            return printf("seed %u: zone %d outside the world\n", seed, i), 1;
        if (mn < WORLDGEN_MIN_STRIP || mx > WORLDGEN_MAX_SIDE || z->w * z->h > WORLDGEN_MAX_AREA)
            return printf("seed %u: zone %d bad size %dx%d\n", seed, i, z->w, z->h), 1;
        if (z->type == ZONE_NONE || z->type >= ZONE_TYPE_COUNT)
            return printf("seed %u: zone %d has no type\n", seed, i), 1;
        if (z->type == ZONE_CASTLE)
            castles++;
        for (j = i + 1; j < l->numZones; j++)
            if (Overlap(z, &l->zones[j]))
                return printf("seed %u: zones %d and %d overlap\n", seed, i, j), 1;
    }
    if (castles < 2)
        return printf("seed %u: only %d castles\n", seed, castles), 1;
    for (k = 0; k < l->numLinks; k++)
    {
        const struct WorldLink *lk = &l->links[k];
        const struct WorldZone *a = &l->zones[lk->a], *b = &l->zones[lk->b];
        int lo, hi;

        if (a->type == ZONE_LAKE || b->type == ZONE_LAKE)
            return printf("seed %u: link touches a lake\n", seed), 1;
        if (lk->len < WORLDGEN_MIN_LINK)
            return printf("seed %u: link %d too short\n", seed, k), 1;
        if (lk->vertical)
        {
            lo = a->y > b->y ? a->y : b->y;
            hi = (a->y + a->h) < (b->y + b->h) ? (a->y + a->h) : (b->y + b->h);
            if (a->x + a->w != lk->pos || b->x != lk->pos)
                return printf("seed %u: link %d wrong vertical border\n", seed, k), 1;
        }
        else
        {
            lo = a->x > b->x ? a->x : b->x;
            hi = (a->x + a->w) < (b->x + b->w) ? (a->x + a->w) : (b->x + b->w);
            if (a->y + a->h != lk->pos || b->y != lk->pos)
                return printf("seed %u: link %d wrong horizontal border\n", seed, k), 1;
        }
        if (lo != lk->start || hi - lo != lk->len)
            return printf("seed %u: link %d wrong extent\n", seed, k), 1;
    }
    if (!IsLinkedConnected(l))
        return printf("seed %u: land not connected through links\n", seed), 1;
    return 0;
}

static int SameLayout(const struct WorldLayout *a, const struct WorldLayout *b)
{
    return a->seed == b->seed && a->numZones == b->numZones && a->numLinks == b->numLinks
        && memcmp(a->zones, b->zones, a->numZones * sizeof(a->zones[0])) == 0
        && memcmp(a->links, b->links, a->numLinks * sizeof(a->links[0])) == 0;
}

static int RunTest(unsigned numSeeds)
{
    static struct WorldLayout l, l2;
    unsigned s, failures = 0;
    double zones = 0, coverage = 0, links = 0;
    unsigned minZ = 999, maxZ = 0, types[ZONE_TYPE_COUNT] = {0};
    int i;

    for (s = 1; s <= numSeeds; s++)
    {
        unsigned seed = s * 2654435761u + 12345;
        long area = 0;

        memset(&l, 0xAA, sizeof(l));
        memset(&l2, 0x55, sizeof(l2));
        WorldGen_Generate(&l, seed);
        WorldGen_Generate(&l2, seed);
        if (!SameLayout(&l, &l2))
        {
            printf("seed %u: not deterministic\n", seed);
            failures++;
            continue;
        }
        if (CheckLayout(&l, seed))
        {
            failures++;
            continue;
        }
        for (i = 0; i < l.numZones; i++)
        {
            area += l.zones[i].w * l.zones[i].h;
            types[l.zones[i].type]++;
        }
        zones += l.numZones;
        links += l.numLinks;
        coverage += area * 100.0 / (WORLDGEN_SIZE * WORLDGEN_SIZE);
        if (l.numZones < minZ) minZ = l.numZones;
        if (l.numZones > maxZ) maxZ = l.numZones;
    }
    printf("seeds: %u, failures: %u\n", numSeeds, failures);
    if (numSeeds > failures)
    {
        unsigned ok = numSeeds - failures;
        printf("zones per world: avg %.1f (min %u, max %u)\n", zones / ok, minZ, maxZ);
        printf("land coverage: avg %.1f%%\n", coverage / ok);
        printf("links per zone: avg %.2f\n", links / zones);
        printf("types per world: village %.1f castle %.1f forest %.1f plains %.1f desert %.1f mountain %.1f swamp %.1f lake %.1f route %.1f\n",
               types[ZONE_VILLAGE] / (double)ok, types[ZONE_CASTLE] / (double)ok, types[ZONE_FOREST] / (double)ok,
               types[ZONE_PLAINS] / (double)ok, types[ZONE_DESERT] / (double)ok, types[ZONE_MOUNTAIN] / (double)ok,
               types[ZONE_SWAMP] / (double)ok, types[ZONE_LAKE] / (double)ok, types[ZONE_ROUTE] / (double)ok);
    }
    return failures != 0;
}

// ---------------------------------------------------------------------------
static const unsigned char sColors[ZONE_TYPE_COUNT][3] = {
    [ZONE_NONE]     = {0, 0, 0},
    [ZONE_VILLAGE]  = {150, 205, 110},
    [ZONE_CASTLE]   = {168, 166, 176},
    [ZONE_FOREST]   = {70, 160, 140},
    [ZONE_PLAINS]   = {124, 200, 90},
    [ZONE_DESERT]   = {226, 196, 128},
    [ZONE_MOUNTAIN] = {140, 108, 84},
    [ZONE_SWAMP]    = {92, 122, 80},
    [ZONE_LAKE]     = {70, 120, 210},
    [ZONE_ROUTE]    = {150, 190, 100},
};

static unsigned char *sImg;
static int sW;

static void Put(int x, int y, const unsigned char *c)
{
    if (x < 0 || y < 0 || x >= sW || y >= sW)
        return;
    memcpy(sImg + (y * sW + x) * 3, c, 3);
}

static void Rect(int x0, int y0, int x1, int y1, const unsigned char *c)
{
    int x, y;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++)
            Put(x, y, c);
}

static int RenderWorld(const char *path, int scale, unsigned seed)
{
    static struct WorldLayout l;
    static const unsigned char sea[3] = {22, 32, 58}, edge[3] = {20, 30, 20}, path_[3] = {206, 180, 120};
    static const unsigned char gold[3] = {250, 220, 60}, red[3] = {220, 40, 40}, dark[3] = {60, 60, 70}, linkc[3] = {255, 255, 255};
    int i, k, n = WORLDGEN_SIZE * scale;
    FILE *f;

    WorldGen_Generate(&l, seed);
    sW = n;
    sImg = malloc(n * n * 3);
    for (i = 0; i < n * n; i++)
        memcpy(sImg + i * 3, sea, 3);

    for (i = 0; i < l.numZones; i++)
    {
        const struct WorldZone *z = &l.zones[i];
        int x0 = z->x * scale, y0 = z->y * scale, x1 = (z->x + z->w) * scale, y1 = (z->y + z->h) * scale;

        Rect(x0, y0, x1, y1, sColors[z->type]);
        if (z->type == ZONE_ROUTE)
        {
            int t = scale > 1 ? scale : 1;
            if (z->w >= z->h)
                Rect(x0, (y0 + y1) / 2 - t, x1, (y0 + y1) / 2 + t, path_);
            else
                Rect((x0 + x1) / 2 - t, y0, (x0 + x1) / 2 + t, y1, path_);
        }
        else if (z->type == ZONE_CASTLE)
        {
            int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, r = 4 * scale;
            Rect(cx - r, cy - r, cx + r, cy + r, dark);
            Rect(cx - r / 2, cy - r / 2, cx + r / 2, cy + r / 2, sColors[ZONE_CASTLE]);
        }
        else if (z->type == ZONE_VILLAGE)
        {
            int cx = (x0 + x1) / 2, cy = (y0 + y1) / 2, r = 3 * scale;
            Rect(cx - r, cy - r, cx + r, cy + r, (const unsigned char[3]){120, 80, 50});
        }
        // thin dark border so zones read as separate rectangles
        Rect(x0, y0, x1, y0 + 1, edge);
        Rect(x0, y1 - 1, x1, y1, edge);
        Rect(x0, y0, x0 + 1, y1, edge);
        Rect(x1 - 1, y0, x1, y1, edge);
    }
    // start marker
    {
        const struct WorldZone *z = &l.zones[0];
        int cx = (z->x + z->w / 2) * scale, cy = (z->y + z->h / 2) * scale, r = 6 * scale;
        Rect(cx - r, cy - r, cx + r, cy + r, gold);
        Rect(cx - r / 2, cy - r / 2, cx + r / 2, cy + r / 2, red);
    }
    // links: short white tick across each shared border
    for (k = 0; k < l.numLinks; k++)
    {
        const struct WorldLink *lk = &l.links[k];
        int mid = (lk->start + lk->len / 2) * scale, p = lk->pos * scale, t = scale > 1 ? scale : 1;

        if (lk->vertical)
            Rect(p - t, mid - 2 * t, p + t, mid + 2 * t, linkc);
        else
            Rect(mid - 2 * t, p - t, mid + 2 * t, p + t, linkc);
    }

    f = fopen(path, "wb");
    if (!f)
        return 1;
    fprintf(f, "P6\n%d %d\n255\n", n, n);
    fwrite(sImg, 3, n * n, f);
    fclose(f);
    free(sImg);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 3 && strcmp(argv[1], "test") == 0)
        return RunTest(atoi(argv[2]));
    if (argc >= 5 && strcmp(argv[1], "render") == 0)
        return RenderWorld(argv[2], atoi(argv[3]), (unsigned)strtoul(argv[4], NULL, 0));
    fprintf(stderr, "usage: %s test <numSeeds> | render <out.ppm> <scale> <seed>\n", argv[0]);
    return 2;
}
