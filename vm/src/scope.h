#ifndef SCOPE_H_
#define SCOPE_H_

#include "base.h"
#include "instruction.h"
#include "string-store.h"

typedef struct {
    string_view* items;
    size_t len;
    size_t cap;
} kokos_variable_list_t;

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

kokos_scope_t kokos_scope_empty(kokos_scope_t* parent, bool top_level);
kokos_scope_t kokos_scope_global(void);

void kokos_scope_add_proc(kokos_scope_t* scope, char const* name, kokos_runtime_proc_t* proc);
kokos_runtime_proc_t* kokos_scope_get_proc(kokos_scope_t* scope, string_view name);

void kokos_scope_dump(kokos_scope_t const* scope);

#endif // SCOPE_H_
