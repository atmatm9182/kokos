#include "scope.h"
#include "base.h"
#include "hash.h"
#include "macros.h"

/*static kokos_runtime_proc_t* kokos_scope_get_proc_cstr(kokos_scope_t* scope, char const* name)*/
/*{*/
/*    if (!scope)*/
/*        return NULL;*/
/**/
/*    kokos_runtime_proc_t* proc = ht_find(&scope->procs, name);*/
/*    if (!proc)*/
/*        return kokos_scope_get_proc_cstr(scope->parent, name);*/
/**/
/*    return proc;*/
/*}*/
/**/
/*kokos_runtime_proc_t* kokos_scope_get_proc(kokos_scope_t* scope, string_view name)*/
/*{*/
/*    KOKOS_VERIFY(name.size < 256);*/
/*    char name_cstr[256];*/
/*    snprintf(name_cstr, name.size + 1, SV_FMT, SV_ARG(name));*/
/**/
/*    return kokos_scope_get_proc_cstr(scope, name_cstr);*/
/*}*/
/**/

void kokos_scope_add_proc(kokos_scope_t* scope, kokos_runtime_string_t* name, kokos_runtime_proc_t* proc)
{
    ht_add(&scope->procs, name, proc);
}

/*static kokos_macro_t* kokos_scope_get_macro_cstr(kokos_scope_t* scope, char const* name)*/
/*{*/
/*    if (!scope)*/
/*        return NULL;*/
/**/
/*    kokos_macro_t* macro = ht_find(&scope->macros, name);*/
/*    if (!macro)*/
/*        return kokos_scope_get_macro_cstr(scope->parent, name);*/
/**/
/*    return macro;*/
/*}*/
/**/
/*kokos_macro_t* kokos_scope_get_macro(kokos_scope_t* scope, string_view name)*/
/*{*/
/*    KOKOS_VERIFY(name.size < 256);*/
/*    char name_cstr[256];*/
/*    sprintf(name_cstr, SV_FMT, SV_ARG(name));*/
/**/
/*    return kokos_scope_get_macro_cstr(scope, name_cstr);*/
/*}*/
/**/
/*void kokos_scope_add_macro(kokos_scope_t* scope, string_view name, kokos_macro_t* macro)*/
/*{*/
/*    KOKOS_ASSERT(ht_add(&scope->macros, (void*)sv_dup(name), macro));*/
/*}*/
/**/
kokos_vm_t* kokos_vm_create(kokos_scope_t* scope);

kokos_scope_t* kokos_scope_derived(kokos_scope_t* parent)
{
    KOKOS_VERIFY(parent != NULL);

    kokos_scope_t* scope = KOKOS_ALLOC(sizeof(*parent));
    scope->parent = parent;
    scope->macro_vm = parent->macro_vm;
    scope->string_store = parent->string_store;
    scope->procs = ht_make(hash_runtime_string_func, hash_runtime_string_eq_func, 53);
    /*scope->macros = ht_make(hash_cstring_func, hash_cstring_eq_func, 5);*/
    scope->call_locations = ht_make(hash_sizet_func, hash_sizet_eq_func, 5);

    DA_INIT(&scope->derived, 0, 3);
    DA_INIT(&scope->code, 0, 17);

    DA_ADD(&parent->derived, scope);

    return scope;
}

kokos_scope_t* kokos_scope_root(void)
{
    kokos_scope_t* scope = KOKOS_ALLOC(sizeof(kokos_scope_t));
    scope->parent = NULL;
    scope->macro_vm = kokos_vm_create(scope);
    scope->string_store = KOKOS_ALLOC(sizeof(*scope->string_store));
    kokos_string_store_init(scope->string_store, 89);
    scope->procs = ht_make(hash_runtime_string_func, hash_runtime_string_eq_func, 53);
    /*scope->macros = ht_make(hash_cstring_func, hash_cstring_eq_func, 53);*/
    scope->call_locations = ht_make(hash_sizet_func, hash_sizet_eq_func, 53);

    DA_INIT(&scope->derived, 0, 53);

    kokos_native_proc_list_t natives = kokos_natives_get();
    DA_INIT(&scope->code, 0, natives.count);

    // setup native functions
    for (size_t i = 0; i < natives.count; i++) {
        kokos_runtime_proc_t* proc = KOKOS_ALLOC(sizeof(kokos_runtime_proc_t));
        proc->native = natives.procs[i];
        proc->type = PROC_NATIVE;

        DA_ADD(&scope->code, INSTR_PUSH(TO_PROC(proc).as_int));

        kokos_runtime_string_t* name = (kokos_runtime_string_t*)kokos_string_store_add_sv(scope->string_store, natives.names[i]);
        DA_ADD(&scope->code, INSTR_ADD_LOCAL(TO_STRING(name).as_int));

        kokos_scope_add_proc(scope, name, proc);
    }

    kokos_natives_free(&natives);

    return scope;
}

void kokos_vm_destroy(kokos_vm_t*);

void kokos_scope_destroy(kokos_scope_t* scope)
{
    if (!scope->parent) {
        kokos_vm_destroy(scope->macro_vm);
        kokos_string_store_destroy(scope->string_store);
        KOKOS_FREE(scope->string_store);
    }

    // WARN: don't free the names, because the string store owns them
    HT_ITER(scope->procs, {
        kokos_runtime_proc_destroy(GET_PROC_PTR(kv.value));
        KOKOS_FREE(kv.value);
    });

    for (size_t i = 0; i < scope->derived.len; i++) {
        kokos_scope_destroy(scope->derived.items[i]);
    }

    DA_FREE(&scope->derived);
    ht_destroy(&scope->procs);
    DA_FREE(&scope->code);
    ht_destroy(&scope->call_locations);
    KOKOS_FREE(scope);
}

void kokos_scope_dump(kokos_scope_t const* scope)
{
    printf("Nothing to see here :)");
    /*printf("procedures:\n");*/
    /*HT_ITER(scope->procs, {*/
    /*    kokos_runtime_proc_t* proc = kv.value;*/
    /*    char const* proc_type = proc->type == PROC_KOKOS ? "kokos" : "native";*/
    /**/
    /*    printf("%s <%s>\n", (char const*)kv.key, proc_type);*/
    /*});*/
    /**/
    /*printf("macros:\n");*/
    /*HT_ITER(scope->macros, {*/
    /*    kokos_macro_t* macro = kv.value;*/
    /**/
    /*    printf("%s:\n", (char const*)kv.key);*/
    /*    kokos_code_dump(macro->instructions);*/
    /*});*/
}
