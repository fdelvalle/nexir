#ifndef NEXIR_H
#define NEXIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum {
    NX_TYPES = 64, NX_CONSTANTS = 128, NX_IMPORTS = 32, NX_FUNCTIONS = 32,
    NX_EXPORTS = 32, NX_BLOCKS = 16, NX_INSNS = 4096, NX_ARGS = 8,
    NX_LOCALS = 128, NX_NAME = 63, NX_LITERAL = 4096, NX_BINARY = 262144,
    NX_FRAMES = 32, NX_OBJECTS = 1024, NX_HANDLES = 64, NX_MAX_FUEL = 1000000
};

typedef enum {
    NX_BOOL, NX_I64, NX_TEXT, NX_BYTES, NX_RECORD, NX_VARIANT, NX_HANDLE
} NxKind;

typedef enum {
    NX_CONST = 1, NX_RECORD_NEW, NX_RECORD_GET, NX_VARIANT_NEW, NX_VARIANT_TAG,
    NX_VARIANT_GET, NX_ADD, NX_SUB, NX_MUL, NX_DIV, NX_EQ, NX_LT, NX_NOT,
    NX_AND, NX_OR, NX_CALL, NX_CAP, NX_BR, NX_IF, NX_RETURN, NX_TRAP
} NxOp;

typedef enum {
    NX_OK, NX_FORMAT, NX_INVALID, NX_LIMIT, NX_MEMORY, NX_PERMISSION,
    NX_ARGUMENT, NX_HOST, NX_OVERFLOW, NX_DIV_ZERO, NX_WRONG_TAG, NX_TRAPPED
} NxStatus;

typedef struct { uint16_t result, count, types[NX_ARGS]; } NxSignature;
typedef struct { uint16_t kind, count, entries[NX_ARGS]; } NxType;
typedef struct {
    uint16_t type, length;
    uint8_t data[NX_LITERAL];
} NxConstant;
typedef struct {
    char interface[NX_NAME + 1], operation[NX_NAME + 1];
    uint16_t version;
    NxSignature signature;
} NxImport;
typedef struct {
    uint16_t op, count;
    uint16_t args[2 * NX_ARGS + 3];
} NxInstruction;
typedef struct {
    uint16_t count, types[NX_ARGS], first, length;
} NxBlock;
typedef struct {
    NxSignature signature;
    uint16_t count;
    NxBlock blocks[NX_BLOCKS];
} NxFunction;
typedef struct { char name[NX_NAME + 1]; uint16_t function; } NxExport;

/* The emitter accepts typed, ordered tables directly. No source text is parsed. */
typedef struct {
    uint16_t types, constants, imports, functions, exports, instructions;
    NxType type[NX_TYPES];
    NxConstant constant[NX_CONSTANTS];
    NxImport import[NX_IMPORTS];
    NxFunction function[NX_FUNCTIONS];
    NxExport exported[NX_EXPORTS];
    NxInstruction instruction[NX_INSNS];
} NxBuilder;

typedef struct NxModule NxModule;
typedef struct { NxStatus status; size_t offset; const char *message; } NxDiagnostic;

void nx_builder_init(NxBuilder *builder);
NxStatus nx_verify(const NxBuilder *builder, NxDiagnostic *diagnostic);
NxStatus nx_emit(const NxBuilder *builder, uint8_t *bytes, size_t capacity,
                 size_t *size, NxDiagnostic *diagnostic);
NxStatus nx_load(const uint8_t *bytes, size_t size, NxModule **module, NxDiagnostic *diagnostic);
void nx_free(NxModule *module);
void nx_inspect(const NxModule *module, FILE *output);
const char *nx_status(NxStatus status);
void nx_constant_i64(NxConstant *constant, int64_t value);

typedef struct { const uint8_t *data; size_t length; } NxSlice;
typedef struct {
    uint16_t type;
    union { bool boolean; int64_t integer; NxSlice slice; uint16_t handle; } as;
} NxValue;

typedef struct NxResources NxResources;
typedef NxStatus (*NxCapability)(void *context, NxResources *resources,
                                 const NxValue *args, size_t count, NxValue *result);
typedef void (*NxDrop)(void *resource);
typedef struct {
    const char *interface, *operation;
    uint16_t version;
} NxResourceOwner;
typedef struct {
    const char *interface, *operation;
    uint16_t version;
    NxSignature signature;
    void *context;
    NxCapability call;
    NxResourceOwner argument_owner[NX_ARGS];
} NxGrant;

/* Callback-only access. A successful create transfers ownership to the invocation. */
NxStatus nx_resource_create(NxResources *resources, uint16_t type, void *value,
                            NxDrop drop, NxValue *result);
void *nx_resource_get(NxResources *resources, NxValue value);

/* Slice results are copied into caller-owned storage. */
typedef struct { NxValue value; uint8_t slice[NX_LITERAL]; } NxResult;
NxStatus nx_run(const NxModule *module, const char *export_name,
                const NxValue *args, size_t count, const NxGrant *grants,
                size_t grant_count, uint32_t fuel, NxResult *result);

#endif
