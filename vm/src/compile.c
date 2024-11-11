#include "ast.h"
#include "macros.h"
#include "runtime.h"
#include "macro.h"
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

char const* kokos_compile_get_err(void)
{
    return err_buf;
}

static void set_error(kokos_location_t location, char const* fmt, ...)
    __attribute__((format(printf, 2, 3)));

static void set_error(kokos_location_t location, char const* fmt, ...)
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

static uint64_t to_double_bytes(kokos_expr_t const* expr)
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

bool expr_to_params(kokos_expr_t const* expr, kokos_params_t* params, kokos_scope_t* scope)
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
        kokos_expr_t const* param = &list.items[i];
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

static bool compile_procedure_def(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    kokos_list_t list = expr->list;
    if (list.len < 3) {
        set_error(expr->token.location,
            "procedure definition must have at least a name and an argument list");
        return false;
    }

    string_view name = list.items[1].token.value;

    kokos_expr_t const* params_expr = &list.items[2];
    if (params_expr->type != EXPR_LIST) {
        set_error(params_expr->token.location,
            "cannot use value of type %s as an argument list for a procedure",
            kokos_expr_type_str(params_expr->type));
        return false;
    }

    kokos_params_t params;
    TRY(expr_to_params(params_expr, &params, scope));

    kokos_scope_t* proc_scope = kokos_scope_derived(scope);

    // Add the procedure before the body compilation to allow recursion
    kokos_runtime_proc_t* proc = KOKOS_ZALLOC(sizeof(kokos_runtime_proc_t));
    proc->type = PROC_KOKOS;
    proc->kokos.params = params;

    /*kokos_scope_add_proc(scope, name, proc);*/

    for (size_t i = 3; i < list.len; i++) {
        TRY(kokos_expr_compile(&list.items[i], proc_scope));
    }

    DA_ADD(&proc_scope->code, INSTR_RET);
    proc->kokos.code = proc_scope->code;

    const kokos_runtime_string_t* rt_name = kokos_string_store_add_sv(scope->string_store, name);

    DA_ADD(&scope->code, INSTR_PUSH(TO_PROC(proc).as_int));
    DA_ADD(&scope->code, INSTR_ADD_LOCAL(rt_name));

    ht_add(&scope->procs, (kokos_runtime_string_t*)rt_name, proc);

    return true;
}

static bool compile_macro_def(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    KOKOS_ASSERT(expr->type == EXPR_LIST);
    KOKOS_ASSERT(expr->list.len >= 1);

    kokos_list_t list = expr->list;
    if (list.len < 3) {
       set_error(expr->token.location,
           "procedure definition must have at least a name and an argument list");
       return false;
    }

    kokos_expr_t const* params_expr = &list.items[2];
    if (params_expr->type != EXPR_LIST) {
       set_error(params_expr->token.location,
           "cannot use value of type %s as an argument list for a procedure",
           kokos_expr_type_str(params_expr->type));
       return false;
    }

    kokos_params_t params;
    TRY(expr_to_params(params_expr, &params, scope));

    kokos_scope_t* macro_scope = kokos_scope_derived(scope);

    // Add the macro before the body compilation to allow recursion
    kokos_macro_t* macro = KOKOS_ALLOC(sizeof(kokos_macro_t));
    macro->params = params;
    macro->name = (void*)kokos_string_store_add_sv(scope->string_store, list.items[1].token.value);

    ht_add(&scope->macros, macro->name, macro);

    for (size_t i = 3; i < list.len; i++) {
       TRY(kokos_expr_compile(&list.items[i], macro_scope));
    }

    DA_ADD(&macro_scope->code, INSTR_RET);
    macro->instructions = macro_scope->code;

    return true;
}

typedef struct {
    uint32_t idx;
    uint32_t hops;
} kokos_var_lookup_result_t;

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

typedef bool (*kokos_expr_compilation_func_t)(kokos_expr_t const*, kokos_scope_t* scope);

