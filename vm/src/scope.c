#include "scope.h"
#include "base.h"
#include "hash.h"
#include "macros.h"
#include "src/instruction.h"

static kokos_runtime_proc_t* kokos_scope_get_proc_cstr(kokos_scope_t* scope, char const* name)
{
    if (!scope)
        return NULL;

    kokos_runtime_proc_t* proc = ht_find(&scope->procs, name);
    if (!proc)
        return kokos_scope_get_proc_cstr(scope->parent, name);

    return proc;
}

kokos_runtime_proc_t* kokos_scope_get_proc(kokos_scope_t* scope, string_view name)
{
    KOKOS_VERIFY(name.size < 256);
    char name_cstr[256];
    snprintf(name_cstr, name.size + 1, SV_FMT, SV_ARG(name));

    return kokos_scope_get_proc_cstr(scope, name_cstr);
}

void kokos_scope_add_proc(kokos_scope_t* scope, string_view name, kokos_runtime_proc_t* proc)
{
    ht_add(&scope->procs, (void*)sv_dup(name), proc);
}

static kokos_macro_t* kokos_scope_get_macro_cstr(kokos_scope_t* scope, char const* name)
{
    if (!scope)
        return NULL;

    kokos_macro_t* macro = ht_find(&scope->macros, name);
    if (!macro)
        return kokos_scope_get_macro_cstr(scope->parent, name);

    return macro;
}

kokos_macro_t* kokos_scope_get_macro(kokos_scope_t* scope, string_view name)
{
    KOKOS_VERIFY(name.size < 256);
    char name_cstr[256];
    sprintf(name_cstr, SV_FMT, SV_ARG(name));

    return kokos_scope_get_macro_cstr(scope, name_cstr);
}

void kokos_scope_add_macro(kokos_scope_t* scope, string_view name, kokos_macro_t* macro)
{
    KOKOS_ASSERT(ht_add(&scope->macros, (void*)sv_dup(name), macro));
}

kokos_vm_t* kokos_vm_create(kokos_scope_t* scope);

kokos_scope_t* kokos_scope_empty(kokos_scope_t* parent, bool top_level)
{
    kokos_scope_t* scope = KOKOS_ALLOC(sizeof(kokos_scope_t));
    scope->procs = ht_make(hash_string_func, hash_string_eq_func, 37);
    scope->macros = ht_make(hash_string_func, hash_string_eq_func, 37);
    DA_INIT(&scope->vars, 0, 7); // seven is a lucky number :^)

    if (parent) {
        scope->parent = parent;
        scope->proc_code = parent->proc_code;
        scope->top_level_code = parent->top_level_code;
        scope->string_store = parent->string_store;
        scope->call_locations = parent->call_locations;
        scope->current_code = top_level ? parent->top_level_code : parent->proc_code;
        scope->macro_vm = parent->macro_vm;
        return scope;
    }

    scope->proc_code = KOKOS_ALLOC(sizeof(kokos_code_t));
    DA_INIT(scope->proc_code, 0, 200);

    scope->top_level_code = KOKOS_ALLOC(sizeof(kokos_code_t));
    DA_INIT(scope->top_level_code, 0, 30);

    scope->current_code = top_level ? scope->top_level_code : scope->proc_code;

    scope->string_store = KOKOS_ALLOC(sizeof(kokos_string_store_t));
    kokos_string_store_init(scope->string_store, 17);

    scope->call_locations = KOKOS_ALLOC(sizeof(hash_table));
    *scope->call_locations = ht_make(hash_sizet_func, hash_sizet_eq_func, 11);

    scope->macro_vm = kokos_vm_create(scope);

    return scope;
}

kokos_scope_t* kokos_scope_global(void)
{
    kokos_scope_t* scope = kokos_scope_empty(NULL, true);
    kokos_native_proc_list_t natives = kokos_natives_get();

    for (size_t i = 0; i < natives.count; i++) {
        kokos_runtime_proc_t* proc = KOKOS_ALLOC(sizeof(kokos_runtime_proc_t));
        proc->native = natives.procs[i];
        proc->type = PROC_NATIVE;

        kokos_scope_add_proc(scope, natives.names[i], proc);
    }

    return scope;
}

void kokos_scope_dump(kokos_scope_t const* scope)
{
    printf("procedures:\n");
    HT_ITER(scope->procs, {
        kokos_runtime_proc_t* proc = kv.value;
        char const* proc_type = proc->type == PROC_KOKOS ? "kokos" : "native";

        printf("%s <%s>\n", (char const*)kv.key, proc_type);
    });

    printf("macros:\n");
    HT_ITER(scope->macros, {
        kokos_macro_t* macro = kv.value;

        printf("%s:\n", (char const*)kv.key);
        kokos_code_dump(macro->instructions);
    });
}
