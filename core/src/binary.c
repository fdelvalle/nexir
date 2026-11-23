#include "internal.h"
#include <stdlib.h>
#include <string.h>

typedef struct {
    const uint8_t *input;
    uint8_t *output;
    size_t pos, end;
    bool failed;
} Stream;

static const uint8_t header[] = {'N','X','I','R','I','R','B','C',1,0,0,0};
enum { TYPES = 1, CONSTANTS, IMPORTS, FUNCTIONS, EXPORTS };

static void raw(Stream *s, void *bytes, size_t n)
{
    if (s->failed || n > s->end - s->pos) {
        s->failed = true;
        return;
    }
    if (s->input) memcpy(bytes, s->input + s->pos, n);
    else if (s->output) memcpy(s->output + s->pos, bytes, n);
    s->pos += n;
}

static void integer(Stream *s, uint32_t *v, unsigned width)
{
    uint8_t b[4] = {0};
    if (!s->input)
        for (unsigned i = 0; i < width; ++i) b[i] = (uint8_t)(*v >> (8 * i));
    raw(s, b, width);
    if (s->input) {
        *v = 0;
        for (unsigned i = 0; i < width; ++i) *v |= (uint32_t)b[i] << (8 * i);
    }
}

static void u16(Stream *s, uint16_t *value)
{
    uint32_t v = *value;
    integer(s, &v, 2);
    *value = (uint16_t)v;
}

static void u8(Stream *s, uint16_t *value)
{
    uint32_t v = *value;
    integer(s, &v, 1);
    *value = (uint16_t)v;
}

static bool count(Stream *s, uint16_t *n, unsigned maximum)
{
    u16(s, n);
    if (*n > maximum) s->failed = true;
    return !s->failed;
}

static void list(Stream *s, uint16_t *n, uint16_t *entries, unsigned maximum)
{
    if (!count(s, n, maximum)) return;
    for (unsigned i = 0; i < *n; ++i) u16(s, &entries[i]);
}

static void name(Stream *s, char *str)
{
    uint16_t n = s->input ? 0 : (uint16_t)strlen(str);
    u8(s, &n);
    if (!n || n > NX_NAME) {
        s->failed = true;
        return;
    }
    raw(s, str, n);
    if (s->input) {
        str[n] = 0;
        if (memchr(str, 0, n)) s->failed = true;
    }
}

static void signature(Stream *s, NxSignature *sig)
{
    u16(s, &sig->result);
    list(s, &sig->count, sig->types, NX_ARGS);
}

static void functions(Stream *s, NxBuilder *m)
{
    if (!count(s, &m->functions, NX_FUNCTIONS)) return;
    unsigned index = 0;
    for (unsigned i = 0; i < m->functions && !s->failed; ++i) {
        NxFunction *f = &m->function[i];
        signature(s, &f->signature);
        if (!count(s, &f->count, NX_BLOCKS)) return;
        for (unsigned j = 0; j < f->count && !s->failed; ++j) {
            NxBlock *b = &f->blocks[j];
            list(s, &b->count, b->types, NX_ARGS);
            if (!count(s, &b->length, NX_INSNS - index)) return;
            b->first = (uint16_t)index;
            for (unsigned k = 0; k < b->length && !s->failed; ++k) {
                NxInstruction *in = &m->instruction[index++];
                u8(s, &in->op);
                u8(s, &in->count);
                if (in->count > sizeof in->args / sizeof in->args[0]) {
                    s->failed = true;
                    return;
                }
                for (unsigned a = 0; a < in->count; ++a) u16(s, &in->args[a]);
            }
        }
    }
    m->instructions = (uint16_t)index;
}

static void section(Stream *s, unsigned id, NxBuilder *m)
{
    switch (id) {
    case TYPES:
        if (!count(s, &m->types, NX_TYPES)) return;
        for (unsigned i = 0; i < m->types && !s->failed; ++i) {
            u16(s, &m->type[i].kind);
            list(s, &m->type[i].count, m->type[i].entries, NX_ARGS);
        }
        break;
    case CONSTANTS:
        if (!count(s, &m->constants, NX_CONSTANTS)) return;
        for (unsigned i = 0; i < m->constants && !s->failed; ++i) {
            NxConstant *c = &m->constant[i];
            u16(s, &c->type);
            if (!count(s, &c->length, NX_LITERAL)) return;
            raw(s, c->data, c->length);
        }
        break;
    case IMPORTS:
        if (!count(s, &m->imports, NX_IMPORTS)) return;
        for (unsigned i = 0; i < m->imports && !s->failed; ++i) {
            NxImport *im = &m->import[i];
            name(s, im->interface);
            u16(s, &im->version);
            name(s, im->operation);
            signature(s, &im->signature);
        }
        break;
    case FUNCTIONS:
        functions(s, m);
        break;
    case EXPORTS:
        if (!count(s, &m->exports, NX_EXPORTS)) return;
        for (unsigned i = 0; i < m->exports && !s->failed; ++i) {
            name(s, m->exported[i].name);
            u16(s, &m->exported[i].function);
        }
        break;
    }
}