static bool compile_all_args_reversed(
    kokos_expr_t const* expr, kokos_scope_t* scope, kokos_expr_compilation_func_t func)
{
    kokos_list_t elements = expr->list;
    for (size_t i = elements.len - 1; i > 0; i--) {
        kokos_expr_t const* elem = &elements.items[i];
        TRY(kokos_expr_compile(elem, scope));
    }

    return true;
}

static bool compile_all_args(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    kokos_list_t elements = expr->list;
    for (size_t i = 1; i < elements.len; i++) {
        kokos_expr_t const* elem = &elements.items[i];
        TRY(kokos_expr_compile(elem, scope));
    }

    return true;
}

static bool compile_mul(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    TRY(compile_all_args(expr, scope));

    DA_ADD(&scope->code, INSTR_MUL(expr->list.len - 1));
    return true;
}

static bool compile_div(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    TRY(compile_all_args(expr, scope));

    DA_ADD(&scope->code, INSTR_DIV(expr->list.len - 1));
    return true;
}
static bool compile_sub(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    TRY(compile_all_args(expr, scope));

    DA_ADD(&scope->code, INSTR_SUB(expr->list.len - 1));
    return true;
}

static bool compile_add(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    TRY(compile_all_args(expr, scope));

    DA_ADD(&scope->code, INSTR_ADD(expr->list.len - 1));
    return true;
}

static bool compile_if(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    kokos_code_t* code = &scope->code;
    if (expr->list.len < 3 || expr->list.len > 4) {
        set_error(expr->token.location, "badly formed 'if' expression");
        return false;
    }

    kokos_list_t exprs = expr->list;

    TRY(kokos_expr_compile(&exprs.items[1], scope));

    DA_ADD(code, INSTR_JZ(0));

    size_t start_ip = code->len;
    // TODO: we should introduce two separate scopes for the true and false branches
    TRY(kokos_expr_compile(&exprs.items[2], scope));

    switch (expr->list.len) {
    case 4: {
        DA_ADD(code, INSTR_BRANCH(0));

        size_t consequence_ip = code->len - start_ip;
        TRY(kokos_expr_compile(&exprs.items[3], scope));

        KOKOS_ASSERT(code->items[start_ip - 1].type == I_JZ);
        code->items[start_ip - 1].operand = consequence_ip + 1; // patch the jump instruction

        KOKOS_ASSERT(code->items[consequence_ip + start_ip - 1].type == I_BRANCH);
        code->items[consequence_ip + start_ip - 1].operand
            = code->len - (consequence_ip + start_ip) + 1; // patch the branch instruction

        return true;
    }
    case 3: {
        size_t consequence_ip = code->len - start_ip;
        KOKOS_ASSERT(code->items[start_ip - 1].type == I_JZ);
        code->items[start_ip - 1].operand = consequence_ip + 1; // patch the jump instruction
        return true;
    }
    default: KOKOS_VERIFY("unreachable");
    }

    return false;
}

static bool compile_eq(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, scope));

    DA_ADD(&scope->code, INSTR_CMP);
    DA_ADD(&scope->code, INSTR_EQ(0));

    return true;
}

static bool compile_neq(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, scope));

    DA_ADD(&scope->code, INSTR_CMP);
    DA_ADD(&scope->code, INSTR_NEQ(0));

    return true;
}

static bool compile_lte(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, scope));

    DA_ADD(&scope->code, INSTR_CMP);
    DA_ADD(&scope->code, INSTR_NEQ(1));

    return true;
}

static bool compile_gte(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, scope));

    DA_ADD(&scope->code, INSTR_CMP);
    DA_ADD(&scope->code, INSTR_NEQ((uint64_t)-1));

    return true;
}

static bool compile_gt(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, scope));

    DA_ADD(&scope->code, INSTR_CMP);
    DA_ADD(&scope->code, INSTR_EQ(1));

    return true;
}

