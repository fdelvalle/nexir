#include "internal.h"
#include <stdlib.h>
#include <string.h>

typedef struct { uint16_t type, object; NxValue scalar; } Value;
typedef struct { Value field[NX_ARGS]; uint16_t tag; } Object;
typedef struct { void *value; NxDrop drop; uint16_t type; } Handle;
typedef struct {
    const NxModule *module;
    const NxGrant *grant[NX_IMPORTS];
    uint32_t fuel;
    size_t objects, handles, bytes;
    Object object[NX_OBJECTS];
    Handle handle[NX_HANDLES];
    uint8_t storage[65536];
} Machine;

struct NxResources {
    Machine *vm;
    unsigned import;
    const NxValue *args;
    size_t count;
};

NxStatus nx_resource_create(NxResources *r, uint16_t type, void *value,
                            NxDrop drop, NxValue *result)
{
    if (!r || !value || !drop || !result) return NX_ARGUMENT;
    Machine *vm = r->vm;
    const NxBuilder *m = &vm->module->ir;
    if (type >= m->types || m->type[type].kind != NX_HANDLE ||
        m->type[type].entries[0] != r->import ||
        m->import[r->import].signature.result != type) return NX_PERMISSION;
    if (vm->handles == NX_HANDLES) return NX_LIMIT;
    for (size_t i = 0; i < vm->handles; ++i)
        if (vm->handle[i].value == value) return NX_ARGUMENT;
    size_t id = vm->handles++;
    vm->handle[id] = (Handle){value, drop, type};
    *result = (NxValue){.type = type, .as.handle = (uint16_t)id};
    return NX_OK;
}

void *nx_resource_get(NxResources *r, NxValue value)
{
    if (!r || value.type >= r->vm->module->ir.types ||
        r->vm->module->ir.type[value.type].kind != NX_HANDLE) return NULL;
    size_t id = value.as.handle;
    if (id >= r->vm->handles || r->vm->handle[id].type != value.type) return NULL;
    for (size_t i = 0; i < r->count; ++i)
        if (r->args[i].type == value.type && r->args[i].as.handle == id)
            return r->vm->handle[id].value;
    return NULL;
}

static Value scalar(NxValue value)
{
    return (Value){.type = value.type, .scalar = value};
}

static Value integer(int64_t n)
{
    return scalar((NxValue){.type = NX_I64, .as.integer = n});
}

static Value boolean(bool b)
{
    return scalar((NxValue){.type = NX_BOOL, .as.boolean = b});
}

static bool valid_value(NxValue v, uint16_t type)
{
    if (v.type != type) return false;
    if (type == NX_TEXT || type == NX_BYTES) {
        NxSlice s = v.as.slice;
        return s.length <= NX_LITERAL && (!s.length || s.data) &&
               (type != NX_TEXT || nx_utf8(s.data, s.length));
    }
    return true;
}

static Value constant(const NxConstant *c)
{
    switch (c->type) {
    case NX_BOOL: return boolean(c->data[0] != 0);
    case NX_I64: return integer(nx_i64(c->data));
    default: return scalar((NxValue){.type = c->type, .as.slice = {c->data, c->length}});
    }
}

static NxStatus arithmetic(unsigned op, int64_t a, int64_t b, Value *out)
{
    int64_t n;
    switch (op) {
    case NX_EQ: *out = boolean(a == b); return NX_OK;
    case NX_LT: *out = boolean(a < b); return NX_OK;
    case NX_ADD:
        if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b))
            return NX_OVERFLOW;
        n = a + b;
        break;
    case NX_SUB:
        if ((b > 0 && a < INT64_MIN + b) || (b < 0 && a > INT64_MAX + b))
            return NX_OVERFLOW;
        n = a - b;
        break;
    case NX_MUL:
        if ((a > 0 && (b > INT64_MAX / a || b < INT64_MIN / a)) ||
            (a < 0 && ((b > 0 && a < INT64_MIN / b) ||
                       (b < 0 && a < INT64_MAX / b)))) return NX_OVERFLOW;
        n = a * b;
        break;
    default:
        if (!b) return NX_DIV_ZERO;
        if (a == INT64_MIN && b == -1) return NX_OVERFLOW;
        n = a / b;
        break;
    }
    *out = integer(n);
    return NX_OK;
}

