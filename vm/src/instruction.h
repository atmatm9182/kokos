#ifndef INSTRUCTION_H_
#define INSTRUCTION_H_

#include "base.h"
#include <stdint.h>

typedef enum {
    I_PUSH = 0,
    I_POP = 1,
    I_ADD = 2,
    I_SUB = 3,
    I_MUL = 4,
    I_DIV = 5,
    I_GET_LOCAL = 6,
    I_ADD_LOCAL = 7,
    I_CALL = 8,
    I_JNZ = 9,
    I_JZ = 10,
    I_BRANCH = 11,
    I_CMP = 12,
    I_EQ = 13,
    I_NEQ = 14,
    I_RET = 15,
    I_ALLOC = 16,
    I_PUSH_SCOPE = 17,
    I_POP_SCOPE = 18,
} kokos_instruction_type_e;

typedef struct {
    kokos_instruction_type_e type;
    uint64_t operand;
} kokos_instruction_t;

#define INSTR_PUSH(op) ((kokos_instruction_t) { .type = I_PUSH, .operand = (op).as_int })
#define INSTR_POP(op) ((kokos_instruction_t) { .type = I_POP, .operand = (op) })
#define INSTR_ADD(op) ((kokos_instruction_t) { .type = I_ADD, .operand = (op) })
#define INSTR_MUL(op) ((kokos_instruction_t) { .type = I_MUL, .operand = (op) })
#define INSTR_DIV(op) ((kokos_instruction_t) { .type = I_DIV, .operand = (op) })
#define INSTR_SUB(op) ((kokos_instruction_t) { .type = I_SUB, .operand = (op) })
#define INSTR_GET_LOCAL(name)                                                                      \
    ((kokos_instruction_t) { .type = I_GET_LOCAL, .operand = (uintptr_t)(name) })
#define INSTR_ADD_LOCAL(name)                                                                      \
    ((kokos_instruction_t) { .type = I_ADD_LOCAL, .operand = (uintptr_t)(name) })
#define INSTR_CALL(name, nargs)                                                                    \
    ((kokos_instruction_t) { .type = I_CALL, .operand = (nargs) << 48 | (uintptr_t)name })
#define INSTR_JZ(op) ((kokos_instruction_t) { .type = I_JZ, .operand = (size_t)(op) })
#define INSTR_JNZ(op) ((kokos_instruction_t) { .type = I_JNZ, .operand = (size_t)(op) })
#define INSTR_BRANCH(op) ((kokos_instruction_t) { .type = I_BRANCH, .operand = (size_t)(op) })
#define INSTR_EQ(op) ((kokos_instruction_t) { .type = I_EQ, .operand = (op) })
#define INSTR_NEQ(op) ((kokos_instruction_t) { .type = I_NEQ, .operand = (op) })

#define INSTR_SFORM(op) ((kokos_instruction_t) { .type = I_SFORM, .operand = (op) })

#define INSTR_CMP ((kokos_instruction_t) { .type = I_CMP })
#define INSTR_RET ((kokos_instruction_t) { .type = I_RET })

#define INSTR_PUSH_SCOPE(count) ((kokos_instruction_t) { .type = I_PUSH_SCOPE, .operand = (count) })
#define INSTR_POP_SCOPE ((kokos_instruction_t) { .type = I_POP_SCOPE })

#define INSTR_ALLOC(t, count) ((kokos_instruction_t) { .type = I_ALLOC, .operand = (t) | (count) })

#define INSTR_ALLOC_ARG_MASK 0xFFFFFFFF

typedef struct {
    kokos_instruction_t* items;
    size_t len;
    size_t cap;
} kokos_code_t;

void kokos_instruction_dump(kokos_instruction_t instruction);
char const* kokos_instruction_type_str(kokos_instruction_type_e type);

void kokos_code_dump(kokos_code_t code);

#endif // INSTRUCTION_H_
