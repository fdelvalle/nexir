#include "programs.h"
#include <assert.h>
#include <limits.h>
#include <string.h>

static NxBuilder builder;
static uint8_t bytes[NX_BINARY];

/* Independently encoded identity module: (i64) -> i64. */
static const uint8_t identity[] = {
    'N','X','I','R','I','R','B','C', 1,0, 0,0, 5,0,0,0,
    1,0, 1,0, 18,0,0,0,
    4,0, 0,0,0,0, 1,0,0,0, 2,0,0,0, 3,0,0,0,
    2,0, 1,0, 2,0,0,0, 0,0,
    3,0, 1,0, 2,0,0,0, 0,0,
    4,0, 1,0, 20,0,0,0,
    1,0, 1,0, 1,0,1,0, 1,0, 1,0,1,0, 1,0, 20,1,0,0,
    5,0, 1,0, 9,0,0,0, 1,0, 4,'m','a','i','n',0,0,
};

static NxValue i64(int64_t value)
{
    return (NxValue){.type = NX_I64, .as.integer = value};
}

static void entry(NxSignature sig, unsigned length)
{
    builder.functions = 1;
    builder.exports = 1;
    builder.exported[0] = (NxExport){"main", 0};
    NxFunction *f = &builder.function[0];
    f->signature = sig;
    f->count = 1;
    f->blocks[0].count = sig.count;
    memcpy(f->blocks[0].types, sig.types, sig.count * sizeof sig.types[0]);
    f->blocks[0].length = (uint16_t)length;
    builder.instructions = (uint16_t)length;
}

static NxModule *roundtrip(void)
{
    size_t size, measured;
    NxDiagnostic diagnostic;
    NxModule *module = NULL;
    assert(nx_emit(&builder, NULL, 0, &measured, &diagnostic) == NX_OK);
    assert(nx_emit(&builder, bytes, sizeof bytes, &size, &diagnostic) == NX_OK);
    assert(size == measured);
    assert(nx_load(bytes, size, &module, &diagnostic) == NX_OK);
    return module;
}

static void encoding(void)
{
    nx_builder_init(&builder);
    entry((NxSignature){NX_I64, 1, {NX_I64}}, 1);
    builder.instruction[0] = (NxInstruction){NX_RETURN, 1, {0}};
    NxModule *module = roundtrip();
    assert(!memcmp(bytes, identity, sizeof identity));
    NxResult result;
    NxValue arg = i64(INT64_MIN);
    assert(nx_run(module, "main", &arg, 1, NULL, 0, 1, &result) == NX_OK);
    assert(result.value.as.integer == INT64_MIN);
    nx_free(module);
    NxDiagnostic diagnostic;
    for (size_t i = 0; i < sizeof identity; ++i) {
        assert(nx_load(identity, i, &module, &diagnostic) != NX_OK);
        assert(!module);
    }
    size_t size;
    assert(nx_emit(&builder, bytes, 1, &size, NULL) == NX_LIMIT && size == sizeof identity);
    assert(!memcmp(bytes, identity, sizeof identity));
    bytes[8] = 2;
    assert(nx_load(bytes, size, &module, NULL) == NX_FORMAT);
    bytes[8] = 1;
    bytes[24] = 65;
    assert(nx_load(bytes, size, &module, NULL) == NX_FORMAT);
    bytes[24] = 4;
    assert(nx_load(bytes, size + 1, &module, NULL) == NX_FORMAT);
    bytes[12] = 6;
    const uint8_t optional[] = {6,0, 0,0, 1,0,0,0, 0x99};
    memcpy(bytes + size, optional, sizeof optional);
    assert(nx_load(bytes, size + sizeof optional, &module, NULL) == NX_OK);
    nx_free(module);
    bytes[size + 2] = 1;
    assert(nx_load(bytes, size + sizeof optional, &module, NULL) == NX_FORMAT);
    bytes[size] = 5;
    bytes[size + 2] = 0;
    assert(nx_load(bytes, size + sizeof optional, &module, NULL) == NX_FORMAT);

    memcpy(bytes, identity, size);
    bytes[size - 19] = 1; /* return of an undefined local */
    assert(nx_load(bytes, size, &module, &diagnostic) == NX_INVALID);
    assert(diagnostic.offset == 0 && !module);
}

static void programs(void)
{
    for (unsigned program = 0; program < PROGRAMS; ++program) {
        nx_example(&builder, program);
        NxModule *module = roundtrip();
        NxValue args[] = {i64(7), i64(12)};
        NxResult result;
        bool displayed = false;
        NxGrant grant = nx_ui_grant(&displayed);
        if (program == PANEL) {
            assert(nx_run(module, "main", args, 2, NULL, 0, 100, &result) == NX_PERMISSION);
            grant.signature.types[0] = NX_I64;
            assert(nx_run(module, "main", args, 2, &grant, 1, 100, &result) == NX_PERMISSION);
            grant = nx_ui_grant(&displayed);
        }
        assert(nx_run(module, "main", args, 2, &grant, program == PANEL, 100, &result) == NX_OK);
        if (program == CALCULATION) assert(result.value.as.integer == 228);
        else {
            assert(result.value.as.boolean);
            if (program == PANEL) assert(displayed);
            args[0] = i64(15);
            assert(nx_run(module, "main", args, 2, &grant, program == PANEL, 100, &result) == NX_OK);
            assert(!result.value.as.boolean);
            if (program == PANEL) assert(!displayed);
        }
        assert(nx_run(module, "main", args, 1, &grant, 1, 100, &result) == NX_ARGUMENT);
        assert(nx_run(module, "main", args, 2, &grant, program == PANEL, 0, &result) == NX_LIMIT);
        nx_free(module);
    }
}

