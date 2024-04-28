#ifndef COMPILE_H_
#define COMPILE_H_

#include "src/ast.h"
#include "src/instruction.h"
#include "src/value.h"
#include <stdint.h>

static uint64_t to_double_bytes(const kokos_expr_t* expr)
{
    double parsed = sv_atof(expr->token.value);
    return TO_VALUE(parsed).as_int;
}

code_t kokos_expr_compile(const kokos_expr_t* expr);

#endif // COMPILE_H_
