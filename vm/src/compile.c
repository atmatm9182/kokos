#include "ast.h"
#include "hash.h"
#include "macros.h"
#include "native.h"
#include "src/runtime.h"
#include "token.h"
#include "vmconstants.h"

#include "base.h"

#include "compile.h"
#include "instruction.h"
#include "value.h"

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

bool expr_to_params(const kokos_expr_t* expr, kokos_params_t* params)
{
    kokos_list_t list = expr->list;

    struct {
        string_view* items;
        size_t len;
        size_t cap;
    } names;
    DA_INIT_ZEROED(&names, list.len);

    bool variadic = false;
    for (size_t i = 0; i < list.len; i++) {
        const kokos_expr_t* param = list.items[i];
        VERIFY_TYPE(param, EXPR_IDENT);

        if (sv_eq_cstr(param->token.value, "&")) {
            if (i != list.len - 2) {
                set_error(expr->token.location, "malformed variadic paramater list");
                return false;
            }

            variadic = true;
            continue;
        }

        DA_ADD(&names, param->token.value);
    }

    *params = (kokos_params_t) { .names = names.items, .len = names.len, .variadic = variadic };
    return true;
}

void kokos_scope_add_proc(kokos_scope_t* scope, const char* name, kokos_compiled_proc_t* proc)
{
    ht_add(&scope->procs, (void*)name, proc);
}

void kokos_scope_add_local(kokos_scope_t* scope, string_view local)
{
    DA_ADD(&scope->vars, local);
}

static kokos_label_t kokos_scope_make_label(kokos_scope_t* scope)
{
    return scope->proc_code->len;
}

static bool kokos_scope_is_top_level(kokos_scope_t const* scope)
{
    return scope->current_code == scope->top_level_code;
}

static bool compile_procedure_def(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    kokos_list_t list = expr->list;
    if (list.len < 3) {
        set_error(expr->token.location,
            "procedure definition must have at least a name and an argument list");
        return false;
    }

    string_view name = list.items[1]->token.value;

    const kokos_expr_t* params_expr = list.items[2];
    if (params_expr->type != EXPR_LIST) {
        set_error(params_expr->token.location,
            "cannot use value of type %s as an argument list for a procedure",
            kokos_expr_type_str(params_expr->type));
        return false;
    }

    kokos_params_t params;
    TRY(expr_to_params(params_expr, &params));

    kokos_scope_t proc_scope = kokos_scope_empty(scope, false);
    for (size_t i = 0; i < params.len; i++) {
        kokos_scope_add_local(&proc_scope, params.names[i]);
    }

    // Add the procedure before the body compilation to allow recursion
    kokos_compiled_proc_t* proc = KOKOS_ALLOC(sizeof(kokos_compiled_proc_t));
    const char* name_str = sv_dup(name);
    proc->params = params;
    proc->label = kokos_scope_make_label(scope);

    kokos_scope_add_proc(scope, name_str, proc);

    for (size_t i = 3; i < list.len; i++) {
        TRY(kokos_expr_compile(list.items[i], &proc_scope));
    }

    DA_ADD(proc_scope.current_code, INSTR_RET);
    return true;
}

