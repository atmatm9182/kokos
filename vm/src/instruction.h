#ifndef INSTRUCTION_H_
#define INSTRUCTION_H_

#include "base.h"
#include <stdint.h>

typedef enum { I_PUSH, I_POP, I_ADD, I_PUSH_LOCAL, I_CALL } instruction_type_e;

typedef struct {
    instruction_type_e type;
    uint64_t operand;
} instruction_t;

#define INSTR_PUSH(op) ((instruction_t) { .type = I_PUSH, .operand = (op) })
#define INSTR_POP(op) ((instruction_t) { .type = I_POP, .operand = (op) })
#define INSTR_ADD(op) ((instruction_t) { .type = I_ADD, .operand = (op) })
#define INSTR_PUSH_LOCAL(op) ((instruction_t) { .type = I_PUSH_LOCAL, .operand = (op) })
#define INSTR_CALL(op) ((instruction_t) { .type = I_CALL, .operand = (op) })

DA_DECLARE(code_t, instruction_t);

void kokos_code_dump(code_t code);

#endif // INSTRUCTION_H_
