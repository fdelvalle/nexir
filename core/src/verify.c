#include "internal.h"
#include <string.h>

static bool signature(const NxBuilder *m, const NxSignature *s)
{
    if (s->result >= m->types || s->count > NX_ARGS) return false;
    for (unsigned i = 0; i < s->count; ++i)
        if (s->types[i] >= m->types) return false;
    return true;
}

static bool boundary_type(const NxBuilder *m, uint16_t type, bool host)
{
    return type < 4 || (host && m->type[type].kind == NX_HANDLE);
}

static bool boundary_signature(const NxBuilder *m, const NxSignature *s, bool host)
{
    if (!boundary_type(m, s->result, host)) return false;
    for (unsigned i = 0; i < s->count; ++i)
        if (!boundary_type(m, s->types[i], host)) return false;
    return true;
}

static bool value(const uint16_t *types, unsigned n, unsigned id, unsigned expected)
{
    return id < n && types[id] == expected;
}

static bool arguments(const uint16_t *types, unsigned n, const uint16_t *args,
                      const uint16_t *expected, unsigned count)
{
    for (unsigned i = 0; i < count; ++i)
        if (!value(types, n, args[i], expected[i])) return false;
    return true;
}

static bool edge(const NxFunction *f, const uint16_t *types, unsigned n,
                 const uint16_t *args, unsigned count)
{
    if (!count || args[0] >= f->count) return false;
    const NxBlock *b = &f->blocks[args[0]];
    return count == 1u + b->count && arguments(types, n, args + 1, b->types, b->count);
}

static bool instruction(const NxBuilder *m, const NxFunction *f, const NxInstruction *in,
                        const uint16_t *types, unsigned n, uint16_t *result)
{
    const uint16_t *a = in->args;
    unsigned count = in->count;
    *result = 0;
    switch (in->op) {
    case NX_CONST:
        if (count != 1 || a[0] >= m->constants) return false;
        *result = m->constant[a[0]].type;
        return true;
    case NX_RECORD_NEW:
        if (!count || a[0] >= m->types) return false;
        *result = a[0];
        return m->type[*result].kind == NX_RECORD &&
               count == 1u + m->type[*result].count &&
               arguments(types, n, a + 1, m->type[*result].entries, count - 1);
    case NX_VARIANT_NEW:
        if (count != 3 || a[0] >= m->types) return false;
        *result = a[0];
        return m->type[*result].kind == NX_VARIANT && a[1] < m->type[*result].count &&
               value(types, n, a[2], m->type[*result].entries[a[1]]);
    case NX_RECORD_GET: case NX_VARIANT_GET: case NX_VARIANT_TAG: {
        if (count != (in->op == NX_VARIANT_TAG ? 1u : 2u) || a[0] >= n) return false;
        const NxType *type = &m->type[types[a[0]]];
        if (type->kind != (in->op == NX_RECORD_GET ? NX_RECORD : NX_VARIANT)) return false;
        if (in->op == NX_VARIANT_TAG) {
            *result = NX_I64;
            return true;
        }
        if (a[1] >= type->count) return false;
        *result = type->entries[a[1]];
        return true;
    }
    case NX_ADD: case NX_SUB: case NX_MUL: case NX_DIV: case NX_EQ: case NX_LT:
        *result = in->op == NX_EQ || in->op == NX_LT ? NX_BOOL : NX_I64;
        return count == 2 && value(types, n, a[0], NX_I64) && value(types, n, a[1], NX_I64);
    case NX_NOT: case NX_AND: case NX_OR:
        *result = NX_BOOL;
        return count == (in->op == NX_NOT ? 1u : 2u) &&
               value(types, n, a[0], NX_BOOL) &&
               (in->op == NX_NOT || value(types, n, a[1], NX_BOOL));
    case NX_CALL: case NX_CAP: {
        if (!count || a[0] >= (in->op == NX_CALL ? m->functions : m->imports)) return false;
        const NxSignature *s = in->op == NX_CALL ? &m->function[a[0]].signature :
                                                  &m->import[a[0]].signature;
        *result = s->result;
        return count == 1u + s->count && arguments(types, n, a + 1, s->types, s->count);
    }
    case NX_BR:
        return edge(f, types, n, a, count);
    case NX_IF: {
        if (count < 3 || !value(types, n, a[0], NX_BOOL) || a[1] >= f->count) return false;
        unsigned second = 2u + f->blocks[a[1]].count;
        return second < count && edge(f, types, n, a + 1, second - 1) &&
               edge(f, types, n, a + second, count - second);
    }
    case NX_RETURN:
        return count == 1 && value(types, n, a[0], f->signature.result);
    case NX_TRAP: return count == 0;
    default: return false;
    }
}

static bool same_type(const NxType *a, const NxType *b)
{
    return a->kind == b->kind && a->count == b->count &&
           !memcmp(a->entries, b->entries, a->count * sizeof a->entries[0]);
}

static int import_order(const NxImport *a, const NxImport *b)
{
    int order = strcmp(a->interface, b->interface);
    if (order) return order;
    if (a->version != b->version) return a->version < b->version ? -1 : 1;
    return strcmp(a->operation, b->operation);
}

