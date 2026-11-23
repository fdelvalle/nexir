#include "engine.h"
#include <assert.h>
#include <string.h>

static NxProgram load(const char *path)
{
    NxProgram program;
    FILE *file = fopen(path, "rb");
    assert(file);
    assert(nx_load(file, &program));
    assert(fclose(file) == 0);
    return program;
}

static void parser(void)
{
    uint8_t file[512];
    FILE *input = fopen("build/crossing.nxg", "rb");
    assert(input);
    size_t size = fread(file, 1, sizeof file, input);
    assert(size < sizeof file && !ferror(input));
    fclose(input);
    NxProgram program;
    assert(nx_parse(file, size, &program));
    for (size_t i = 0; i < size; ++i) {
        assert(!nx_parse(file, i, &program));
        assert(program.rules == 10);
    }
    file[size] = 0;
    assert(!nx_parse(file, size + 1, &program));

    const size_t first_entity = 12 + 2 * 4;
    const size_t first_rule = first_entity + 6 * 18;
    const struct { size_t index; uint8_t value; } invalid[] = {
        {3, '6'}, {4, 1}, {5, 1}, {6, 0}, {7, 1}, {9, 1}, {11, 2},
        {15, 1}, {first_entity, 2}, {first_entity + 1, 3},
        {first_entity + 10, 0}, {first_entity + 17, 1},
        {first_rule, 2}, {first_rule + 1, 255}, {first_rule + 2, 1},
        {first_rule + 4, 255}, {first_rule + 5, 128},
        {first_rule + 10 + 2, NX_KEYS}, {first_rule + 40 + 3, 0},
        {first_rule + 60 + 5, 2}, {first_rule + 60 + 6, 1},
        {first_rule + 90 + 4, NX_VELOCITY},
    };
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; ++i) {
        size_t index = invalid[i].index;
        uint8_t original = file[index];
        file[index] = invalid[i].value;
        assert(!nx_parse(file, size, &program));
        assert(program.rules == 10);
        file[index] = original;
    }
    assert(nx_parse(file, size, &program));
}

static void physics(void)
{
    NxProgram program = load("build/crossing.nxg");
    NxWorld world;
    nx_start(&world, &program);
    for (int i = 0; i < NX_HZ; ++i) nx_step(&world);
    assert(world.entity[1].x == 210 * NX_HZ);

    NxEntity a = {.width = 10, .height = 10};
    NxEntity b = {.x = 10 * NX_HZ, .width = 10, .height = 10};
    assert(!nx_aabb(&a, &b));
    --b.x;
    assert(nx_aabb(&a, &b));
    b.y = -10 * NX_HZ;
    assert(!nx_aabb(&a, &b));

    nx_key(&world, NX_UP, true);
    nx_step(&world);
    assert(world.entity[0].y == 440 * NX_HZ - 220);
    assert(!world.pressed);
    nx_key(&world, NX_UP, true);
    assert(!world.pressed);
    nx_key(&world, NX_UP, false);
    assert(world.released == (1u << NX_UP));
    nx_step(&world);
    assert(world.entity[0].vy == 0 && !world.released);

    world.entity[0].x = world.entity[3].x;
    world.entity[0].y = world.entity[3].y;
    nx_step(&world);
    assert(world.scene == 1);
    nx_key(&world, NX_SPACE, true);
    nx_key(&world, NX_SPACE, false);
    nx_step(&world);
    assert(world.scene == 0 && world.entity[0].y == 440 * NX_HZ);
}

typedef struct { uint32_t background; unsigned draws; } Frame;

static void clear(void *context, uint32_t rgb)
{
    Frame *frame = context;
    *frame = (Frame){.background = rgb};
}

static void draw(void *context, const NxEntity *entity)
{
    Frame *frame = context;
    assert(entity->width && entity->height);
    ++frame->draws;
}

static void second_game(void)
{
    NxProgram program = load("build/bounce.nxg");
    NxWorld world;
    nx_start(&world, &program);
    Frame frame;
    NxRenderer renderer = {&frame, clear, draw};
    nx_render(&world, &renderer);
    assert(frame.background == 0x192235 && frame.draws == 6);
    world.entity[1].y = 468 * NX_HZ;
    nx_step(&world);
    assert(world.scene == 1);
    nx_render(&world, &renderer);
    assert(frame.background == 0x672d44 && !frame.draws);
}

static void river(void)
{
    NxProgram program = load("build/river_flight.nxg");
    NxWorld world;
    nx_start(&world, &program);
    nx_key(&world, NX_SPACE, true);
    nx_step(&world);
    assert(world.entity[7].x == world.entity[3].x + 25 * NX_HZ / 2);
    assert(world.entity[7].vy == -420);
    world.entity[6].x = world.entity[3].x;
    world.entity[6].y = world.entity[3].y;
    nx_step(&world);
    assert(world.entity[9].y == 447 * NX_HZ);
    nx_key(&world, NX_LEFT, true);
    for (int i = 0; i < NX_HZ; ++i) nx_step(&world);
    assert(world.scene == 1);
    nx_key(&world, NX_RESTART, true);
    nx_step(&world);
    assert(world.scene == 0 && world.entity[3].x == 305 * NX_HZ);
}

int main(void)
{
    parser();
    physics();
    second_game();
    river();
    puts("NXG7 parser, VM, physics, input and rendering tests passed");
}
