#include "compile.h"

static uint64_t to_double_bytes(const kokos_expr_t* expr)
{
    double parsed = sv_atof(expr->token.value);
    return TO_VALUE(parsed).as_int;
}

code_t kokos_expr_compile(const kokos_expr_t* expr)
{
    code_t code;
    DA_INIT(&code, 0, 1);

    switch (expr->type) {
    case EXPR_FLOAT_LIT:
    case EXPR_INT_LIT:   {
        uint64_t value = to_double_bytes(expr);
        DA_ADD(&code, INSTR_PUSH(value));
        break;
    }
    case EXPR_LIST: {
        kokos_list_t list = expr->list;
        KOKOS_VERIFY(list.len > 0);
        string_view head = list.items[0]->token.value;

        KOKOS_VERIFY(sv_eq_cstr(head, "+"));

        for (size_t i = 1; i < list.len; i++) {
            const kokos_expr_t* elem = list.items[i];

            uint64_t num = to_double_bytes(elem);
            DA_ADD(&code, INSTR_PUSH(num));
        }

        DA_ADD(&code, INSTR_ADD(list.len - 1));
        break;
    }
    default: KOKOS_TODO();
    }

    return code;
}