static void arguments(Value *out, const Value *locals, const uint16_t *ids, unsigned count)
{
    for (unsigned i = 0; i < count; ++i) out[i] = locals[ids[i]];
}

static NxStatus capability(Machine *vm, unsigned id, const Value *args, Value *result)
{
    const NxImport *im = &vm->module->ir.import[id];
    const NxGrant *grant = vm->grant[id];
    NxValue input[NX_ARGS], output = {0};
    for (unsigned i = 0; i < im->signature.count; ++i) input[i] = args[i].scalar;
    size_t first_handle = vm->handles;
    NxResources resources = {vm, id, input, im->signature.count};
    NxStatus status = grant->call(grant->context, &resources, input, im->signature.count, &output);
    if (status != NX_OK)
        return status == NX_PERMISSION || status == NX_LIMIT ? status : NX_HOST;
    uint16_t type = im->signature.result;
    if (!valid_value(output, type)) return NX_HOST;
    if (vm->module->ir.type[type].kind == NX_HANDLE) {
        size_t index = output.as.handle;
        if (index < first_handle || index >= vm->handles || vm->handle[index].type != type)
            return NX_HOST;
    }
    if (type == NX_TEXT || type == NX_BYTES) {
        size_t n = output.as.slice.length;
        if (n > sizeof vm->storage - vm->bytes) return NX_LIMIT;
        uint8_t *copy = vm->storage + vm->bytes;
        if (n) memcpy(copy, output.as.slice.data, n);
        output.as.slice.data = copy;
        vm->bytes += n;
    }
    *result = scalar(output);
    return NX_OK;
}

static NxStatus execute(Machine *vm, unsigned function, const Value *args,
                        unsigned depth, Value *result)
{
    if (depth == NX_FRAMES) return NX_LIMIT;
    const NxBuilder *m = &vm->module->ir;
    const NxFunction *f = &m->function[function];
    Value locals[NX_LOCALS];
    unsigned block = 0;
    memcpy(locals, args, f->signature.count * sizeof locals[0]);
    for (;;) {
        const NxBlock *b = &f->blocks[block];
        unsigned n = b->count;
        for (unsigned i = 0; i < b->length; ++i) {
            if (!vm->fuel) return NX_LIMIT;
            --vm->fuel;
            const NxInstruction *in = &m->instruction[b->first + i];
            const uint16_t *a = in->args;
            Value value = {0};
            NxStatus status = NX_OK;
            switch (in->op) {
            case NX_CONST:
                value = constant(&m->constant[a[0]]);
                break;
            case NX_RECORD_NEW: case NX_VARIANT_NEW: {
                if (vm->objects == NX_OBJECTS) return NX_LIMIT;
                value.type = a[0];
                value.object = (uint16_t)vm->objects++;
                Object *object = &vm->object[value.object];
                if (in->op == NX_RECORD_NEW)
                    arguments(object->field, locals, a + 1, in->count - 1);
                else {
                    object->tag = a[1];
                    object->field[0] = locals[a[2]];
                }
                break;
            }
            case NX_RECORD_GET:
                value = vm->object[locals[a[0]].object].field[a[1]];
                break;
            case NX_VARIANT_GET: {
                const Object *object = &vm->object[locals[a[0]].object];
                if (object->tag != a[1]) return NX_WRONG_TAG;
                value = object->field[0];
                break;
            }
            case NX_VARIANT_TAG:
                value = integer(vm->object[locals[a[0]].object].tag);
                break;
            case NX_ADD: case NX_SUB: case NX_MUL: case NX_DIV: case NX_EQ: case NX_LT:
                status = arithmetic(in->op, locals[a[0]].scalar.as.integer,
                                    locals[a[1]].scalar.as.integer, &value);
                break;
            case NX_NOT:
                value = boolean(!locals[a[0]].scalar.as.boolean);
                break;
            case NX_AND: case NX_OR: {
                bool a0 = locals[a[0]].scalar.as.boolean, a1 = locals[a[1]].scalar.as.boolean;
                value = boolean(in->op == NX_AND ? a0 && a1 : a0 || a1);
                break;
            }
            case NX_CALL: case NX_CAP: {
                Value next[NX_ARGS];
                arguments(next, locals, a + 1, in->count - 1);
                status = in->op == NX_CAP ? capability(vm, a[0], next, &value) :
                         execute(vm, a[0], next, depth + 1, &value);
                break;
            }
            case NX_IF: case NX_BR: {
                const uint16_t *edge = a;
                if (in->op == NX_IF)
                    edge = a + (locals[a[0]].scalar.as.boolean ? 1 : 2 + f->blocks[a[1]].count);
                block = edge[0];
                Value next[NX_ARGS];
                unsigned count = f->blocks[block].count;
                arguments(next, locals, edge + 1, count);
                memcpy(locals, next, count * sizeof locals[0]);
                break;
            }
            case NX_RETURN:
                *result = locals[a[0]];
                return NX_OK;
            case NX_TRAP:
                return NX_TRAPPED;
            default:
                return NX_INVALID;
            }
            if (status != NX_OK) return status;
            if (!nx_terminator(in->op)) locals[n++] = value;
        }
    }
}

