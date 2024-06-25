#include "ast.h"
#include "hash.h"
#include "macros.h"
#include "native.h"
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

void kokos_ctx_add_proc(
    kokos_compiler_context_t* ctx, const char* name, kokos_compiled_proc_t* proc)
{
    ht_add(&ctx->procedures, (void*)name, proc);
}

void kokos_ctx_add_local(kokos_compiler_context_t* ctx, string_view local)
{
    DA_ADD(&ctx->locals, local);
}

static bool compile_procedure_def(
    const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    (void)code;

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

    kokos_compiler_context_t new_ctx = kokos_ctx_empty();
    new_ctx.parent = ctx;
    for (size_t i = 0; i < params.len; i++) {
        kokos_ctx_add_local(&new_ctx, params.names[i]);
    }

    // Add the procedure before the body compilation to allow recursion
    kokos_compiled_proc_t* proc = KOKOS_ALLOC(sizeof(kokos_compiled_proc_t));
    const char* name_str = sv_dup(name);
    proc->params = params;

    kokos_ctx_add_proc(ctx, name_str, proc);

    size_t proc_ip = ctx->procedure_code.len;
    for (size_t i = 3; i < list.len; i++) {
        TRY(kokos_expr_compile(list.items[i], &new_ctx, &ctx->procedure_code));
    }

    proc->ip = proc_ip;
    DA_ADD(&ctx->procedure_code, INSTR_RET);
    return true;
}

static size_t kokos_ctx_get_var_idx(const kokos_compiler_context_t* ctx, string_view name)
{
    kokos_variable_list_t locals = ctx->locals;
    for (size_t i = 0; i < locals.len; i++) {
        if (sv_eq(name, locals.items[i])) {
            return i;
        }
    }
    return (size_t)-1;
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
    const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    kokos_list_t elements = expr->list;
    for (size_t i = elements.len - 1; i > 0; i--) {
        const kokos_expr_t* elem = elements.items[i];
        TRY(kokos_expr_compile(elem, ctx, code));
    }

    return true;
}

static bool compile_all_args(
    const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    kokos_list_t elements = expr->list;
    for (size_t i = 1; i < elements.len; i++) {
        const kokos_expr_t* elem = elements.items[i];
        TRY(kokos_expr_compile(elem, ctx, code));
    }

    return true;
}

static bool compile_mul(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    TRY(compile_all_args(expr, ctx, code));

    DA_ADD(code, INSTR_MUL(expr->list.len - 1));
    return true;
}

static bool compile_div(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    TRY(compile_all_args(expr, ctx, code));

    DA_ADD(code, INSTR_DIV(expr->list.len - 1));
    return true;
}
static bool compile_sub(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    TRY(compile_all_args(expr, ctx, code));

    DA_ADD(code, INSTR_SUB(expr->list.len - 1));
    return true;
}

static bool compile_add(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    TRY(compile_all_args(expr, ctx, code));

    DA_ADD(code, INSTR_ADD(expr->list.len - 1));
    return true;
}

static bool compile_if(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    if (expr->list.len < 3 || expr->list.len > 4) {

        set_error(expr->token.location, "badly formed 'if' expression");
        return false;
    }

    kokos_list_t exprs = expr->list;

    TRY(kokos_expr_compile(exprs.items[1], ctx, code));

    DA_ADD(code, INSTR_JZ(0));

    size_t start_ip = code->len;
    TRY(kokos_expr_compile(exprs.items[2], ctx, code));

    switch (expr->list.len) {
    case 4: {
        DA_ADD(code, INSTR_BRANCH(0));

        size_t consequence_ip = code->len - start_ip;
        TRY(kokos_expr_compile(exprs.items[3], ctx, code));

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

static bool compile_eq(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, ctx, code));

    TRY(compile_all_args(expr, ctx, code));

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_EQ(0));

    return true;
}

static bool compile_neq(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, ctx, code));

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_NEQ(0));

    return true;
}

static bool compile_lte(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, ctx, code));

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_NEQ(1));

    return true;
}

static bool compile_gte(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, ctx, code));

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_NEQ((uint64_t)-1));

    return true;
}

static bool compile_gt(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, ctx, code));

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_EQ(1));

    return true;
}

static bool compile_lt(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    TRY(expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL));

    TRY(compile_all_args(expr, ctx, code));

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_EQ((uint64_t)-1));

    return true;
}

bool compile_let(kokos_expr_t const* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    KOKOS_ASSERT(expr->type == EXPR_LIST);

    VERIFY_TYPE(expr->list.items[1], EXPR_LIST);
    kokos_list_t vars = expr->list.items[1]->list;

    for (size_t i = 0; i < vars.len; i += 2) {
        if (ctx->locals.len >= MAX_LOCALS) {
            set_error(vars.items[i]->token.location, "local limit exceeded");
            return false;
        }

        kokos_expr_t const* key = vars.items[i];
        VERIFY_TYPE(key, EXPR_IDENT);

        kokos_expr_t const* value = vars.items[i + 1];
        TRY(kokos_expr_compile(value, ctx, code));

        DA_ADD(&ctx->locals, key->token.value);
        DA_ADD(code, INSTR_STORE_LOCAL(ctx->locals.len - 1));
    }

    // compile body with the new bindings
    for (size_t i = 0; i < expr->list.len - 2; i++) {
        TRY(kokos_expr_compile(expr->list.items[i + 2], ctx, code));
    }

    return true;
}

