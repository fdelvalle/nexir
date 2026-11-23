#include "engine.h"
#include <string.h>

void nx_start(NxWorld *world, const NxProgram *program)
{
    *world = (NxWorld){.program = program};
    memcpy(world->entity, program->entity, sizeof world->entity);
}

void nx_key(NxWorld *w, enum NxKey key, bool down)
{
    if ((unsigned)key >= NX_KEYS) return;
    unsigned bit = 1u << (unsigned)key;
    if (down && !(w->held & bit)) w->pressed |= bit;
    if (!down && (w->held & bit)) w->released |= bit;
    if (down) w->held |= bit;
    else w->held &= ~bit;
}

bool nx_aabb(const NxEntity *a, const NxEntity *b)
{
    return a->x < (int64_t)b->x + b->width * NX_HZ &&
           b->x < (int64_t)a->x + a->width * NX_HZ &&
           a->y < (int64_t)b->y + b->height * NX_HZ &&
           b->y < (int64_t)a->y + a->height * NX_HZ;
}

static bool matches(const NxWorld *w, const NxRule *r)
{
    switch (r->event) {
    case NX_TICK: return true;
    case NX_HELD: return (w->held & (1u << r->a)) != 0;
    case NX_PRESS: return (w->pressed & (1u << r->a)) != 0;
    case NX_RELEASE: return (w->released & (1u << r->a)) != 0;
    case NX_COLLISION: return nx_aabb(&w->entity[r->a], &w->entity[r->b]);
    }
    return false;
}

static int32_t bounded(int32_t position)
{
    const int32_t limit = 32768 * NX_HZ;
    if (position < -limit) return -limit;
    if (position > limit) return limit;
    return position;
}

static bool dispatch(NxWorld *w, bool collision)
{
    const NxProgram *p = w->program;
    for (size_t i = 0; i < p->rules; ++i) {
        const NxRule *r = &p->rule[i];
        if (r->scene != w->scene || (r->event == NX_COLLISION) != collision ||
            !matches(w, r)) continue;
        if (r->action == NX_GOTO) {
            w->scene = r->target;
            memcpy(w->entity, p->entity, sizeof w->entity);
            return true;
        }
        NxEntity *e = &w->entity[r->target];
        if (r->action == NX_VELOCITY) {
            e->vx = r->x;
            e->vy = r->y;
        } else if (r->action == NX_PLACE_NEAR) {
            const NxEntity *source = &w->entity[r->x];
            e->x = bounded(source->x + ((int32_t)source->width - e->width) * NX_HZ / 2);
            e->y = bounded(source->y + (int32_t)r->y * NX_HZ);
        } else {
            e->x = (int32_t)r->x * NX_HZ;
            e->y = (int32_t)r->y * NX_HZ;
        }
    }
    return false;
}

void nx_step(NxWorld *w)
{
    if (!dispatch(w, false)) {
        for (size_t i = 0; i < w->program->entities; ++i) {
            NxEntity *e = &w->entity[i];
            if (e->scene != w->scene) continue;
            e->x = bounded(e->x + e->vx);
            e->y = bounded(e->y + e->vy);
        }
        dispatch(w, true);
    }
    w->pressed = 0;
    w->released = 0;
}

void nx_render(const NxWorld *w, const NxRenderer *renderer)
{
    renderer->clear(renderer->context, w->program->background[w->scene]);
    for (size_t i = 0; i < w->program->entities; ++i)
        if (w->entity[i].scene == w->scene)
            renderer->draw(renderer->context, &w->entity[i]);
}
