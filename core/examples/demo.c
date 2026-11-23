#include "programs.h"
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

static int failure(NxStatus status, const NxDiagnostic *d)
{
    fprintf(stderr, "%s", nx_status(status));
    if (d) fprintf(stderr, " (%zu: %s)", d->offset, d->message);
    fputc('\n', stderr);
    return 1;
}

static int demo(void)
{
    static NxBuilder builder;
    static uint8_t bytes[NX_BINARY];
    for (unsigned i = 0; i < PROGRAMS; ++i) {
        NxDiagnostic diagnostic;
        size_t size;
        nx_example(&builder, i);
        NxStatus status = nx_emit(&builder, bytes, sizeof bytes, &size, &diagnostic);
        if (status != NX_OK) return failure(status, &diagnostic);
        char path[72];
        snprintf(path, sizeof path, "build/core/%s.nxir", nx_example_names[i]);
        FILE *file = fopen(path, "wb");
        if (!file) return failure(NX_ARGUMENT, NULL);
        size_t written = fwrite(bytes, 1, size, file);
        if (fclose(file) || written != size) return failure(NX_ARGUMENT, NULL);
        NxModule *module;
        status = nx_load(bytes, size, &module, &diagnostic);
        if (status != NX_OK) return failure(status, &diagnostic);
        NxValue args[] = {{.type = NX_I64, .as.integer = 7}, {.type = NX_I64, .as.integer = 12}};
        bool displayed = false;
        NxGrant grant = nx_ui_grant(&displayed);
        NxResult result;
        status = nx_run(module, "main", args, 2, &grant, i == PANEL, 100, &result);
        nx_free(module);
        if (status != NX_OK) return failure(status, NULL);
        printf("%s (%zu bytes): ", nx_example_names[i], size);
        if (result.value.type == NX_I64) printf("%" PRId64, result.value.as.integer);
        else printf("%s", result.value.as.boolean ? "approved" : "denied");
        if (i == PANEL) printf(" (mock UI: %s)", displayed ? "approved" : "denied");
        putchar('\n');
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 1) return demo();
    if (argc != 3 || strcmp(argv[1], "--inspect")) {
        fprintf(stderr, "Usage: %s [--inspect file.nxir]\n", argv[0]);
        return 2;
    }
    FILE *file = fopen(argv[2], "rb");
    if (!file) return failure(NX_ARGUMENT, NULL);
    static uint8_t bytes[NX_BINARY + 1];
    size_t size = fread(bytes, 1, sizeof bytes, file);
    int error = ferror(file);
    fclose(file);
    if (error) return failure(NX_FORMAT, NULL);
    NxModule *module;
    NxDiagnostic diagnostic;
    NxStatus status = nx_load(bytes, size, &module, &diagnostic);
    if (status != NX_OK) return failure(status, &diagnostic);
    nx_inspect(module, stdout);
    nx_free(module);
    return 0;
}
