#ifndef COMPILE_H_
#define COMPILE_H_

#include "ast.h"
#include "base.h"
#include "instruction.h"
#include "string-store.h"
#include "scope.h"

typedef struct {
    kokos_string_store_t string_store;
    kokos_code_t instructions;
    hash_table call_locations;
    size_t top_level_code_start;
} kokos_compiled_module_t;

// NOTE: maybe create a compiler structure so we potentially can run it multithreaded
bool kokos_expr_compile(kokos_expr_t const* expr, kokos_scope_t* scope);
bool kokos_compile_module(
    kokos_module_t module, kokos_scope_t* scope, kokos_compiled_module_t* compiled_module);

bool kokos_compile_ok(void);
char const* kokos_compile_get_err(void);

#endif // COMPILE_H_
