#ifndef COMPILE_H_
#define COMPILE_H_

#include "ast.h"
#include "instruction.h"
#include "value.h"

#include <stdint.h>

code_t kokos_expr_compile(const kokos_expr_t* expr);

#endif // COMPILE_H_
