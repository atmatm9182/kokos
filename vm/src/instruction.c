#include "instruction.h"

void kokos_instruction_dump(kokos_instruction_t instruction)
{

    switch (instruction.type) {
    case I_PUSH:        printf("push %lu", instruction.operand); break;
    case I_POP:         printf("pop"); break;
    case I_ADD:         printf("add %lu", instruction.operand); break;
    case I_PUSH_LOCAL:  printf("push_local %lu", instruction.operand); break;
    case I_CALL:        printf("call %s", (const char*)instruction.operand); break;
    case I_JZ:          printf("jz %ld", (int64_t)instruction.operand); break;
    case I_JNZ:         printf("jnz %ld", (int64_t)instruction.operand); break;
    case I_BRANCH:      printf("branch %ld", (int64_t)instruction.operand); break;
    case I_SFORM:       printf("sform %s", (const char*)instruction.operand); break;
    case I_CALL_NATIVE: printf("call_native %lux", instruction.operand); break;
    }
}

void kokos_code_dump(kokos_code_t code)
{
    for (size_t i = 0; i < code.len; i++) {
        kokos_instruction_t instr = code.items[i];
        kokos_instruction_dump(instr);
        printf("\n");
    }
}
