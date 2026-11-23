#include "src/app.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int number(const char *text, int max)
{
    char *end;
    errno = 0;
    long value = strtol(text, &end, 10);
    return errno || end == text || *end || value < 0 || value > max ? -1 : (int)value;
}

int app_cli(int argc, char **argv)
{
    Program program;
    if (!app_load(argv[2], &program)) return 2;
    if (argc == 3) {
        printf("VALID: %s (%d widgets, %d actions)\n",
               program.title, program.nw, program.na);
        return 0;
    }

    int id = number(argv[3], 255);
    int times = argc == 5 ? number(argv[4], 100) : 1;
    if (id < 0 || times < 1) return 5;
    for (int i = 0; i < program.na; ++i) {
        const Action *action = &program.actions[i];
        if (action->id != id) continue;
        if (argc == 4) {
            int ok = app_execute(&program, action);
            printf("ok=%d status=%s", ok, status);
            for (int k = 0; k < 16; ++k)
                printf(" var%d=%lld", k, (long long)program.vars[k]);
            puts("");
            return ok ? 0 : 4;
        }
        for (int step = 1; step <= times; ++step) {
            if (!app_execute(&program, action)) return 4;
            printf("step=%d status=%s var0=%lld\n",
                   step, status, (long long)program.vars[0]);
        }
        return 0;
    }
    return 5;
}
