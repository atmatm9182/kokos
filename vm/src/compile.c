#include "ast.h"
#include "macro.h"
#include "macros.h"
#include "runtime.h"
#include "scope.h"
#include "token.h"

#include "base.h"

#include "compile.h"
#include "instruction.h"
#include "value.h"

#include "vm.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERIFY_TYPE(expr, t)                                                                       \
    do {                                                                                           \
        if ((expr)->type != (t)) {                                                                 \
            set_error((expr)->token.location, "type mismatch: expected '%s', found '%s'\n",        \
                kokos_expr_type_str((t)), kokos_expr_type_str((expr)->type));                      \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

static char err_buf[512];

bool kokos_compile_ok(void)
{
    return strlen(err_buf) == 0;
}

const char* kokos_compile_get_err(void)
{
    return err_buf;
}

static void set_error(kokos_location_t location, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));

static void set_error(kokos_location_t location, const char* fmt, ...)
{
    int len = sprintf(err_buf, "%s:%lu:%lu ", location.filename, location.row, location.col);

    va_list sprintf_args;
    va_start(sprintf_args, fmt);
    vsprintf(err_buf + len, fmt, sprintf_args);
}

typedef enum {
    P_EQUAL,
    P_AT_LEAST,
} arity_predicate_e;

static bool expect_arity(kokos_location_t location, int expected, int got, arity_predicate_e pred)
{
    switch (pred) {
    case P_EQUAL:
        if (expected != got) {
            set_error(location, "Arity mismatch: expected %d arguments, got %d", expected, got);
            return false;
        }
        break;
    case P_AT_LEAST:
        if (got < expected) {
            set_error(
                location, "Arity mismatch: expected at least %d arguments, got %d", expected, got);
            return false;
        }
        break;
    }

    return true;
}

static uint64_t to_double_bytes(const kokos_expr_t* expr)
{
    double parsed = sv_atof(expr->token.value);
    return TO_VALUE(parsed).as_int;
}

static kokos_expr_t empty_exprs[0] = {};

static kokos_list_t empty_list = { .items = empty_exprs, .len = 0 };

static inline kokos_list_t list_slice(kokos_list_t list, size_t idx)
{
    ssize_t count = list.len - idx;
    if (count <= 0) {
        return empty_list;
    }

    return (kokos_list_t) { .items = list.items + idx, .len = count };
}

bool kokos_expr_to_params(const kokos_expr_t* expr, kokos_params_t* params, kokos_scope_t* scope)
{
    kokos_list_t list = expr->list;

    struct {
        kokos_runtime_string_t** items;
        size_t len;
        size_t cap;
    } names;
    DA_INIT_ZEROED(&names, list.len);

    bool variadic = false;
    for (size_t i = 0; i < list.len; i++) {
        const kokos_expr_t* param = &list.items[i];
        VERIFY_TYPE(param, EXPR_IDENT);

        if (sv_eq_cstr(param->token.value, "&")) {
            if (i != list.len - 2) {
                set_error(expr->token.location, "malformed variadic paramater list");
                return false;
            }

            variadic = true;
            continue;
        }

        DA_ADD(&names, (void*)kokos_string_store_add_sv(scope->string_store, param->token.value));
    }

    *params = (kokos_params_t) { .names = names.items, .len = names.len, .variadic = variadic };
    return true;
}

static bool get_special_value(string_view value, uint64_t* out)
{
    if (sv_eq_cstr(value, "true")) {
        *out = TRUE_BITS;
        return true;
    }
    if (sv_eq_cstr(value, "false")) {
        *out = FALSE_BITS;
        return true;
    }
    if (sv_eq_cstr(value, "nil")) {
        *out = NIL_BITS;
        return true;
    }

    return false;
}

typedef bool (*kokos_compilation_func_t)(const kokos_expr_t*, kokos_scope_t* scope);

static bool kokos_compile_all(
    kokos_list_t exprs, kokos_scope_t* scope, kokos_compilation_func_t comp)
{
    for (size_t i = 0; i < exprs.len; i++) {
        printf("compiling ");
        kokos_expr_dump(&exprs.items[i]);
        printf("\n");
        TRY(comp(&exprs.items[i], scope));
    }

    return true;
}

static inline bool kokos_compile_all_reversed(
    kokos_list_t exprs, kokos_scope_t* scope, kokos_compilation_func_t comp)
{
    for (size_t i = 0; i < exprs.len; i++) {
        TRY(comp(&exprs.items[exprs.len - 1 - i], scope));
    }

    return true;
}

#include "sform.c"

typedef bool (*kokos_sform_t)(const kokos_expr_t* expr, kokos_scope_t* scope);

typedef struct {
    const char* name;
    kokos_sform_t sform;
} kokos_sform_pair_t;

static kokos_sform_pair_t sforms[] = {
#define X(f, name) { #name, kokos_sform_##f },
    ENUMERATE_SFORMS
#undef X
};

#define SFORMS_COUNT (sizeof(sforms) / sizeof(sforms[0]))

static kokos_sform_t get_sform(string_view name)
{
    for (size_t i = 0; i < SFORMS_COUNT; i++) {
        const char* sname = sforms[i].name;
        if (sv_eq_cstr(name, sname)) {
            return sforms[i].sform;
        }
    }

    return NULL;
}

static kokos_scope_t* scope_get_root(kokos_scope_t* scope)
{
    while (scope->parent != NULL) {
        scope = scope->parent;
    }

    return scope;
}

static void scope_add_call_location(kokos_scope_t* scope, size_t ip, kokos_token_t where)
{
    scope = scope_get_root(scope);
    kokos_token_t* tok = KOKOS_ALLOC(sizeof(kokos_token_t));
    *tok = where;

    ht_add(&scope->call_locations, (void*)ip, tok);
}

typedef struct {
    kokos_expr_t* exprs;
    size_t len;
} kokos_expr_list_t;

kokos_expr_list_t kokos_values_to_exprs(
    const kokos_value_t* values, size_t values_count, kokos_scope_t* scope)
{
    struct {
        kokos_expr_t* items;
        size_t len, cap;
    } result;
    DA_INIT(&result, 0, values_count);

    for (size_t i = 0; i < values_count; i++) {
        kokos_value_t val = values[i];
        kokos_token_t tok;
        kokos_expr_t expr = { .flags = EXPR_FLAGS_NONE };

        // do a separate check since the doubles are untagged
        // i could probably come up with a better aproach, but i do not care right now :P
        if (IS_DOUBLE(val)) {
            char buf[64];
            sprintf(buf, "%f", val.as_double);
            const kokos_runtime_string_t* string
                = kokos_string_store_add_cstr(scope->string_store, buf);

            // TODO: come up with a smart way to add token locations since this function is used to
            // turn macro evaluation results into the ast at compile time and we probably know where
            // that macro was called => we know where that macro was evaluated => we can report
            // a nice error message to the user later
            tok.type = TT_FLOAT_LIT;
            tok.value = sv_make(string->ptr, string->len);

            expr.type = EXPR_FLOAT_LIT;
            goto add_expr;
        }

        switch (GET_TAG(val.as_int)) {
        case STRING_TAG: {
            kokos_runtime_string_t* str = GET_STRING(val);

            tok.type = TT_STR_LIT;
            tok.value = sv_make(str->ptr, str->len);

            expr.type = EXPR_STRING_LIT;
            break;
        }
        case LIST_TAG: {
            tok.type = TT_LPAREN;
            tok.value = sv_make_cstr("(");

            kokos_runtime_list_t* list = GET_LIST(val);
            kokos_expr_list_t exprs = kokos_values_to_exprs(list->items, list->len, scope);

            expr.type = EXPR_LIST;
            expr.list = (kokos_list_t) { .items = exprs.exprs, .len = exprs.len };
            break;
        }
        case SYM_TAG: {
            kokos_runtime_sym_t* sym = GET_SYM(val);
            tok.value = sv_make(sym->ptr, sym->len);
            tok.type = TT_IDENT;

            expr.type = EXPR_IDENT;
            break;
        }
        case INT_TAG: {
            char buf[10];
            sprintf(buf, "%d", GET_INT(val));
            const kokos_runtime_string_t* string
                = kokos_string_store_add_cstr(scope->string_store, buf);

            tok.value = sv_make(string->ptr, string->len);
            tok.type = TT_INT_LIT;

            expr.type = EXPR_INT_LIT;
            break;
        }
        default: {
            char buf[512];
            sprintf(buf, "value with tag %lx to expr", VALUE_TAG(val));
            KOKOS_TODO(buf);
        }
        }

    add_expr:
        expr.token = tok;
        DA_ADD(&result, expr);
    }

    return (kokos_expr_list_t) { .exprs = result.items, .len = result.len };
}

bool kokos_expr_compile_quoted(const kokos_expr_t* expr, kokos_scope_t* scope);

static bool kokos_eval_macro(
    const kokos_macro_t* macro, const kokos_expr_t* expr, kokos_scope_t* scope)
{
    KOKOS_ASSERT(expr->type == EXPR_LIST);
    KOKOS_ASSERT(expr->list.len >= 1);

    kokos_list_t args = list_slice(expr->list, 1);
    string_view head = expr->list.items[0].token.value;

    if (macro->params.variadic) {
        KOKOS_TODO();
    }

    if (macro->params.len != args.len) {
        set_error(expr->token.location,
            "expected %lu arguments for procedure '" SV_FMT "', got %lu instead", macro->params.len,
            SV_ARG(head), args.len);
        return false;
    }

    kokos_scope_t* dumb_scope = kokos_scope_derived(scope);

    for (size_t i = 0; i < args.len; i++) {
        TRY(kokos_expr_compile_quoted(&args.items[i], dumb_scope));
        DA_ADD(&dumb_scope->code, INSTR_ADD_LOCAL(macro->params.names[i]));
    }

    DA_EXTEND(&dumb_scope->code, &macro->instructions);

    kokos_vm_t* vm = scope->macro_vm;
    if (!kokos_vm_run_code(vm, dumb_scope->code)) {
        const char* ex = kokos_exception_to_string(&vm->registers.exception);
        set_error(expr->token.location, "macro evaluation has produced an exception: %s", ex);
        return false;
    }

    kokos_frame_t* f = vm->frames.data[0];
    kokos_expr_list_t exprs = kokos_values_to_exprs(f->stack.data, f->stack.sp, scope);
    for (size_t i = 0; i < exprs.len; i++) {
        TRY(kokos_expr_compile(&exprs.exprs[i], scope));
    }

    kokos_exprs_destroy_recursively(exprs.exprs, exprs.len);

    return true;
}

static bool compile_list(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    kokos_list_t list = expr->list;
    if (list.len == 0) {
        DA_ADD(&scope->code, INSTR_ALLOC(LIST_BITS, 0));
        return true;
    }

    string_view head = list.items[0].token.value;

    kokos_sform_t sform = get_sform(head);
    if (sform) {
        TRY(sform(expr, scope));
        return true;
    }

    kokos_macro_t* macro = kokos_scope_get_macro(scope, head);
    if (macro) {
        TRY(kokos_eval_macro(macro, expr, scope));
        return true;
    }

    /*kokos_macro_t* macro = kokos_scope_get_macro(scope, head);*/
    /*if (macro) {*/
    /*    return kokos_eval_macro(macro, expr, scope);*/
    /*}*/
    /**/
    kokos_list_t args = list_slice(expr->list, 1);
    TRY(kokos_compile_all_reversed(args, scope, kokos_expr_compile));
    DA_ADD(&scope->code,
        INSTR_CALL(kokos_string_store_add_sv(scope->string_store, head), list.len - 1));

    return true;
}

bool compile_quoted_list(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    kokos_list_t list = expr->list;
    for (ssize_t i = list.len - 1; i >= 0; i--) {
        TRY(kokos_expr_compile_quoted(&list.items[i], scope));
    }

    DA_ADD(&scope->code, INSTR_ALLOC(LIST_BITS, list.len));
    return true;
}

bool kokos_expr_compile(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    kokos_code_t* code = &scope->code;

    switch (expr->type) {
    case EXPR_FLOAT_LIT: {
        uint64_t value = to_double_bytes(expr);
        DA_ADD(code, INSTR_PUSH(TO_VALUE(value)));
        break;
    }
    case EXPR_INT_LIT: {
        uint64_t value = sv_atoi(expr->token.value);
        DA_ADD(code, INSTR_PUSH(TO_INT_INT(value)));
        break;
    }
    case EXPR_LIST: {
        if (EXPR_QUOTED(expr)) {
            return compile_quoted_list(expr, scope);
        }
        return compile_list(expr, scope);
    }
    case EXPR_IDENT: {
        uint64_t special;
        if (get_special_value(expr->token.value, &special)) {
            DA_ADD(code, INSTR_PUSH(TO_STRING_INT(special)));
            break;
        }

        DA_ADD(code,
            INSTR_GET_LOCAL(kokos_string_store_add_sv(scope->string_store, expr->token.value)));
        break;
    }
    case EXPR_STRING_LIT: {
        DA_ADD(&scope->code,
            INSTR_PUSH(TO_STRING(
                (void*)kokos_string_store_add_sv(scope->string_store, expr->token.value))));
        break;
    }
    case EXPR_MAP: {
        kokos_map_t map = expr->map;
        for (size_t i = 0; i < map.len; i++) {
            TRY(kokos_expr_compile(&map.keys[i], scope));
            TRY(kokos_expr_compile(&map.values[i], scope));
        }

        DA_ADD(code, INSTR_ALLOC(MAP_BITS, map.len));
        break;
    }
    case EXPR_VECTOR: {
        kokos_vec_t vec = expr->vec;
        for (ssize_t i = vec.len - 1; i >= 0; i--) {
            TRY(kokos_expr_compile(&vec.items[i], scope));
        }

        DA_ADD(code, INSTR_ALLOC(VECTOR_BITS, vec.len));
        break;
    }
    default: {
        char buf[128] = { 0 };
        sprintf(buf, "compilation of expression %s is not implemented",
            kokos_expr_type_str(expr->type));
        KOKOS_TODO(buf);
    }
    }

    return true;
}

bool kokos_expr_compile_quoted(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    switch (expr->type) {
    case EXPR_INT_LIT:
    case EXPR_FLOAT_LIT:
    case EXPR_STRING_LIT:
    case EXPR_MAP:
    case EXPR_VECTOR:     {
        return kokos_expr_compile(expr, scope);
    }
    case EXPR_LIST: {
        return compile_quoted_list(expr, scope);
    }
    case EXPR_IDENT: {
        kokos_runtime_string_t* sym
            = (void*)kokos_string_store_add_sv(scope->string_store, expr->token.value);
        DA_ADD(&scope->code, INSTR_PUSH(TO_SYM(sym)));
        return true;
    }
    default: {
        char buf[128] = { 0 };
        sprintf(buf, "compilation of QUOTED expression %s is not implemented",
            kokos_expr_type_str(expr->type));
        KOKOS_TODO(buf);
    }
    }

    return true;
}

static void kokos_compiled_module_init_from_scope(
    kokos_compiled_module_t* module, kokos_scope_t* scope)
{
    KOKOS_ASSERT(scope->parent == NULL); // ensuring that the module's scope is toplevel!

    module->call_locations = scope->call_locations;
    module->string_store = *scope->string_store;
    module->procs = scope->procs;
    /*module->macros = scope->macros;*/
    module->top_level_code_start = module->instructions.len;

    DA_ADD(&scope->code, INSTR_RET);
    module->instructions = scope->code;
}

bool kokos_compile_module(
    kokos_module_t module, kokos_scope_t* scope, kokos_compiled_module_t* compiled_module)
{
    for (size_t i = 0; i < module.len; i++) {
        TRY(kokos_expr_compile(&module.items[i], scope));
    }

    kokos_compiled_module_init_from_scope(compiled_module, scope);
    return true;
}