NxStatus nx_verify(const NxBuilder *m, NxDiagnostic *d)
{
    if (!m) return nx_error(d, NX_ARGUMENT, 0, "missing builder");
    if (m->types < 4 || m->types > NX_TYPES || m->constants > NX_CONSTANTS ||
        m->imports > NX_IMPORTS || m->functions > NX_FUNCTIONS ||
        m->exports > NX_EXPORTS || m->instructions > NX_INSNS)
        return nx_error(d, NX_LIMIT, 0, "table limit");
    for (unsigned i = 0; i < m->types; ++i) {
        const NxType *t = &m->type[i];
        if (t->count > NX_ARGS ||
            (i < 4 && (t->kind != i || t->count)) ||
            (i >= 4 && (t->kind < NX_RECORD || t->kind > NX_HANDLE)) ||
            (t->kind == NX_VARIANT && !t->count) ||
            (t->kind == NX_HANDLE && (t->count != 1 || t->entries[0] >= m->imports)))
            return nx_error(d, NX_INVALID, i, "type declaration");
        if (t->kind != NX_HANDLE)
            for (unsigned j = 0; j < t->count; ++j)
                if (t->entries[j] >= i) return nx_error(d, NX_INVALID, i, "type must reference earlier type");
        for (unsigned j = 0; j < i; ++j)
            if (same_type(t, &m->type[j])) return nx_error(d, NX_INVALID, i, "duplicate type");
    }
    for (unsigned i = 0; i < m->constants; ++i) {
        const NxConstant *c = &m->constant[i];
        if (c->type > NX_BYTES || c->length > NX_LITERAL ||
            (c->type == NX_BOOL && (c->length != 1 || c->data[0] > 1)) ||
            (c->type == NX_I64 && c->length != 8) ||
            (c->type == NX_TEXT && !nx_utf8(c->data, c->length)))
            return nx_error(d, NX_INVALID, i, "invalid literal");
        if (i) {
            const NxConstant *p = &m->constant[i - 1];
            int order = p->type != c->type ? (int)p->type - c->type :
                        p->length != c->length ? (int)p->length - c->length :
                        memcmp(p->data, c->data, c->length);
            if (order >= 0) return nx_error(d, NX_INVALID, i, "constant order or duplicate");
        }
    }
    for (unsigned i = 0; i < m->imports; ++i) {
        const NxImport *import = &m->import[i];
        if (!nx_name(import->interface) || !nx_name(import->operation) || !import->version ||
            !signature(m, &import->signature) || !boundary_signature(m, &import->signature, true) ||
            (i && import_order(&m->import[i - 1], import) >= 0))
            return nx_error(d, NX_INVALID, i, "invalid or unordered import");
        const NxType *result = &m->type[import->signature.result];
        if (result->kind == NX_HANDLE && result->entries[0] != i)
            return nx_error(d, NX_INVALID, i, "only the owning import may create a handle");
    }
    for (unsigned i = 0; i < m->functions; ++i) {
        const NxFunction *f = &m->function[i];
        if (!signature(m, &f->signature) || !f->count || f->count > NX_BLOCKS)
            return nx_error(d, NX_INVALID, i, "function signature or block count");
        for (unsigned j = 0; j < f->count; ++j) {
            const NxBlock *b = &f->blocks[j];
            if (b->count > NX_ARGS || !b->length)
                return nx_error(d, NX_INVALID, j, "invalid block");
            for (unsigned k = 0; k < b->count; ++k)
                if (b->types[k] >= m->types)
                    return nx_error(d, NX_INVALID, j, "unknown block parameter type");
        }
        const NxBlock *entry = &f->blocks[0];
        if (entry->count != f->signature.count ||
            memcmp(entry->types, f->signature.types, entry->count * sizeof entry->types[0]))
            return nx_error(d, NX_INVALID, i, "entry signature mismatch");
    }
    unsigned index = 0;
    for (unsigned i = 0; i < m->functions; ++i) {
        const NxFunction *f = &m->function[i];
        for (unsigned j = 0; j < f->count; ++j) {
            const NxBlock *b = &f->blocks[j];
            if (b->first != index || b->length > m->instructions - index)
                return nx_error(d, NX_INVALID, index, "noncontiguous instruction range");
            uint16_t types[NX_LOCALS];
            unsigned n = b->count;
            memcpy(types, b->types, n * sizeof types[0]);
            for (unsigned k = 0; k < b->length; ++k, ++index) {
                const NxInstruction *in = &m->instruction[index];
                bool last = k + 1 == b->length;
                uint16_t result;
                if (in->count > sizeof in->args / sizeof in->args[0] ||
                    nx_terminator(in->op) != last || !instruction(m, f, in, types, n, &result))
                    return nx_error(d, NX_INVALID, index, "invalid instruction, operands or SSA reference");
                if (!last) {
                    if (n == NX_LOCALS) return nx_error(d, NX_LIMIT, index, "local value limit");
                    types[n++] = result;
                }
            }
        }
    }
    if (index != m->instructions) return nx_error(d, NX_INVALID, index, "orphan instructions");
    for (unsigned i = 0; i < m->exports; ++i) {
        const NxExport *e = &m->exported[i];
        if (!nx_name(e->name) || e->function >= m->functions ||
            (i && strcmp(m->exported[i - 1].name, e->name) >= 0) ||
            !boundary_signature(m, &m->function[e->function].signature, false))
            return nx_error(d, NX_INVALID, i, "invalid or unordered export");
    }
    return nx_error(d, NX_OK, 0, "ok");
}
