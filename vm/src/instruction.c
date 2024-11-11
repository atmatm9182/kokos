#include "instruction.h"
#include "macros.h"
#include "runtime.h"
#include "src/value.h"

char const* kokos_instruction_type_str(kokos_instruction_type_e type)
{
    switch (type) {
    case I_PUSH:        return "push";
    case I_POP:         return "pop";
    case I_ADD:         return "add";
    case I_SUB:         return "sub";
    case I_MUL:         return "mul";
    case I_DIV:         return "div";
    case I_GET_LOCAL:   return "get_local";
    case I_ADD_LOCAL:   return "add_local";
    case I_CALL:        return "call";
    case I_JZ:          return "jz";
    case I_JNZ:         return "jnz";
    case I_BRANCH:      return "branch";
    case I_CMP:         return "cmp";
    case I_EQ:          return "eq";
    case I_NEQ:         return "neq";
    case I_RET:         return "ret";
    case I_ALLOC:       return "alloc";
    case I_PUSH_SCOPE:  return "push_scope";
    case I_POP_SCOPE:   return "pop_scope";
    default:            {
        char buf[512];
        sprintf(buf, "printing of instruction type %d", type);
        KOKOS_TODO(buf);
    }
    }
}

void kokos_instruction_dump(kokos_instruction_t instruction)
{
    char const* type = kokos_instruction_type_str(instruction.type);

    printf("%s", type);

    switch (instruction.type) {
    case I_CMP:
    case I_RET:
    case I_POP_SCOPE:
    case I_POP:       break;

    case I_CALL:
    case I_GET_LOCAL:
    case I_ADD_LOCAL: {
        printf(" " RT_STRING_FMT, RT_STRING_ARG(*GET_STRING_INT(instruction.operand)));
        break;
    }

    case I_PUSH:
    case I_EQ:
    case I_NEQ:  {
        printf(" ");
        kokos_value_print(TO_VALUE(instruction.operand));
        break;
    }

    case I_ALLOC: {
        switch (GET_TAG(instruction.operand)) {
        case VECTOR_TAG: printf(" vector"); break;
        case STRING_TAG: printf(" string"); break;
        case LIST_TAG: printf(" list"); break;
        case PROC_TAG: printf(" proc"); break;
        case MAP_TAG: printf(" map"); break;
        default:
            KOKOS_TODO("unknown alloc tag");
        }

        uint32_t arg = instruction.operand & INSTR_ALLOC_ARG_MASK;
        printf(" %d", arg);

        break;
    }

    case I_ADD:
    case I_MUL:
    case I_DIV:
    case I_SUB:
    case I_JZ:
    case I_JNZ:
    case I_PUSH_SCOPE:
    case I_BRANCH:      printf(" %lu", instruction.operand); break;
    default:            {
        char buf[512];
        sprintf(buf, "printing of instruction type %d", instruction.type);
        KOKOS_TODO(buf);
    }
    }
}

void kokos_code_dump(kokos_code_t code)
{
    size_t len = code.len;
    size_t chars = 1;
    while (len > 10) {
        chars++;
        len /= 10;
    }

    for (size_t i = 0; i < code.len; i++) {
        kokos_instruction_t instr = code.items[i];
        size_t count = printf("[%lu] ", i) - 3;

        // padding from [%lu] to the instruction representation
        for (; count < chars; count++) {
            printf(" ");
        }

        kokos_instruction_dump(instr);
        printf("\n");
    }
}
