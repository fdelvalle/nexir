#include "internal.h"
#include <string.h>

NxStatus nx_error(NxDiagnostic *d, NxStatus status, size_t offset, const char *message)
{
    if (d) *d = (NxDiagnostic){status, offset, message};
    return status;
}

const char *nx_status(NxStatus status)
{
    static const char *const names[] = {
        "ok", "invalid encoding", "verification failed", "resource limit",
        "out of memory", "capability denied", "invalid invocation argument",
        "host contract failure", "integer overflow", "division by zero",
        "wrong variant alternative", "explicit trap"
    };
    return (unsigned)status < sizeof names / sizeof names[0] ? names[status] : "unknown error";
}

const char *nx_opcode(unsigned op)
{
    static const char *const names[] = {
        "invalid", "const", "record.new", "record.get", "variant.new",
        "variant.tag", "variant.get", "i64.add", "i64.sub", "i64.mul",
        "i64.div", "i64.eq", "i64.lt", "bool.not", "bool.and", "bool.or",
        "call", "call.cap", "br", "br.if", "return", "trap"
    };
    return op < sizeof names / sizeof names[0] ? names[op] : names[0];
}

bool nx_terminator(unsigned op) { return op >= NX_BR && op <= NX_TRAP; }

void nx_builder_init(NxBuilder *builder)
{
    memset(builder, 0, sizeof *builder);
    builder->types = 4;
    for (uint16_t i = 0; i < 4; ++i) builder->type[i].kind = i;
}

bool nx_name(const char name[NX_NAME + 1])
{
    for (size_t i = 0; i <= NX_NAME; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (!c) return i != 0;
        if (!(c >= 'a' && c <= 'z') &&
            !(i && ((c >= '0' && c <= '9') || c == '.' || c == '_'))) return false;
    }
    return false;
}

bool nx_utf8(const uint8_t *s, size_t length)
{
    if (!s && length) return false;
    for (size_t i = 0; i < length;) {
        uint8_t first = s[i++];
        if (first < 0x80) continue;
        unsigned more;
        uint32_t value, minimum;
        if (first >= 0xc2 && first <= 0xdf) {
            more = 1; value = first & 0x1f; minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            more = 2; value = first & 0x0f; minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            more = 3; value = first & 7; minimum = 0x10000;
        } else return false;
        if (length - i < more) return false;
        for (unsigned j = 0; j < more; ++j) {
            uint8_t next = s[i++];
            if ((next & 0xc0) != 0x80) return false;
            value = (value << 6) | (next & 0x3f);
        }
        if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            return false;
    }
    return true;
}

int64_t nx_i64(const uint8_t bytes[8])
{
    uint64_t raw = 0;
    for (unsigned i = 0; i < 8; ++i) raw |= (uint64_t)bytes[i] << (8 * i);
    return raw <= INT64_MAX ? (int64_t)raw : -1 - (int64_t)(UINT64_MAX - raw);
}

void nx_constant_i64(NxConstant *constant, int64_t value)
{
    constant->type = NX_I64;
    constant->length = 8;
    for (unsigned i = 0; i < 8; ++i)
        constant->data[i] = (uint8_t)((uint64_t)value >> (8 * i));
}