static bool compile_lt(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, scope));

    DA_ADD(&scope->code, INSTR_CMP);
    DA_ADD(&scope->code, INSTR_EQ((uint64_t)-1));

    return true;
}

// NOTE: this is kinda weird. `let` creates a new scope, but the locals will still
// be stored into the `locals` array of the current function frame
static bool compile_let(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    KOKOS_ASSERT(expr->type == EXPR_LIST);

    VERIFY_TYPE(&expr->list.items[1], EXPR_LIST);
    kokos_list_t vars = expr->list.items[1].list;

    DA_ADD(&scope->code, INSTR_PUSH_SCOPE(vars.len / 2));

    for (size_t i = 0; i < vars.len; i += 2) {
        kokos_expr_t const* key = &vars.items[i];
        VERIFY_TYPE(key, EXPR_IDENT);

        kokos_expr_t const* value = &vars.items[i + 1];
        TRY(kokos_expr_compile(value, scope));

        kokos_runtime_string_t* var_name = (void*)kokos_string_store_add_sv(scope->string_store, key->token.value);
        DA_ADD(&scope->code, INSTR_ADD_LOCAL(var_name));
    }

    // compile body with the new bindings
    for (size_t i = 0; i < expr->list.len - 2; i++) {
        TRY(kokos_expr_compile(&expr->list.items[i + 2], scope));
    }

    DA_ADD(&scope->code, INSTR_POP_SCOPE);

    return true;
}

typedef bool (*kokos_sform_t)(kokos_expr_t const* expr, kokos_scope_t* scope);

typedef struct {
    char const* name;
    kokos_sform_t sform;
} kokos_sform_pair_t;

static kokos_sform_pair_t sforms[] = {
    { "proc", compile_procedure_def },
    { "macro", compile_macro_def },
    { "let", compile_let },
    { "+", compile_add },
    { "-", compile_sub },
    { "*", compile_mul },
    { "/", compile_div },
    { "if", compile_if },
    { "<", compile_lt },
    { ">", compile_gt },
    { "<=", compile_lte },
    { ">=", compile_gte },
    { "=", compile_eq },
    { "/=", compile_neq },
};

#define SFORMS_COUNT (sizeof(sforms) / sizeof(sforms[0]))

static kokos_sform_t get_sform(string_view name)
{
    for (size_t i = 0; i < SFORMS_COUNT; i++) {
        char const* sname = sforms[i].name;
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

static void kokos_compile_string_lit(
    kokos_expr_t const* expr, kokos_scope_t* scope, kokos_code_t* code)
{
    kokos_runtime_string_t* string = (void*)kokos_string_store_add_sv(scope->string_store, expr->token.value);
    DA_ADD(code, INSTR_PUSH(TO_STRING(string).as_int));
}

typedef struct {
    kokos_expr_t* exprs;
    size_t len;
} kokos_expr_list_t;

kokos_expr_list_t kokos_values_to_exprs(
    kokos_value_t const* values, size_t values_count, kokos_scope_t* scope)
{
    struct {
        kokos_expr_t* items;
        size_t len, cap;
    } result;
    DA_INIT(&result, 0, values_count);

    for (size_t i = 0; i < values_count; i++) {
        kokos_value_t val = values[i];
        kokos_token_t tok;
        kokos_expr_t expr;

        // do a separate check since the doubles are untagged
        // i could probably come up with a better aproach, but i do not care right now :P
        if (IS_DOUBLE(val)) {
            char buf[64];
            sprintf(buf, "%f", val.as_double);
            kokos_runtime_string_t const* string
                = kokos_string_store_add_cstr(scope->string_store, buf);

            // TODO: come up with a smart way to add token locations since this function is used to
            // turn macro evaluation results into the ast at compile time and we probably know where
            // that macro was called => we know where that macro was evaluated => we can report
            // a nice error message to the user later
            tok.type = TT_FLOAT_LIT;
            tok.value = sv_make(string->ptr, string->len);

            expr.type = EXPR_FLOAT_LIT;
            expr.flags = EXPR_FLAGS_NONE;
            goto add_expr;
        }

        switch (GET_TAG(val.as_int)) {
        case INT_TAG: {
            char buf[64];
            sprintf(buf, "%d", GET_INT(val));
            kokos_runtime_string_t const* string
                = kokos_string_store_add_cstr(scope->string_store, buf);

            tok.value = sv_make(string->ptr, string->len);
            tok.type = TT_INT_LIT;

            expr.type = EXPR_INT_LIT;
            break;
        default: KOKOS_TODO();
        }
        }

    add_expr:
        expr.token = tok;
        DA_ADD(&result, expr);
    }

    return (kokos_expr_list_t) { .exprs = result.items, .len = result.len };
}

bool kokos_expr_compile_quoted(kokos_expr_t const* expr, kokos_scope_t* scope);

static bool kokos_eval_macro(
    kokos_macro_t const* macro, kokos_expr_t const* expr, kokos_scope_t* scope)
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
                 "expected %lu arguments for procedure '" SV_FMT "', got %lu instead",
                 macro->params.len,
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
        set_error(expr->token.location,
                  "macro evaluation has produced an exception: %s",
                  ex);
        return false;
    }

    kokos_frame_t* f = vm->frames.data[0];
    kokos_expr_list_t exprs = kokos_values_to_exprs(f->stack.data, f->stack.sp, scope);
    for (size_t i = 0; i < exprs.len; i++) {
       TRY(kokos_expr_compile(&exprs.exprs[i], scope));
    }

    KOKOS_FREE(exprs.exprs);

    return true;
}

