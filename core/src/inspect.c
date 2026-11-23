#include "internal.h"
#include <inttypes.h>

static void signature(FILE *out, const NxSignature *sig)
{
    fputc('(', out);
    for (unsigned i = 0; i < sig->count; ++i)
        fprintf(out, "%st%u", i ? ", " : "", sig->types[i]);
    fprintf(out, ") -> t%u", sig->result);
}

void nx_inspect(const NxModule *module, FILE *out)
{
    if (!module || !out) return;
    const NxBuilder *m = &module->ir;
    static const char *const kinds[] = {"bool","i64","text","bytes","record","variant","handle"};
    fputs("NEXIR IR 1.0\n", out);
    for (unsigned i = 0; i < m->types; ++i) {
        const NxType *t = &m->type[i];
        fprintf(out, "type t%u = %s", i, kinds[t->kind]);
        for (unsigned j = 0; j < t->count; ++j)
            fprintf(out, " %s%u", t->kind == NX_HANDLE ? "import" : "t", t->entries[j]);
        fputc('\n', out);
    }
    for (unsigned i = 0; i < m->constants; ++i) {
        const NxConstant *c = &m->constant[i];
        fprintf(out, "const c%u : t%u = ", i, c->type);
        if (c->type == NX_I64) fprintf(out, "%" PRId64, nx_i64(c->data));
        else if (c->type == NX_BOOL) fputs(c->data[0] ? "true" : "false", out);
        else for (unsigned j = 0; j < c->length; ++j) fprintf(out, "%02x", c->data[j]);
        fputc('\n', out);
    }
    for (unsigned i = 0; i < m->imports; ++i) {
        const NxImport *im = &m->import[i];
        fprintf(out, "import %u %s@%u.%s ", i, im->interface, im->version, im->operation);
        signature(out, &im->signature);
        fputc('\n', out);
    }
    for (unsigned i = 0; i < m->functions; ++i) {
        const NxFunction *f = &m->function[i];
        fprintf(out, "fn %u ", i);
        signature(out, &f->signature);
        fputc('\n', out);
        for (unsigned j = 0; j < f->count; ++j) {
            const NxBlock *b = &f->blocks[j];
            fprintf(out, "  block %u", j);
            for (unsigned k = 0; k < b->count; ++k)
                fprintf(out, " %%%u:t%u", k, b->types[k]);
            fputc('\n', out);
            for (unsigned k = 0; k < b->length; ++k) {
                const NxInstruction *in = &m->instruction[b->first + k];
                fputs("    ", out);
                if (!nx_terminator(in->op)) fprintf(out, "%%%u = ", b->count + k);
                fputs(nx_opcode(in->op), out);
                for (unsigned a = 0; a < in->count; ++a) fprintf(out, " %u", in->args[a]);
                fputc('\n', out);
            }
        }
    }
    for (unsigned i = 0; i < m->exports; ++i)
        fprintf(out, "export %s = fn %u\n", m->exported[i].name, m->exported[i].function);
}