static bool same_signature(const NxSignature *a, const NxSignature *b)
{
    return a->result == b->result && a->count == b->count &&
           !memcmp(a->types, b->types, a->count * sizeof a->types[0]);
}

static bool owners_match(const NxBuilder *m, const NxSignature *s, const NxGrant *grant)
{
    for (unsigned i = 0; i < s->count; ++i) {
        const NxType *type = &m->type[s->types[i]];
        if (type->kind != NX_HANDLE) continue;
        const NxImport *owner = &m->import[type->entries[0]];
        const NxResourceOwner *expected = &grant->argument_owner[i];
        if (!expected->interface || !expected->operation || expected->version != owner->version ||
            strcmp(expected->interface, owner->interface) ||
            strcmp(expected->operation, owner->operation)) return false;
    }
    return true;
}

static NxStatus bind(Machine *vm, const NxGrant *grants, size_t count)
{
    const NxBuilder *m = &vm->module->ir;
    for (unsigned i = 0; i < m->imports; ++i) {
        const NxImport *im = &m->import[i];
        for (size_t j = 0; j < count; ++j) {
            const NxGrant *g = &grants[j];
            if (!g->interface || !g->operation || !g->call || g->version != im->version ||
                strcmp(g->interface, im->interface) || strcmp(g->operation, im->operation) ||
                !same_signature(&im->signature, &g->signature) ||
                !owners_match(m, &im->signature, g)) continue;
            if (vm->grant[i]) return NX_PERMISSION;
            vm->grant[i] = g;
        }
        if (!vm->grant[i]) return NX_PERMISSION;
    }
    return NX_OK;
}

NxStatus nx_run(const NxModule *m, const char *name, const NxValue *args,
                size_t count, const NxGrant *grants, size_t grant_count,
                uint32_t fuel, NxResult *result)
{
    if (!m || !name || !result || (count && !args) || (grant_count && !grants))
        return NX_ARGUMENT;
    if (fuel > NX_MAX_FUEL || grant_count > NX_IMPORTS) return NX_LIMIT;
    unsigned id = m->ir.functions;
    for (unsigned i = 0; i < m->ir.exports; ++i)
        if (!strcmp(m->ir.exported[i].name, name)) id = m->ir.exported[i].function;
    if (id == m->ir.functions) return NX_ARGUMENT;
    const NxSignature *sig = &m->ir.function[id].signature;
    if (count != sig->count) return NX_ARGUMENT;
    Value input[NX_ARGS], output;
    for (unsigned i = 0; i < count; ++i) {
        if (!valid_value(args[i], sig->types[i])) return NX_ARGUMENT;
        input[i] = scalar(args[i]);
    }
    Machine *vm = calloc(1, sizeof *vm);
    if (!vm) return NX_MEMORY;
    vm->module = m;
    vm->fuel = fuel;
    NxStatus status = bind(vm, grants, grant_count);
    if (status == NX_OK) status = execute(vm, id, input, 0, &output);
    if (status == NX_OK) {
        result->value = output.scalar;
        if (output.type == NX_BYTES || output.type == NX_TEXT) {
            NxSlice s = output.scalar.as.slice;
            if (s.length) memmove(result->slice, s.data, s.length);
            result->value.as.slice.data = result->slice;
        }
    }
    while (vm->handles) {
        Handle *h = &vm->handle[--vm->handles];
        h->drop(h->value);
    }
    free(vm);
    return status;
}
