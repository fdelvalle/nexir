#include "nexir.h"
#include <stdlib.h>
#include <string.h>

/* Keep the foreign interface independent of the layout of NxBuilder. */
NxBuilder *agent_new(void)
{
    NxBuilder *m = malloc(sizeof *m);
    if (m) nx_builder_init(m);
    return m;
}

void agent_free(NxBuilder *m) { free(m); }

static bool copy_name(char out[NX_NAME + 1], const char *name)
{
    size_t n = strlen(name);
    if (!n || n > NX_NAME) return false;
    memcpy(out, name, n + 1);
    return true;
}

static bool signature(NxSignature *sig, uint16_t result, const uint16_t *args, uint16_t n)
{
    if (n > NX_ARGS) return false;
    *sig = (NxSignature){.result = result, .count = n};
    if (n) memcpy(sig->types, args, n * sizeof *args);
    return true;
}

bool agent_type(NxBuilder *m, uint16_t kind, const uint16_t *entries, uint16_t n)
{
    if (m->types == NX_TYPES || n > NX_ARGS) return false;
    NxType *t = &m->type[m->types++];
    *t = (NxType){.kind = kind, .count = n};
    if (n) memcpy(t->entries, entries, n * sizeof *entries);
    return true;
}

bool agent_constant(NxBuilder *m, uint16_t type, const uint8_t *data, uint16_t n)
{
    if (m->constants == NX_CONSTANTS || n > NX_LITERAL) return false;
    NxConstant *c = &m->constant[m->constants++];
    c->type = type;
    c->length = n;
    if (n) memcpy(c->data, data, n);
    return true;
}

bool agent_import(NxBuilder *m, const char *interface, uint16_t version, const char *op,
                  uint16_t result, const uint16_t *args, uint16_t n)
{
    if (m->imports == NX_IMPORTS) return false;
    NxImport *im = &m->import[m->imports];
    if (!copy_name(im->interface, interface) || !copy_name(im->operation, op) ||
        !signature(&im->signature, result, args, n)) return false;
    im->version = version;
    ++m->imports;
    return true;
}

bool agent_function(NxBuilder *m, uint16_t result, const uint16_t *args, uint16_t n)
{
    if (m->functions == NX_FUNCTIONS ||
        !signature(&m->function[m->functions].signature, result, args, n)) return false;
    ++m->functions;
    return true;
}

bool agent_block(NxBuilder *m, const uint16_t *types, uint16_t n)
{
    if (!m->functions || n > NX_ARGS) return false;
    NxFunction *f = &m->function[m->functions - 1];
    if (f->count == NX_BLOCKS) return false;
    NxBlock *b = &f->blocks[f->count++];
    *b = (NxBlock){.count = n, .first = m->instructions};
    if (n) memcpy(b->types, types, n * sizeof *types);
    return true;
}

bool agent_instruction(NxBuilder *m, uint16_t op, const uint16_t *args, uint16_t n)
{
    if (!m->functions || m->instructions == NX_INSNS ||
        n > sizeof m->instruction[0].args / sizeof m->instruction[0].args[0]) return false;
    NxFunction *f = &m->function[m->functions - 1];
    if (!f->count) return false;
    NxInstruction *in = &m->instruction[m->instructions++];
    *in = (NxInstruction){.op = op, .count = n};
    if (n) memcpy(in->args, args, n * sizeof *args);
    ++f->blocks[f->count - 1].length;
    return true;
}

bool agent_export(NxBuilder *m, const char *name, uint16_t function)
{
    if (m->exports == NX_EXPORTS || !copy_name(m->exported[m->exports].name, name)) return false;
    m->exported[m->exports++].function = function;
    return true;
}