static size_t kokos_scope_get_var_idx(const kokos_scope_t* scope, string_view name)
{
    if (!scope) {
        return (size_t)-1;
    }

    kokos_variable_list_t vars = scope->vars;
    for (size_t i = 0; i < vars.len; i++) {
        if (sv_eq(name, vars.items[i])) {
            return i;
        }
    }

    return kokos_scope_get_var_idx(scope->parent, name);
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

static bool compile_all_args_reversed(
    const kokos_expr_t* expr, kokos_scope_t* scope, kokos_code_t* code)
{
    kokos_list_t elements = expr->list;
    for (size_t i = elements.len - 1; i > 0; i--) {
        const kokos_expr_t* elem = elements.items[i];
        TRY(kokos_expr_compile(elem, scope));
    }

    return true;
}

static bool compile_all_args(const kokos_expr_t* expr, kokos_scope_t* scope, kokos_code_t* code)
{
    kokos_list_t elements = expr->list;
    for (size_t i = 1; i < elements.len; i++) {
        const kokos_expr_t* elem = elements.items[i];
        TRY(kokos_expr_compile(elem, scope));
    }

    return true;
}

static bool compile_mul(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    kokos_code_t* code = scope->current_code;
    TRY(compile_all_args(expr, scope, code));

    DA_ADD(code, INSTR_MUL(expr->list.len - 1));
    return true;
}

static bool compile_div(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    kokos_code_t* code = scope->current_code;
    TRY(compile_all_args(expr, scope, code));

    DA_ADD(code, INSTR_DIV(expr->list.len - 1));
    return true;
}
static bool compile_sub(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    kokos_code_t* code = scope->current_code;
    TRY(compile_all_args(expr, scope, code));

    DA_ADD(code, INSTR_SUB(expr->list.len - 1));
    return true;
}

static bool compile_add(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    kokos_code_t* code = scope->current_code;
    TRY(compile_all_args(expr, scope, code));

    DA_ADD(code, INSTR_ADD(expr->list.len - 1));
    return true;
}

static bool compile_if(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    kokos_code_t* code = scope->current_code;
    if (expr->list.len < 3 || expr->list.len > 4) {
        set_error(expr->token.location, "badly formed 'if' expression");
        return false;
    }

    kokos_list_t exprs = expr->list;

    TRY(kokos_expr_compile(exprs.items[1], scope));

    DA_ADD(code, INSTR_JZ(0));

    kokos_scope_t consequence_scope = kokos_scope_empty(scope, kokos_scope_is_top_level(scope));

    size_t start_ip = code->len;
    TRY(kokos_expr_compile(exprs.items[2], &consequence_scope));

    switch (expr->list.len) {
    case 4: {
        DA_ADD(code, INSTR_BRANCH(0));

        kokos_scope_t alternative_scope = kokos_scope_empty(scope, kokos_scope_is_top_level(scope));

        size_t consequence_ip = code->len - start_ip;
        TRY(kokos_expr_compile(exprs.items[3], &alternative_scope));

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

static bool compile_eq(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    kokos_code_t* code = scope->current_code;
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, scope, code));

    TRY(compile_all_args(expr, scope, code));

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_EQ(0));

    return true;
}

static bool compile_neq(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    kokos_code_t* code = scope->current_code;
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, scope, code));

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_NEQ(0));

    return true;
}

static bool compile_lte(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    kokos_code_t* code = scope->current_code;
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, scope, code));

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_NEQ(1));

    return true;
}

static bool compile_gte(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    kokos_code_t* code = scope->current_code;
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, scope, code));

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_NEQ((uint64_t)-1));

    return true;
}

static bool compile_gt(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    kokos_code_t* code = scope->current_code;
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, scope, code));

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_EQ(1));

    return true;
}

static bool compile_lt(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    kokos_code_t* code = scope->current_code;
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, scope, code));

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_EQ((uint64_t)-1));

    return true;
}

// NOTE: this is kinda wierd. `let` creates a new scope, but the locals will still
// be stored into the `locals` array of the current function frame
static bool compile_let(kokos_expr_t const* expr, kokos_scope_t* scope)
{
    KOKOS_ASSERT(expr->type == EXPR_LIST);

    VERIFY_TYPE(expr->list.items[1], EXPR_LIST);
    kokos_list_t vars = expr->list.items[1]->list;

    kokos_scope_t let_scope = kokos_scope_empty(scope, kokos_scope_is_top_level(scope));

    for (size_t i = 0; i < vars.len; i += 2) {
        if (scope->vars.len >= MAX_LOCALS) {
            set_error(vars.items[i]->token.location, "local limit exceeded");
            return false;
        }

        kokos_expr_t const* key = vars.items[i];
        VERIFY_TYPE(key, EXPR_IDENT);

        kokos_expr_t const* value = vars.items[i + 1];
        TRY(kokos_expr_compile(value, &let_scope));

        DA_ADD(&let_scope.vars, key->token.value);
        DA_ADD(let_scope.current_code, INSTR_STORE_LOCAL(scope->vars.len - 1));
    }

    // compile body with the new bindings
    for (size_t i = 0; i < expr->list.len - 2; i++) {
        TRY(kokos_expr_compile(expr->list.items[i + 2], &let_scope));
    }

    return true;
}

