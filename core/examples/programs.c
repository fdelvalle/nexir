#include "programs.h"
#include <string.h>

const char *const nx_example_names[PROGRAMS] = {"calculation", "expense", "panel"};

static void block(NxBuilder *m, NxFunction *f, NxBlock b,
                  const NxInstruction *code, uint16_t count)
{
    b.first = m->instructions;
    b.length = count;
    f->blocks[f->count++] = b;
    memcpy(m->instruction + m->instructions, code, count * sizeof *code);
    m->instructions += count;
}

static void expense(NxBuilder *m)
{
    m->type[m->types++] = (NxType){NX_RECORD, 2, {NX_I64, NX_I64}};
    m->constants = 1;
    nx_constant_i64(&m->constant[0], 0);
    NxFunction *policy = &m->function[m->functions++];
    policy->signature = (NxSignature){NX_BOOL, 1, {4}};
    const NxInstruction evaluate[] = {
        {NX_RECORD_GET, 2, {0, 0}},
        {NX_RECORD_GET, 2, {0, 1}},
        {NX_CONST, 1, {0}},
        {NX_LT, 2, {3, 1}},
        {NX_LT, 2, {2, 1}},
        {NX_NOT, 1, {5}},
        {NX_AND, 2, {4, 6}},
        {NX_RETURN, 1, {7}},
    };
    block(m, policy, (NxBlock){.count = 1, .types = {4}}, evaluate,
          sizeof evaluate / sizeof evaluate[0]);
}

void nx_example(NxBuilder *m, unsigned id)
{
    nx_builder_init(m);
    if (id == EXPENSE || id == PANEL) expense(m);
    NxFunction *main = &m->function[m->functions++];
    uint16_t result = id == CALCULATION ? NX_I64 : NX_BOOL;
    main->signature = (NxSignature){result, 2, {NX_I64, NX_I64}};
    m->exports = 1;
    m->exported[0] = (NxExport){"main", (uint16_t)(m->functions - 1)};
    NxBlock entry = {.count = 2, .types = {NX_I64, NX_I64}};
    if (id == CALCULATION) {
        const NxInstruction code[] = {
            {NX_ADD, 2, {0, 1}}, {NX_MUL, 2, {2, 1}}, {NX_RETURN, 1, {3}},
        };
        block(m, main, entry, code, sizeof code / sizeof code[0]);
        return;
    }
    const NxInstruction decision[] = {
        {NX_RECORD_NEW, 3, {4, 0, 1}}, {NX_CALL, 2, {0, 2}},
        {NX_IF, 5, {3, 1, 3, 2, 3}},
    };
    block(m, main, entry, decision, sizeof decision / sizeof decision[0]);
    const NxBlock outcome = {.count = 1, .types = {NX_BOOL}};
    const NxInstruction return_bool[] = {{NX_RETURN, 1, {0}}};
    const NxInstruction render[] = {{NX_CAP, 2, {0, 0}}, {NX_RETURN, 1, {0}}};
    if (id == PANEL) {
        m->imports = 1;
        m->import[0] = (NxImport){"ui.mock", "publish_decision", 1, {NX_BOOL, 1, {NX_BOOL}}};
        block(m, main, outcome, render, sizeof render / sizeof render[0]);
        block(m, main, outcome, render, sizeof render / sizeof render[0]);
    } else {
        block(m, main, outcome, return_bool, 1);
        block(m, main, outcome, return_bool, 1);
    }
}

static NxStatus ui_callback(void *context, NxResources *resources, const NxValue *args,
                            size_t count, NxValue *result)
{
    (void)resources;
    if (count != 1 || args[0].type != NX_BOOL) return NX_HOST;
    *(bool *)context = args[0].as.boolean;
    *result = (NxValue){.type = NX_BOOL, .as.boolean = true};
    return NX_OK;
}

NxGrant nx_ui_grant(bool *displayed)
{
    return (NxGrant){.interface = "ui.mock", .operation = "publish_decision", .version = 1,
                    .signature = {NX_BOOL, 1, {NX_BOOL}}, .context = displayed, .call = ui_callback};
}