static void encode(Stream *s, NxBuilder *m)
{
    uint8_t magic[sizeof header];
    memcpy(magic, header, sizeof header);
    raw(s, magic, sizeof magic);
    uint32_t count = EXPORTS;
    integer(s, &count, 4);
    for (uint32_t id = TYPES; id <= EXPORTS; ++id) {
        Stream measure = {.end = NX_BINARY};
        section(&measure, id, m);
        if (measure.failed) {
            s->failed = true;
            return;
        }
        uint32_t flags = 1, length = (uint32_t)measure.pos;
        integer(s, &id, 2);
        integer(s, &flags, 2);
        integer(s, &length, 4);
        section(s, id, m);
    }
}

NxStatus nx_emit(const NxBuilder *builder, uint8_t *bytes, size_t capacity,
                 size_t *size, NxDiagnostic *d)
{
    if (!size || (!bytes && capacity)) return nx_error(d, NX_ARGUMENT, 0, "missing output");
    NxStatus status = nx_verify(builder, d);
    if (status != NX_OK) return status;
    NxBuilder *copy = malloc(sizeof *copy);
    if (!copy) return nx_error(d, NX_MEMORY, 0, "encoder allocation");
    *copy = *builder;
    Stream measure = {.end = NX_BINARY};
    encode(&measure, copy);
    if (measure.failed) {
        status = nx_error(d, NX_LIMIT, measure.pos, "binary limit");
    } else {
        *size = measure.pos;
        if (bytes && capacity < *size) {
            status = nx_error(d, NX_LIMIT, 0, "output capacity");
        } else if (bytes) {
            Stream output = {.output = bytes, .end = capacity};
            encode(&output, copy);
        }
    }
    free(copy);
    return status;
}

NxStatus nx_load(const uint8_t *bytes, size_t size, NxModule **module, NxDiagnostic *d)
{
    if (!module) return nx_error(d, NX_ARGUMENT, 0, "missing module output");
    *module = NULL;
    if (!bytes || size < 16 || memcmp(bytes, header, sizeof header))
        return nx_error(d, NX_FORMAT, 0, "expected NXIR IR 1.0");
    if (size > NX_BINARY) return nx_error(d, NX_LIMIT, 0, "binary limit");
    Stream s = {.input = bytes, .pos = sizeof header, .end = size};
    uint32_t n = 0;
    integer(&s, &n, 4);
    if (n < EXPORTS || n > 32) return nx_error(d, NX_FORMAT, s.pos, "section count");
    NxModule *m = calloc(1, sizeof *m);
    if (!m) return nx_error(d, NX_MEMORY, 0, "module allocation");
    uint32_t previous = 0;
    for (unsigned i = 0; i < n; ++i) {
        uint32_t id = 0, flags = 0, length = 0;
        integer(&s, &id, 2);
        integer(&s, &flags, 2);
        integer(&s, &length, 4);
        if (s.failed || id <= previous || length > size - s.pos ||
            (i < EXPORTS ? id != i + 1 || flags != 1 : flags != 0)) {
            s.failed = true;
            break;
        }
        previous = id;
        size_t end = s.pos + length;
        if (id <= EXPORTS) {
            s.end = end;
            section(&s, id, &m->ir);
            s.end = size;
            if (s.failed || s.pos != end) {
                s.failed = true;
                break;
            }
        } else {
            s.pos = end;
        }
    }
    NxStatus status = s.failed || s.pos != size ?
                      nx_error(d, NX_FORMAT, s.pos, "invalid section or trailing bytes") :
                      nx_verify(&m->ir, d);
    if (status == NX_OK) *module = m;
    else free(m);
    return status;
}

void nx_free(NxModule *module) { free(module); }