typedef bool (*kokos_sform_t)(kokos_expr_t const* expr, kokos_scope_t* scope);

typedef struct {
    const char* name;
    kokos_sform_t sform;
} kokos_sform_pair_t;

static kokos_sform_pair_t sforms[] = {
    { "proc", compile_procedure_def },
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

    ht_add(scope->call_locations, (void*)ip, tok);
}

static void kokos_compile_string_lit(
    const kokos_expr_t* expr, kokos_scope_t* scope, kokos_code_t* code)
{
    string_view value = expr->token.value;

    kokos_runtime_string_t const* string;
    kokos_runtime_string_t const* existing = kokos_string_store_find(scope->string_store, value);
    if (existing) {
        string = existing;
        goto end;
    }

    char* str = KOKOS_ALLOC(sizeof(char) * value.size);
    memcpy(str, value.ptr, value.size * sizeof(char));

    kokos_runtime_string_t* s = kokos_runtime_string_new(str, value.size);
    string = kokos_string_store_add(scope->string_store, s);

end: {
    uint64_t string_value = STRING_BITS | (uint64_t)string;
    DA_ADD(code, INSTR_PUSH(string_value));
}
}

static bool compile_list(const kokos_expr_t* expr, kokos_scope_t* scope, kokos_code_t* code)
{
    kokos_list_t list = expr->list;
    if (list.len == 0) {
        DA_ADD(code, INSTR_ALLOC(LIST_BITS, 0));
        return true;
    }

    string_view head = list.items[0]->token.value;

    kokos_sform_t sform = get_sform(head);
    if (sform) {
        TRY(sform(expr, scope));
        return true;
    }

    kokos_native_proc_t native = kokos_find_native(head);
    if (native) {
        TRY(compile_all_args_reversed(expr, scope, code));

        DA_ADD(code, INSTR_CALL_NATIVE(list.len - 1, (uint64_t)native));
        return true;
    }

    KOKOS_VERIFY(head.size < 256);

    char head_buf[256];
    sprintf(head_buf, SV_FMT, SV_ARG(head));
    head_buf[head.size] = '\0';

    kokos_compiled_proc_t* proc = kokos_scope_get_proc(scope, head_buf);
    if (!proc) {
        set_error(list.items[0]->token.location, "undefined procedure '" SV_FMT "'", SV_ARG(head));
        return false;
    }

    if (proc->params.variadic) {
        if (list.len < proc->params.len) {
            set_error(expr->token.location,
                "expected at least %lu arguments for procedure '" SV_FMT "', got %lu instead",
                proc->params.len - 1, SV_ARG(head), list.len - 1);
            return false;
        }

        size_t i;
        for (i = 1; i < proc->params.len; i++) {
            TRY(kokos_expr_compile(list.items[i], scope));
        }

        for (size_t j = list.len - 1; j >= i; j--) {
            TRY(kokos_expr_compile(list.items[j], scope));
        }

        DA_ADD(code, INSTR_ALLOC(VECTOR_BITS, list.len - proc->params.len));

        goto success;
    }

    if (proc->params.len != list.len - 1) {
        set_error(expr->token.location,
            "expected %lu arguments for procedure '" SV_FMT "', got %lu instead", proc->params.len,
            SV_ARG(head), list.len - 1);
        return false;
    }

    TRY(compile_all_args_reversed(expr, scope, code));

success:
    scope_add_call_location(scope, code->len, expr->token);
    DA_ADD(code, INSTR_CALL(proc->params.len, proc->label));
    return true;
}

