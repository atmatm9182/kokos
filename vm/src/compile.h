#ifndef COMPILE_H_
#define COMPILE_H_

#include "ast.h"
#include "base.h"
#include "instruction.h"

#include <stdint.h>

DA_DECLARE(kokos_variable_list_t, string_view);

typedef struct kokos_compiler_context {
    hash_table functions;
    kokos_variable_list_t locals;

    struct kokos_compiler_context* parent;
} kokos_compiler_context_t;

typedef struct {
    string_view* names;
    size_t len;
    bool variadic;
} kokos_params_t;

typedef struct {
    kokos_params_t params;
    code_t body;
} kokos_compiled_proc_t;

kokos_compiler_context_t kokos_empty_compiler_context(void);
void kokos_ctx_add_proc(
    kokos_compiler_context_t* ctx, const char* name, kokos_compiled_proc_t* proc);
kokos_compiled_proc_t* kokos_ctx_get_proc(kokos_compiler_context_t* ctx, const char* name);

code_t kokos_expr_compile(const kokos_expr_t* expr, kokos_compiler_context_t* context);

#endif // COMPILE_H_
