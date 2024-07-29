#ifndef COMPILE_H_
#define COMPILE_H_

#include "ast.h"
#include "base.h"
#include "instruction.h"
#include "string-store.h"

typedef struct {
    string_view* items;
    size_t len;
    size_t cap;
} kokos_variable_list_t;

typedef struct {
    string_view* names;
    size_t len;
    bool variadic;
} kokos_params_t;

typedef size_t kokos_label_t;

typedef struct {
    kokos_label_t label;
    size_t code_len;
    kokos_params_t params;
} kokos_compiled_proc_t;

typedef struct scope {
    // root vars
    kokos_string_store_t* string_store;
    kokos_code_t* proc_code;
    kokos_code_t* top_level_code;
    hash_table* call_locations;

    // individual vars
    hash_table procs;
    kokos_variable_list_t vars;
    kokos_code_t* current_code;

    struct scope* parent;
} kokos_scope_t;

typedef struct {
    kokos_string_store_t string_store;
    kokos_code_t instructions;
    hash_table call_locations;
    size_t top_level_code_start;
} kokos_compiled_module_t;

kokos_scope_t kokos_scope_empty(kokos_scope_t* parent, bool top_level);
void kokos_scope_add_proc(kokos_scope_t* scope, const char* name, kokos_compiled_proc_t* proc);
kokos_compiled_proc_t* kokos_scope_get_proc(kokos_scope_t* scope, const char* name);

// NOTE: maybe create a compiler structure so we potentially can run it multithreaded
bool kokos_expr_compile(const kokos_expr_t* expr, kokos_scope_t* context);
bool kokos_compile_module(
    kokos_module_t module, kokos_scope_t* scope, kokos_compiled_module_t* compiled_module);

bool kokos_compile_ok(void);
const char* kokos_compile_get_err(void);

#endif // COMPILE_H_
