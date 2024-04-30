#ifndef INSTRUCTION_H_
#define INSTRUCTION_H_

#include "base.h"
#include <stdint.h>

typedef enum {
    I_PUSH,
    I_POP,
    I_ADD,
    I_PUSH_LOCAL,
    I_CALL,
    I_JNZ,
    I_JZ,
    I_BRANCH
} kokos_instruction_type_e;

typedef struct {
    kokos_instruction_type_e type;
    uint64_t operand;
} kokos_instruction_t;

#define INSTR_PUSH(op) ((kokos_instruction_t) { .type = I_PUSH, .operand = (op) })
#define INSTR_POP(op) ((kokos_instruction_t) { .type = I_POP, .operand = (op) })
#define INSTR_ADD(op) ((kokos_instruction_t) { .type = I_ADD, .operand = (op) })
#define INSTR_PUSH_LOCAL(op) ((kokos_instruction_t) { .type = I_PUSH_LOCAL, .operand = (op) })
#define INSTR_CALL(op) ((kokos_instruction_t) { .type = I_CALL, .operand = (op) })
#define INSTR_JZ(op) ((kokos_instruction_t) { .type = I_JZ, .operand = (op) })
#define INSTR_JNZ(op) ((kokos_instruction_t) { .type = I_JNZ, .operand = (op) })
#define INSTR_BRANCH(op) ((kokos_instruction_t) { .type = I_BRANCH, .operand = (op) })

typedef struct {
    kokos_instruction_t* items;
    size_t len;
    size_t cap;
} kokos_code_t;

void kokos_code_dump(kokos_code_t code);

#endif // INSTRUCTION_H_
