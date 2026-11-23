#ifndef NX_APP_BINARY_H
#define NX_APP_BINARY_H
#include <stdint.h>

static inline uint16_t u16(const uint8_t*p){return (uint16_t)(p[0]|(p[1]<<8));}
static inline int64_t i64(const uint8_t*p){uint64_t v=0;for(int i=0;i<8;i++)v|=(uint64_t)p[i]<<(8*i);return v <= INT64_MAX ? (int64_t)v : -1 - (int64_t)(UINT64_MAX - v);}
#define OP_LOAD 1
#define OP_INPUT 2
#define OP_ADD 3
#define OP_SUB 4
#define OP_CHECK_POS 5
#define OP_CHECK_LE 6
#define OP_STORE 7
#define OP_STATUS 8
#define OP_CLEAR 9
#define OP_PUSH 10
#define OP_EQ 11
#define OP_JZ 12
#define OP_JMP 13
#define OP_HALT 255

#endif
