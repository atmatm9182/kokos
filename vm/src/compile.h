#ifndef COMPILE_H_
#define COMPILE_H_

#include "ast.h"
#include "base.h"
#include "instruction.h"
#include "runtime.h"

#include <stdint.h>

typedef struct {
    string_view* items;
    size_t len;
    size_t cap;
} kokos_variable_list_t;

typedef struct {
    kokos_runtime_string_t const** items;
    // use longer field names so the struct can't be used as a dynamic array
    size_t length;
    size_t capacity;
} kokos_string_store_t;

typedef struct kokos_compiler_context {
    hash_table procedures;
    kokos_variable_list_t locals;
    kokos_string_store_t string_store;
    kokos_code_t procedure_code;
    hash_table call_locations;

    struct kokos_compiler_context* parent;
} kokos_compiler_context_t;

typedef struct {
    string_view* names;
    size_t len;
    bool variadic;
} kokos_params_t;

typedef struct {
    kokos_params_t params;
    uint64_t ip;
} kokos_compiled_proc_t;

kokos_compiler_context_t kokos_ctx_empty(kokos_compiler_context_t* parent);
void kokos_ctx_add_proc(
    kokos_compiler_context_t* ctx, const char* name, kokos_compiled_proc_t* proc);
kokos_compiled_proc_t* kokos_ctx_get_proc(kokos_compiler_context_t* ctx, const char* name);

// NOTE: maybe create a compiler structure so we potentially can run it multithreaded
bool kokos_expr_compile(
    const kokos_expr_t* expr, kokos_compiler_context_t* context, kokos_code_t* code);
kokos_code_t kokos_compile_program(kokos_program_t program, kokos_compiler_context_t* ctx);

bool kokos_compile_ok(void);
const char* kokos_compile_get_err(void);

#endif // COMPILE_H_