static void arithmetic(void)
{
    const struct { NxOp op; int64_t a, b; NxStatus status; int64_t value; } cases[] = {
        {NX_ADD, INT64_MAX, 1, NX_OVERFLOW, 0}, {NX_ADD, INT64_MIN, -1, NX_OVERFLOW, 0},
        {NX_SUB, INT64_MIN, 1, NX_OVERFLOW, 0}, {NX_SUB, INT64_MAX, -1, NX_OVERFLOW, 0},
        {NX_MUL, INT64_MIN, -1, NX_OVERFLOW, 0}, {NX_MUL, -1, INT64_MIN, NX_OVERFLOW, 0},
        {NX_MUL, INT64_MAX, 2, NX_OVERFLOW, 0}, {NX_MUL, INT64_MIN, 1, NX_OK, INT64_MIN},
        {NX_MUL, INT64_MIN, 0, NX_OK, 0}, {NX_MUL, -7, -3, NX_OK, 21},
        {NX_DIV, INT64_MIN, -1, NX_OVERFLOW, 0}, {NX_DIV, 7, 0, NX_DIV_ZERO, 0},
        {NX_DIV, -7, 3, NX_OK, -2}, {NX_DIV, 7, -3, NX_OK, -2},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; ++i) {
        nx_builder_init(&builder);
        entry((NxSignature){NX_I64, 2, {NX_I64, NX_I64}}, 2);
        builder.instruction[0] = (NxInstruction){cases[i].op, 2, {0, 1}};
        builder.instruction[1] = (NxInstruction){NX_RETURN, 1, {2}};
        NxModule *module = roundtrip();
        NxValue args[] = {i64(cases[i].a), i64(cases[i].b)};
        NxResult result = {.value = {.type = NX_I64, .as.integer = 42}};
        assert(nx_run(module, "main", args, 2, NULL, 0, 2, &result) == cases[i].status);
        assert(result.value.as.integer == (cases[i].status == NX_OK ? cases[i].value : 42));
        nx_free(module);
    }
}

static void control(void)
{
    nx_builder_init(&builder);
    entry((NxSignature){NX_I64, 2, {NX_I64, NX_I64}}, 2);
    builder.function[0].count = 2;
    builder.function[0].blocks[0].length = 1;
    builder.function[0].blocks[1] = (NxBlock){.count = 2, .types = {NX_I64,NX_I64}, .first = 1, .length = 1};
    builder.instruction[0] = (NxInstruction){NX_BR, 3, {1,1,0}};
    builder.instruction[1] = (NxInstruction){NX_RETURN, 1, {0}};
    NxModule *module = roundtrip();
    NxResult result;
    NxValue args[] = {i64(7), i64(12)};
    assert(nx_run(module, "main", args, 2, NULL, 0, 2, &result) == NX_OK);
    assert(result.value.as.integer == 12);
    nx_free(module);

    builder.instruction[0].args[1] = 2;
    assert(nx_verify(&builder, NULL) == NX_INVALID);
    builder.instruction[0] = (NxInstruction){NX_BR, 3, {0,0,1}};
    module = roundtrip();
    assert(nx_run(module, "main", args, 2, NULL, 0, 49, &result) == NX_LIMIT);
    nx_free(module);

    nx_builder_init(&builder);
    entry((NxSignature){NX_I64, 0, {0}}, 2);
    builder.instruction[0] = (NxInstruction){NX_CALL, 1, {0}};
    builder.instruction[1] = (NxInstruction){NX_RETURN, 1, {0}};
    module = roundtrip();
    assert(nx_run(module, "main", NULL, 0, NULL, 0, 100, &result) == NX_LIMIT);
    nx_free(module);
}

static void resource_limits(void)
{
    nx_builder_init(&builder);
    builder.type[builder.types++] = (NxType){.kind = NX_RECORD};
    entry((NxSignature){NX_I64, 0, {0}}, 2);
    builder.instruction[0] = (NxInstruction){NX_RECORD_NEW, 1, {4}};
    builder.instruction[1] = (NxInstruction){NX_BR, 1, {0}};
    NxModule *module = roundtrip();
    NxResult result;
    assert(nx_run(module, "main", NULL, 0, NULL, 0, NX_MAX_FUEL, &result) == NX_LIMIT);
    nx_free(module);
    builder.instruction[1] = (NxInstruction){NX_TRAP, 0, {0}};
    module = roundtrip();
    assert(nx_run(module, "main", NULL, 0, NULL, 0, 2, &result) == NX_TRAPPED);
    nx_free(module);
}

