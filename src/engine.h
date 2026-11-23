#ifndef NX_ENGINE_H
#define NX_ENGINE_H
#include "nxg.h"

typedef struct {
    const NxProgram *program;
    NxEntity entity[NX_ENTITIES];
    unsigned scene, held, pressed, released;
} NxWorld;

typedef struct {
    void *context;
    void (*clear)(void *, uint32_t);
    void (*draw)(void *, const NxEntity *);
} NxRenderer;

/* The program must come from a successful nx_parse/nx_load and outlive the world. */
void nx_start(NxWorld *world, const NxProgram *program);
void nx_key(NxWorld *world, enum NxKey key, bool down);
void nx_step(NxWorld *world);
bool nx_aabb(const NxEntity *a, const NxEntity *b);
void nx_render(const NxWorld *world, const NxRenderer *renderer);
#endif
