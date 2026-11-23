#include "app.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    Program app;
    assert(app_load("build/legacy/wallet.nx", &app));
    strcpy(inputs[1], "250");
    assert(app_execute(&app, &app.actions[0]));
    assert(app.vars[0] == 499750 && !inputs[1][0]);
    strcpy(inputs[1], "999999");
    assert(!app_execute(&app, &app.actions[0]));
    assert(app.vars[0] == 499750 && inputs[1][0]);

    assert(app_load("build/legacy/calculator.nx", &app));
    strcpy(inputs[1], "42");
    assert(app_execute(&app, &app.actions[0]));
    assert(app.vars[0] == 42);
    app.vars[0] = INT64_MAX;
    strcpy(inputs[1], "1");
    assert(!app_execute(&app, &app.actions[0]));
    assert(app.vars[0] == INT64_MAX);

    assert(app_load("build/legacy/toggle.nx", &app));
    assert(app_execute(&app, &app.actions[0]) && app.vars[0] == 1);
    assert(app_execute(&app, &app.actions[0]) && app.vars[0] == 0);

    FILE *file = fopen("build/legacy/wallet.nx", "rb");
    assert(file);
    unsigned char bytes[512];
    size_t size = fread(bytes, 1, sizeof bytes, file);
    assert(!ferror(file) && size < sizeof bytes);
    fclose(file);
    /* Skip the title, record count, variable record and first widget. */
    size_t widget = 11 + bytes[9] + 2 + 12;
    while (widget < size && (bytes[widget] != 1 || bytes[widget + 3] != 4))
        widget += 3 + bytes[widget + 1] + ((size_t)bytes[widget + 2] << 8);
    assert(widget + 4 < size);
    bytes[widget + 4] = 255;
    file = fopen("build/legacy/invalid.nx", "wb");
    assert(file);
    assert(fwrite(bytes, 1, size, file) == size);
    assert(fclose(file) == 0);
    assert(!app_load("build/legacy/invalid.nx", &app));
    assert(strcmp(app.title, "NEXIR Toggle") == 0);
    puts("NXIR v3 application tests passed");
}