bool kokos_expr_compile(const kokos_expr_t* expr, kokos_scope_t* scope)
{
    kokos_code_t* code = scope->current_code;

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
        return compile_list(expr, scope, code);
    }
    case EXPR_IDENT: {
        uint64_t special;
        if (get_special_value(expr->token.value, &special)) {
            DA_ADD(code, INSTR_PUSH(special));
            break;
        }

        int64_t local_idx = kokos_scope_get_var_idx(scope, expr->token.value);
        if (local_idx == -1) {
            set_error(expr->token.location,
                "could not find variable " SV_FMT " in the current scope",
                SV_ARG(expr->token.value));
            return false;
        }

        DA_ADD(code, INSTR_PUSH_LOCAL((uint64_t)local_idx));
        break;
    }
    case EXPR_STRING_LIT: {
        kokos_compile_string_lit(expr, scope, code);
        break;
    }
    case EXPR_MAP: {
        kokos_map_t map = expr->map;
        for (size_t i = 0; i < map.len; i++) {
            TRY(kokos_expr_compile(map.keys[i], scope));
            TRY(kokos_expr_compile(map.values[i], scope));
        }

        DA_ADD(code, INSTR_ALLOC(MAP_BITS, map.len));
        break;
    }
    case EXPR_VECTOR: {
        kokos_vec_t vec = expr->vec;
        for (ssize_t i = vec.len - 1; i >= 0; i--) {
            TRY(kokos_expr_compile(vec.items[i], scope));
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

static void kokos_compiled_module_init_from_scope(
    kokos_compiled_module_t* module, kokos_scope_t const* scope)
{
    module->call_locations = *scope->call_locations;
    module->string_store = *scope->string_store;

    module->instructions = *scope->proc_code;

    module->top_level_code_start = module->instructions.len;

    for (size_t i = 0; i < scope->top_level_code->len; i++) {
        DA_ADD(&module->instructions, scope->top_level_code->items[i]);
    }

    DA_ADD(&module->instructions, INSTR_RET);
}

bool kokos_compile_module(
    kokos_module_t module, kokos_scope_t* scope, kokos_compiled_module_t* compiled_module)
{
    for (size_t i = 0; i < module.len; i++) {
        TRY(kokos_expr_compile(module.items[i], scope));
    }

    kokos_compiled_module_init_from_scope(compiled_module, scope);
    return true;
}

kokos_scope_t kokos_scope_empty(kokos_scope_t* parent, bool top_level)
{
    kokos_scope_t scope = { 0 };
    scope.procs = ht_make(
        hash_string_func, hash_string_eq_func, 11); // TODO: use actual hash and eq funcs here
    DA_INIT(&scope.vars, 0, 7); // seven is a lucky number :^)

    if (parent) {
        scope.parent = parent;
        scope.proc_code = parent->proc_code;
        scope.top_level_code = parent->top_level_code;
        scope.string_store = parent->string_store;
        scope.call_locations = parent->call_locations;
        scope.current_code = top_level ? parent->top_level_code : parent->proc_code;
        return scope;
    }

    scope.proc_code = KOKOS_ALLOC(sizeof(kokos_code_t));
    DA_INIT(scope.proc_code, 0, 200);

    scope.top_level_code = KOKOS_ALLOC(sizeof(kokos_code_t));
    DA_INIT(scope.top_level_code, 0, 30);

    scope.current_code = top_level ? scope.top_level_code : scope.proc_code;

    scope.string_store = KOKOS_ALLOC(sizeof(kokos_string_store_t));
    kokos_string_store_init(scope.string_store, 17);

    scope.call_locations = KOKOS_ALLOC(sizeof(hash_table));
    *scope.call_locations = ht_make(hash_sizet_func, hash_sizet_eq_func, 11);

    return scope;
}

kokos_compiled_proc_t* kokos_scope_get_proc(kokos_scope_t* scope, const char* name)
{
    if (!scope)
        return NULL;

    kokos_compiled_proc_t* proc = ht_find(&scope->procs, name);
    if (!proc)
        return kokos_scope_get_proc(scope->parent, name);

    return proc;
}
