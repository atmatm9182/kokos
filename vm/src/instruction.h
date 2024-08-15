#ifndef INSTRUCTION_H_
#define INSTRUCTION_H_

#include "base.h"
#include <stdint.h>

typedef enum {
    I_PUSH,
    I_POP,
    I_ADD,
    I_SUB,
    I_MUL,
    I_DIV,
    I_PUSH_LOCAL,
    I_STORE_LOCAL,
    I_CALL,
    I_JNZ,
    I_JZ,
    I_BRANCH,
    I_CALL_NATIVE,
    I_CMP,
    I_EQ,
    I_NEQ,
    I_RET,
    I_ALLOC,
} kokos_instruction_type_e;

typedef struct {
    kokos_instruction_type_e type;
    uint64_t operand;
} kokos_instruction_t;

#define INSTR_PUSH(op) ((kokos_instruction_t) { .type = I_PUSH, .operand = (op) })
#define INSTR_POP(op) ((kokos_instruction_t) { .type = I_POP, .operand = (op) })
#define INSTR_ADD(op) ((kokos_instruction_t) { .type = I_ADD, .operand = (op) })
#define INSTR_MUL(op) ((kokos_instruction_t) { .type = I_MUL, .operand = (op) })
#define INSTR_DIV(op) ((kokos_instruction_t) { .type = I_DIV, .operand = (op) })
#define INSTR_SUB(op) ((kokos_instruction_t) { .type = I_SUB, .operand = (op) })
#define INSTR_PUSH_LOCAL(hops, idx)                                                                \
    ((kokos_instruction_t) {                                                                       \
        .type = I_PUSH_LOCAL, .operand = ((uint64_t)(hops) << 32) | (uint64_t)(idx) })
#define INSTR_STORE_LOCAL(op) ((kokos_instruction_t) { .type = I_STORE_LOCAL, .operand = (op) })
#define INSTR_CALL(arity, proc)                                                                    \
    ((kokos_instruction_t) { .type = I_CALL, .operand = ((arity) << 32 | (proc)) })
#define INSTR_JZ(op) ((kokos_instruction_t) { .type = I_JZ, .operand = (op) })
#define INSTR_JNZ(op) ((kokos_instruction_t) { .type = I_JNZ, .operand = (op) })
#define INSTR_BRANCH(op) ((kokos_instruction_t) { .type = I_BRANCH, .operand = (op) })
#define INSTR_EQ(op) ((kokos_instruction_t) { .type = I_EQ, .operand = (op) })
#define INSTR_NEQ(op) ((kokos_instruction_t) { .type = I_NEQ, .operand = (op) })

#define INSTR_SFORM(op) ((kokos_instruction_t) { .type = I_SFORM, .operand = (op) })
#define INSTR_CALL_NATIVE(nargs, op)                                                               \
    ((kokos_instruction_t) { .type = I_CALL_NATIVE, .operand = (uint64_t)(nargs) << 48 | (op) })

#define INSTR_CMP ((kokos_instruction_t) { .type = I_CMP })
#define INSTR_RET ((kokos_instruction_t) { .type = I_RET })

#define INSTR_ALLOC(t, count) ((kokos_instruction_t) { .type = I_ALLOC, .operand = (t) | (count) })

typedef struct {
    kokos_instruction_t* items;
    size_t len;
    size_t cap;
} kokos_code_t;

void kokos_instruction_dump(kokos_instruction_t instruction);
void kokos_code_dump(kokos_code_t code);
char const* kokos_instruction_type_str(kokos_instruction_type_e type);

#endif // INSTRUCTION_H_
