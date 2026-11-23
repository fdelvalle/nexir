#ifndef NXG_H
#define NXG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum { NX_HZ = 60, NX_WIDTH = 640, NX_HEIGHT = 480,
       NX_SCENES = 16, NX_ENTITIES = 128, NX_RULES = 256 };
enum NxKey { NX_LEFT, NX_RIGHT, NX_UP, NX_DOWN, NX_SPACE, NX_RESTART, NX_KEYS };
enum NxShape { NX_RECT, NX_OUTLINE, NX_OVAL };
enum NxEvent { NX_TICK, NX_HELD, NX_PRESS, NX_RELEASE, NX_COLLISION };
enum NxAction { NX_VELOCITY, NX_POSITION, NX_GOTO, NX_PLACE_NEAR };

typedef struct {
    int32_t x, y, vx, vy;
    uint16_t width, height;
    uint32_t rgb;
    uint8_t scene, shape;
} NxEntity;

typedef struct {
    uint8_t scene, event, a, b, action, target;
    int16_t x, y;
} NxRule;

typedef struct {
    size_t scenes, entities, rules;
    uint32_t background[NX_SCENES];
    NxEntity entity[NX_ENTITIES];
    NxRule rule[NX_RULES];
} NxProgram;

bool nx_parse(const uint8_t *bytes, size_t size, NxProgram *out);
bool nx_load(FILE *file, NxProgram *out);

#endif
