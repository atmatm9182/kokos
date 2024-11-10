#ifndef SCOPE_H_
#define SCOPE_H_

#include "base.h"
#include "instruction.h"
#include "string-store.h"
#include "macro.h"

typedef struct {
    string_view* items;
    size_t len;
    size_t cap;
} kokos_variable_list_t;

struct scope;

typedef struct {
    struct scope** items;
    size_t len;
    size_t cap;
} kokos_scope_list_t;

typedef struct scope {
    kokos_string_store_t* string_store;
    kokos_code_t code;
    hash_table call_locations;
    hash_table procs;
    hash_table macros;
    kokos_vm_t* macro_vm;
    kokos_scope_list_t derived;

    struct scope* parent;
} kokos_scope_t;

kokos_scope_t* kokos_scope_derived(kokos_scope_t* parent);
kokos_scope_t* kokos_scope_root(void);
void kokos_scope_destroy(kokos_scope_t*);

kokos_macro_t* kokos_scope_get_macro(kokos_scope_t* scope, string_view name);

void kokos_scope_dump(kokos_scope_t const* scope);

#endif // SCOPE_H_
