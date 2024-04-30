#include "instruction.h"

void kokos_code_dump(kokos_code_t code)
{
    for (size_t i = 0; i < code.len; i++) {
        kokos_instruction_t instr = code.items[i];
        switch (instr.type) {
        case I_PUSH:       printf("push %lu", instr.operand); break;
        case I_POP:        printf("pop"); break;
        case I_ADD:        printf("add %lu", instr.operand); break;
        case I_PUSH_LOCAL: printf("push_local %lu", instr.operand); break;
        case I_CALL:       printf("call %lu", instr.operand); break;
        }
        printf("\n");
    }
}
