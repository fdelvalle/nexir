#ifndef NEXIR_PROGRAMS_H
#define NEXIR_PROGRAMS_H
#include "nexir.h"

enum { CALCULATION, EXPENSE, PANEL, PROGRAMS };
extern const char *const nx_example_names[PROGRAMS];
void nx_example(NxBuilder *m, unsigned id);
NxGrant nx_ui_grant(bool *displayed);

#endif