static bool compile_list(kokos_expr_t const* expr, kokos_scope_t* scope)
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
    TRY(compile_all_args_reversed(expr, scope, kokos_expr_compile));
    DA_ADD(&scope->code, INSTR_CALL(kokos_string_store_add_sv(scope->string_store, head), list.len - 1));

    return true;
}

bool compile_quoted_list(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    kokos_list_t list = expr->list;
    for (ssize_t i = list.len - 1; i >= 0; i--) {
        TRY(kokos_expr_compile_quoted(&list.items[i], scope));
    }

    DA_ADD(&scope->code, INSTR_ALLOC(LIST_BITS, list.len));
    return true;
}

bool kokos_expr_compile(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    kokos_code_t* code = &scope->code;

    switch (expr->type) {
    case EXPR_FLOAT_LIT: {
        uint64_t value = to_double_bytes(expr);
        DA_ADD(code, INSTR_PUSH(value));
        break;
    }
    case EXPR_INT_LIT: {
        uint64_t value = sv_atoi(expr->token.value);
        DA_ADD(code, INSTR_PUSH(TO_INT(value)));
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
            DA_ADD(code, INSTR_PUSH(special));
            break;
        }

        DA_ADD(code, INSTR_GET_LOCAL(kokos_string_store_add_sv(scope->string_store, expr->token.value)));
        break;
    }
    case EXPR_STRING_LIT: {
        kokos_compile_string_lit(expr, scope, code);
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

bool kokos_expr_compile_quoted(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    switch (expr->type) {
    case EXPR_INT_LIT:
    case EXPR_FLOAT_LIT:
    case EXPR_STRING_LIT:
    case EXPR_MAP:
    case EXPR_IDENT:
    case EXPR_VECTOR:     {
        return kokos_expr_compile(expr, scope);
    }
    case EXPR_LIST: {
        return compile_quoted_list(expr, scope);
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
    kokos_compiled_module_t* module, kokos_scope_t const* scope)
{
    KOKOS_ASSERT(scope->parent == NULL); // ensuring that the module's scope is toplevel!

    module->call_locations = scope->call_locations;
    module->string_store = *scope->string_store;
    module->procs = scope->procs;
    /*module->macros = scope->macros;*/
    module->instructions = scope->code;
    module->top_level_code_start = module->instructions.len;

    DA_ADD(&module->instructions, INSTR_RET);
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
