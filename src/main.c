#include "app.h"
#include "nxg.h"
#ifdef NX_X11
#include "frontend.h"
#endif
#include <string.h>

static int usage(const char *name)
{
    fprintf(stderr, "Usage: %s --validate file.nx|file.nxg\n"
                    "       %s --run file.nx action_id\n"
                    "       %s --run-seq file.nx action_id count\n", name, name, name);
#ifdef NX_X11
    fprintf(stderr, "       %s file.nxg | file.nx [legacy.nxg]\n", name);
#endif
    return 2;
}

int main(int argc, char **argv)
{
    bool validate = argc == 3 && strcmp(argv[1], "--validate") == 0;
    if ((argc == 4 && strcmp(argv[1], "--run") == 0) ||
        (argc == 5 && strcmp(argv[1], "--run-seq") == 0))
        return app_cli(argc, argv);
    if (!validate
#ifdef NX_X11
        && argc != 2 && argc != 3
#endif
    ) return usage(argv[0]);
    const char *path = argv[validate ? 2 : 1];
    if (argc == 2 && strcmp(path, "--help") == 0) {
        usage(argv[0]);
        return 0;
    }
    if (path[0] == '-' && !validate) return usage(argv[0]);

    FILE *file = fopen(path, "rb");
    if (!file) {
        perror(path);
        return 1;
    }
    char magic[4];
    size_t count = fread(magic, 1, sizeof magic, file);
    if (count == sizeof magic && memcmp(magic, "NXIR", 4) == 0) {
        fclose(file);
        if (validate) return app_cli(argc, argv);
#ifdef NX_X11
#ifdef NX_WORKSPACE
        return app_workspace(argc, argv);
#else
        if (argc != 2) return usage(argv[0]);
        Program program;
        return app_load(path, &program) ? app_window(&program) : 2;
#endif
#endif
    }
    NxProgram program;
    bool valid = count == sizeof magic && fseek(file, 0, SEEK_SET) == 0 &&
                 nx_load(file, &program);
    fclose(file);
    if (!valid) {
        fprintf(stderr, "%s: invalid or unsupported binary\n", path);
        return 1;
    }
#ifdef NX_X11
    if (!validate) return argc == 2 ? nx_window(&program) : usage(argv[0]);
#endif
    printf("NXG7: %zu scenes, %zu entities, %zu rules\n",
           program.scenes, program.entities, program.rules);
    return 0;
}
