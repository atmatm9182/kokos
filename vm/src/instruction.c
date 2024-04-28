#include "instruction.h"

void kokos_code_dump(code_t code)
{
    for (size_t i = 0; i < code.len; i++) {
        instruction_t instr = code.items[i];
        switch (instr.type) {
        case I_PUSH: printf("push %lu", instr.operand); break;
        case I_POP:  printf("pop"); break;
        case I_ADD:  printf("add %lu", instr.operand); break;
        }
        printf("\n");
    }
}

