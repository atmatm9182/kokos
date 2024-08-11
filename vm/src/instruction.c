#include "instruction.h"
#include "macros.h"

char const* kokos_instruction_type_str(kokos_instruction_type_e type)
{
    switch (type) {
    case I_PUSH:        return "push";
    case I_POP:         return "pop";
    case I_ADD:         return "add";
    case I_SUB:         return "sub";
    case I_MUL:         return "mul";
    case I_DIV:         return "div";
    case I_PUSH_LOCAL:  return "push_local";
    case I_STORE_LOCAL: return "store_local";
    case I_CALL:        return "call";
    case I_JZ:          return "jz";
    case I_JNZ:         return "jnz";
    case I_BRANCH:      return "branch";
    case I_CALL_NATIVE: return "call_native";
    case I_CMP:         return "cmp";
    case I_EQ:          return "eq";
    case I_NEQ:         return "neq";
    case I_RET:         return "ret";
    case I_ALLOC:       return "alloc";
    default:            KOKOS_TODO();
    }
}

void kokos_instruction_dump(kokos_instruction_t instruction)
{
    char const* type = kokos_instruction_type_str(instruction.type);

    switch (instruction.type) {
    case I_PUSH:        printf("%s 0x%lx", type, instruction.operand); break;
    case I_POP:         printf("%s", type); break;
    case I_ADD:         printf("%s %lu", type, instruction.operand); break;
    case I_MUL:         printf("%s %lu", type, instruction.operand); break;
    case I_DIV:         printf("%s %lu", type, instruction.operand); break;
    case I_SUB:         printf("%s %lu", type, instruction.operand); break;
    case I_PUSH_LOCAL:  printf("%s %lu", type, instruction.operand); break;
    case I_STORE_LOCAL: printf("%s %lu", type, instruction.operand); break;
    case I_CALL:        printf("%s 0x%lx", type, instruction.operand); break;
    case I_JZ:          printf("%s %ld", type, (int64_t)instruction.operand); break;
    case I_JNZ:         printf("%s %ld", type, (int64_t)instruction.operand); break;
    case I_BRANCH:      printf("%s %ld", type, (int64_t)instruction.operand); break;
    case I_CALL_NATIVE: printf("%s 0x%lx", type, instruction.operand); break;
    case I_CMP:         printf("%s", type); break;
    case I_EQ:          printf("%s 0x%lx", type, instruction.operand); break;
    case I_NEQ:         printf("%s 0x%lx", type, instruction.operand); break;
    case I_RET:         printf("%s", type); break;
    case I_ALLOC:       printf("%s 0x%lx", type, instruction.operand); break;
    default:            KOKOS_TODO();
    }
}

void kokos_code_dump(kokos_code_t code)
{
    for (size_t i = 0; i < code.len; i++) {
        kokos_instruction_t instr = code.items[i];

        printf("[%lu] ", i);
        kokos_instruction_dump(instr);
        printf("\n");
    }
}