typedef bool (*kokos_sform_t)(
    const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code);

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
    { "!=", compile_neq },
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

static void kokos_compile_string_lit(
    const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{

    string_view value = expr->token.value;
    char* str = KOKOS_ALLOC(sizeof(char) * value.size);
    memcpy(str, value.ptr, value.size * sizeof(char));

    kokos_runtime_string_t* string = KOKOS_ALLOC(sizeof(kokos_runtime_string_t));
    string->ptr = str;
    string->len = value.size;
    DA_ADD(&ctx->string_store, string);

    uint64_t string_value = STRING_BITS | (uint64_t)string;
    DA_ADD(code, INSTR_PUSH(string_value));
}

static bool compile_list(
    const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    kokos_list_t list = expr->list;
    if (list.len == 0) {
        DA_ADD(code, INSTR_ALLOC(LIST_BITS, 0));
        return true;
    }

    string_view head = list.items[0]->token.value;

    kokos_sform_t sform = get_sform(head);
    if (sform) {
        TRY(sform(expr, ctx, code));
        return true;
    }

    kokos_native_proc_t native = kokos_find_native(head);
    if (native) {
        TRY(compile_all_args_reversed(expr, ctx, code));

        DA_ADD(code, INSTR_CALL_NATIVE(list.len - 1, (uint64_t)native));
        return true;
    }

    KOKOS_VERIFY(head.size < 256);

    char head_buf[256];
    sprintf(head_buf, SV_FMT, SV_ARG(head));
    head_buf[head.size] = '\0';

    kokos_compiled_proc_t* proc = kokos_ctx_get_proc(ctx, head_buf);
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
            TRY(kokos_expr_compile(list.items[i], ctx, code));
        }

        for (size_t j = list.len - 1; j >= i; j--) {
            TRY(kokos_expr_compile(list.items[j], ctx, code));
        }

        DA_ADD(code, INSTR_ALLOC(VECTOR_BITS, list.len - proc->params.len));
        DA_ADD(code, INSTR_CALL(proc->params.len, proc->ip));
        return true;
    }

    if (proc->params.len != list.len - 1) {
        set_error(expr->token.location,
            "expected %lu arguments for procedure '" SV_FMT "', got %lu instead", proc->params.len,
            SV_ARG(head), list.len - 1);
        return false;
    }

    TRY(compile_all_args_reversed(expr, ctx, code));

    DA_ADD(code, INSTR_CALL(proc->params.len, proc->ip));
    return true;
}

bool kokos_expr_compile(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
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
        return compile_list(expr, ctx, code);
    }
    case EXPR_IDENT: {
        uint64_t special;
        if (get_special_value(expr->token.value, &special)) {
            DA_ADD(code, INSTR_PUSH(special));
            break;
        }

        int64_t local_idx = kokos_ctx_get_var_idx(ctx, expr->token.value);
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
        kokos_compile_string_lit(expr, ctx, code);
        break;
    }
    case EXPR_MAP: {
        kokos_map_t map = expr->map;
        for (size_t i = 0; i < map.len; i++) {
            TRY(kokos_expr_compile(map.keys[i], ctx, code));
            TRY(kokos_expr_compile(map.values[i], ctx, code));
        }

        DA_ADD(code, INSTR_ALLOC(MAP_BITS, map.len));
        break;
    }
    case EXPR_VECTOR: {
        kokos_vec_t vec = expr->vec;
        for (ssize_t i = vec.len - 1; i >= 0; i--) {
            TRY(kokos_expr_compile(vec.items[i], ctx, code));
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

kokos_code_t kokos_compile_program(kokos_program_t program, kokos_compiler_context_t* ctx)
{
    kokos_code_t code;
    DA_INIT(&code, 0, 10);

    for (size_t i = 0; i < program.len; i++) {
        if (!kokos_expr_compile(program.items[i], ctx, &code)) {
            DA_FREE(&code);
            break;
        }
    }

    return code;
}

static uint64_t string_hash_func(const void* ptr)
{
    return hash_djb2((const char*)ptr);
}

static bool string_eq_func(const void* lhs, const void* rhs)
{
    const char* ls = (const char*)lhs;
    const char* rs = (const char*)rhs;

    return strcmp(ls, rs) == 0;
}

kokos_compiler_context_t kokos_ctx_empty(void)
{
    hash_table functions = ht_make(string_hash_func, string_eq_func, 11);
    kokos_variable_list_t vars;
    kokos_string_store_t strings;
    kokos_code_t code;
    DA_INIT(&vars, 0, 7);
    DA_INIT(&strings, 0, 7);
    DA_INIT(&code, 0, 77);

    return (kokos_compiler_context_t) { .procedures = functions,
        .locals = vars,
        .parent = NULL,
        .string_store = strings,
        .procedure_code = code };
}

kokos_compiled_proc_t* kokos_ctx_get_proc(kokos_compiler_context_t* ctx, const char* name)
{
    if (!ctx)
        return NULL;

    kokos_compiled_proc_t* proc = ht_find(&ctx->procedures, name);
    if (!proc)
        return kokos_ctx_get_proc(ctx->parent, name);

    return proc;
}
