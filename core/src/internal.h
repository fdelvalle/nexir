#ifndef NEXIR_INTERNAL_H
#define NEXIR_INTERNAL_H
#include "nexir.h"

struct NxModule { NxBuilder ir; };

NxStatus nx_error(NxDiagnostic *d, NxStatus status, size_t offset, const char *message);
bool nx_utf8(const uint8_t *data, size_t length);
bool nx_name(const char name[NX_NAME + 1]);
int64_t nx_i64(const uint8_t bytes[8]);
const char *nx_opcode(unsigned op);
bool nx_terminator(unsigned op);

#endif
