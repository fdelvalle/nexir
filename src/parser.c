#include "nxg.h"
#include <string.h>

enum { HEADER = 12, SCENE = 4, ENTITY = 18, RULE = 10,
       MAX_FILE = HEADER + NX_SCENES * SCENE + NX_ENTITIES * ENTITY + NX_RULES * RULE };

static unsigned u16(const uint8_t *p)
{
    return p[0] | ((unsigned)p[1] << 8);
}

static int16_t i16(const uint8_t *p)
{
    int32_t value = (int32_t)u16(p);
    return (int16_t)(value < 32768 ? value : value - 65536);
}

static uint32_t rgb(const uint8_t *p)
{
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
}

static bool in_scene(const NxProgram *p, unsigned id, unsigned scene)
{
    return id < p->entities && p->entity[id].scene == scene;
}

static bool valid_rule(const NxProgram *p, const NxRule *r)
{
    if (r->scene >= p->scenes || r->event > NX_COLLISION || r->action > NX_PLACE_NEAR)
        return false;
    switch (r->event) {
    case NX_TICK:
        if (r->a || r->b) return false;
        break;
    case NX_COLLISION:
        if (r->a == r->b || !in_scene(p, r->a, r->scene) ||
            !in_scene(p, r->b, r->scene)) return false;
        break;
    default:
        if (r->a >= NX_KEYS || r->b) return false;
        break;
    }
    if (r->action == NX_GOTO)
        return r->target < p->scenes && !r->x && !r->y;
    if (!in_scene(p, r->target, r->scene)) return false;
    if (r->action == NX_PLACE_NEAR)
        return r->x >= 0 && (unsigned)r->x != r->target &&
               in_scene(p, (unsigned)r->x, r->scene);
    return true;
}

bool nx_parse(const uint8_t *bytes, size_t size, NxProgram *out)
{
    NxProgram p = {0};
    if (!bytes || !out || size < HEADER || memcmp(bytes, "NXG7", 4) != 0 ||
        bytes[4] != 0 || bytes[5] != 0) return false;
    p.scenes = u16(bytes + 6);
    p.entities = u16(bytes + 8);
    p.rules = u16(bytes + 10);
    if (!p.scenes || p.scenes > NX_SCENES || p.entities > NX_ENTITIES ||
        p.rules > NX_RULES || size != HEADER + p.scenes * SCENE +
        p.entities * ENTITY + p.rules * RULE) return false;
    const uint8_t *record = bytes + HEADER;
    for (size_t i = 0; i < p.scenes; ++i, record += SCENE) {
        if (record[3]) return false;
        p.background[i] = rgb(record);
    }
    for (size_t i = 0; i < p.entities; ++i, record += ENTITY) {
        NxEntity *e = &p.entity[i];
        e->scene = record[0];
        e->shape = record[1];
        e->x = (int32_t)i16(record + 2) * NX_HZ;
        e->y = (int32_t)i16(record + 4) * NX_HZ;
        e->vx = i16(record + 6);
        e->vy = i16(record + 8);
        e->width = (uint16_t)u16(record + 10);
        e->height = (uint16_t)u16(record + 12);
        e->rgb = rgb(record + 14);
        if (e->scene >= p.scenes || e->shape > NX_OVAL || !e->width ||
            !e->height || e->width > NX_WIDTH || e->height > NX_HEIGHT ||
            record[17]) return false;
    }
    for (size_t i = 0; i < p.rules; ++i, record += RULE) {
        NxRule *r = &p.rule[i];
        *r = (NxRule){record[0], record[1], record[2], record[3],
                      record[4], record[5], i16(record + 6), i16(record + 8)};
        if (!valid_rule(&p, r)) return false;
    }
    *out = p;
    return true;
}

bool nx_load(FILE *file, NxProgram *out)
{
    uint8_t bytes[MAX_FILE + 1];
    size_t size = fread(bytes, 1, sizeof bytes, file);
    return !ferror(file) && nx_parse(bytes, size, out);
}