static void types(void)
{
    nx_builder_init(&builder);
    builder.type[builder.types++] = (NxType){NX_VARIANT, 2, {NX_I64, NX_I64}};
    entry((NxSignature){NX_I64, 1, {NX_I64}}, 3);
    builder.instruction[0] = (NxInstruction){NX_VARIANT_NEW, 3, {4,0,0}};
    builder.instruction[1] = (NxInstruction){NX_VARIANT_GET, 2, {1,1}};
    builder.instruction[2] = (NxInstruction){NX_RETURN, 1, {2}};
    NxModule *module = roundtrip();
    NxValue arg = i64(7);
    NxResult result;
    assert(nx_run(module, "main", &arg, 1, NULL, 0, 3, &result) == NX_WRONG_TAG);
    nx_free(module);
    builder.instruction[1] = (NxInstruction){NX_VARIANT_TAG, 1, {1}};
    module = roundtrip();
    assert(nx_run(module, "main", &arg, 1, NULL, 0, 3, &result) == NX_OK);
    assert(result.value.as.integer == 0);
    nx_free(module);

    nx_builder_init(&builder);
    entry((NxSignature){NX_TEXT, 0, {0}}, 2);
    builder.constants = 1;
    builder.constant[0] = (NxConstant){NX_TEXT, 2, {0xc3,0xa9}};
    builder.instruction[0] = (NxInstruction){NX_CONST, 1, {0}};
    builder.instruction[1] = (NxInstruction){NX_RETURN, 1, {0}};
    module = roundtrip();
    assert(nx_run(module, "main", NULL, 0, NULL, 0, 2, &result) == NX_OK);
    nx_free(module);
    assert(result.value.as.slice.length == 2 && result.value.as.slice.data == result.slice);
    assert(result.slice[0] == 0xc3 && result.slice[1] == 0xa9);
    builder.constant[0].data[0] = 0xc0;
    assert(nx_verify(&builder, NULL) == NX_INVALID);
    builder.constant[0].data[0] = 0xc3;
    builder.constants = 2;
    builder.constant[1] = builder.constant[0];
    assert(nx_verify(&builder, NULL) == NX_INVALID);
}

typedef struct { unsigned opened, used, dropped; } Counts;
static void drop(void *p)
{
    ++((Counts *)p)->dropped;
}

static NxStatus open_resource(void *p, NxResources *r, const NxValue *args,
                              size_t count, NxValue *result)
{
    (void)args;
    assert(!count);
    ++((Counts *)p)->opened;
    return nx_resource_create(r, 4, p, drop, result);
}

static NxStatus use_resource(void *p, NxResources *r, const NxValue *args,
                             size_t count, NxValue *result)
{
    Counts *counts = p;
    assert(count == 1 && nx_resource_get(r, args[0]) == counts);
    assert(nx_resource_get(r, (NxValue){.type = 4, .as.handle = 63}) == NULL);
    *result = i64(++counts->used);
    return NX_OK;
}

static void handles(void)
{
    nx_builder_init(&builder);
    entry((NxSignature){NX_I64, 0, {0}}, 3);
    builder.type[builder.types++] = (NxType){NX_HANDLE, 1, {0}};
    builder.imports = 2;
    builder.import[0] = (NxImport){"fs.mock", "open", 1, {4, 0, {0}}};
    builder.import[1] = (NxImport){"fs.mock", "use", 1, {NX_I64, 1, {4}}};
    builder.instruction[0] = (NxInstruction){NX_CAP, 1, {0}};
    builder.instruction[1] = (NxInstruction){NX_CAP, 2, {1,0}};
    builder.instruction[2] = (NxInstruction){NX_RETURN, 1, {1}};
    Counts counts = {0};
    NxGrant grants[] = {
        {.interface = "fs.mock", .operation = "open", .version = 1,
         .signature = {4, 0, {0}}, .context = &counts, .call = open_resource},
        {.interface = "fs.mock", .operation = "use", .version = 1,
         .signature = {NX_I64, 1, {4}}, .context = &counts, .call = use_resource,
         .argument_owner = {{"fs.mock", "open", 1}}},
    };
    NxModule *module = roundtrip();
    NxResult result;
    grants[1].argument_owner[0].operation = "other";
    assert(nx_run(module, "main", NULL, 0, grants, 2, 3, &result) == NX_PERMISSION);
    assert(!counts.opened && !counts.dropped);
    grants[1].argument_owner[0].operation = "open";
    assert(nx_run(module, "main", NULL, 0, grants, 2, 1, &result) == NX_LIMIT);
    assert(counts.opened == 1 && counts.used == 0 && counts.dropped == 1);
    assert(nx_run(module, "main", NULL, 0, grants, 2, 3, &result) == NX_OK);
    assert(result.value.as.integer == 1);
    assert(counts.opened == 2 && counts.used == 1 && counts.dropped == 2);
    nx_free(module);
}

int main(void)
{
    encoding();
    programs();
    arithmetic();
    control();
    resource_limits();
    types();
    handles();
    puts("NEXIR IR 1.0 conformance checks passed");
    return 0;
}
