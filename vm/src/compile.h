#ifndef COMPILE_H_
#define COMPILE_H_

#include "ast.h"
#include "base.h"
#include "instruction.h"
#include "runtime.h"

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
    kokos_code_t* code;
    hash_table* call_locations;

    // individual vars
    hash_table procs;
    kokos_variable_list_t vars;

    struct scope* parent;
} kokos_scope_t;

kokos_scope_t kokos_scope_empty(kokos_scope_t* parent);
void kokos_scope_add_proc(
    kokos_scope_t* scope, const char* name, kokos_compiled_proc_t* proc);
kokos_compiled_proc_t* kokos_scope_get_proc(kokos_scope_t* scope, const char* name);

// NOTE: maybe create a compiler structure so we potentially can run it multithreaded
bool kokos_expr_compile(
    const kokos_expr_t* expr, kokos_scope_t* context, kokos_code_t* code);
kokos_code_t kokos_compile_module(kokos_module_t module, kokos_scope_t* scope);

bool kokos_compile_ok(void);
const char* kokos_compile_get_err(void);

#endif // COMPILE_H_
