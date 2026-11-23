#include "nxg.h"
#ifdef NX_X11
#include "frontend.h"
#endif
#include <string.h>

int main(int argc, char **argv)
{
    bool validate = argc == 3 && strcmp(argv[1], "--validate") == 0;
    if (!validate
#ifdef NX_X11
        && argc != 2
#endif
    ) {
        fprintf(stderr, "Usage: %s --validate file.nxg"
#ifdef NX_X11
                        " | %s file.nxg"
#endif
                        "\n", argv[0]
#ifdef NX_X11
                        , argv[0]
#endif
        );
        return 2;
    }
    const char *path = argv[validate ? 2 : 1];
    FILE *file = fopen(path, "rb");
    if (!file) {
        perror(path);
        return 1;
    }
    NxProgram program;
    bool valid = nx_load(file, &program);
    fclose(file);
    if (!valid) {
        fprintf(stderr, "%s: invalid or unsupported NXG7 file\n", path);
        return 1;
    }
#ifdef NX_X11
    if (!validate) return nx_window(&program);
#endif
    printf("NXG7: %zu scenes, %zu entities, %zu rules\n",
           program.scenes, program.entities, program.rules);
    return 0;
}
