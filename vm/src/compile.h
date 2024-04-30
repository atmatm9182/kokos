#ifndef COMPILE_H_
#define COMPILE_H_

#include "ast.h"
#include "base.h"
#include "instruction.h"

#include <stdint.h>

typedef struct {
    string_view* items;
    size_t len;
    size_t cap;
} kokos_variable_list_t;

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
    const char* name;
    kokos_params_t params;
    kokos_code_t body;
} kokos_compiled_proc_t;

kokos_compiler_context_t kokos_ctx_empty(void);
void kokos_ctx_add_proc(
    kokos_compiler_context_t* ctx, const char* name, kokos_compiled_proc_t* proc);
kokos_compiled_proc_t* kokos_ctx_get_proc(kokos_compiler_context_t* ctx, const char* name);

void kokos_expr_compile(const kokos_expr_t* expr, kokos_compiler_context_t* context, kokos_code_t* code);
kokos_code_t kokos_compile_program(kokos_program_t program, kokos_compiler_context_t* ctx);

#endif // COMPILE_H_
