#include "macros.h"
#include "hash.h"
#include "native.h"
#include "token.h"

#include "base.h"

#include "compile.h"
#include "instruction.h"
#include "value.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

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

// TODO: handle variadics
bool expr_to_params(const kokos_expr_t* expr, kokos_params_t* params)
{
    kokos_list_t list = expr->list;

    struct {
        string_view* items;
        size_t len;
        size_t cap;
    } names;
    DA_INIT_ZEROED(&names, list.len);

    for (size_t i = 0; i < list.len; i++) {
        const kokos_expr_t* param = list.items[i];
        if (param->type != EXPR_IDENT) {
            set_error(param->token.location, "procedure parameters must all be valid identifiers");
            return false;
        }

        DA_ADD(&names, param->token.value);
    }

    *params = (kokos_params_t) { .names = names.items, .len = names.len, .variadic = false };
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
    if (!expr_to_params(params_expr, &params)) {
        return false;
    }

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
        if (!kokos_expr_compile(list.items[i], &new_ctx, &ctx->procedure_code)) {
            return false;
        }
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

// TODO: refactor this
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

static bool compile_all_args(
    const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    kokos_list_t elements = expr->list;
    for (size_t i = 1; i < elements.len; i++) {
        const kokos_expr_t* elem = elements.items[i];
        if (!kokos_expr_compile(elem, ctx, code)) {
            return false;
        }
    }

    return true;
}

static bool compile_mul(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    if (!compile_all_args(expr, ctx, code)) {
        return false;
    }

    DA_ADD(code, INSTR_MUL(expr->list.len - 1));
    return true;
}

static bool compile_div(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    if (!compile_all_args(expr, ctx, code)) {
        return false;
    }

    DA_ADD(code, INSTR_DIV(expr->list.len - 1));
    return true;
}
static bool compile_sub(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    if (!compile_all_args(expr, ctx, code)) {
        return false;
    }

    DA_ADD(code, INSTR_SUB(expr->list.len - 1));
    return true;
}

static bool compile_add(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    if (!compile_all_args(expr, ctx, code)) {
        return false;
    }

    DA_ADD(code, INSTR_ADD(expr->list.len - 1));
    return true;
}

static bool compile_if(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    // TODO: support other forms of if expression and handle an error
    KOKOS_VERIFY(expr->list.len == 4);

    kokos_list_t exprs = expr->list;

    if (!kokos_expr_compile(exprs.items[1], ctx, code)) {
        return false;
    }

    DA_ADD(code, INSTR_JZ(0));

    size_t start_ip = code->len;
    if (!kokos_expr_compile(exprs.items[2], ctx, code)) {
        return false;
    }

    DA_ADD(code, INSTR_BRANCH(0));

    size_t consequence_ip = code->len - start_ip;
    if (!kokos_expr_compile(exprs.items[3], ctx, code)) {
        return false;
    }

    KOKOS_ASSERT(code->items[start_ip - 1].type == I_JZ);
    code->items[start_ip - 1].operand = consequence_ip + 1; // patch the jump instruction

    KOKOS_ASSERT(code->items[consequence_ip + start_ip - 1].type == I_BRANCH);
    code->items[consequence_ip + start_ip - 1].operand
        = code->len - (consequence_ip + start_ip) + 1; // patch the branch instruction

    return true;
}

static bool compile_eq(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    if (!expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL)) {
        return false;
    }

    if (!compile_all_args(expr, ctx, code)) {
        return false;
    }

    if (!compile_all_args(expr, ctx, code)) {
        return false;
    }

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_EQ(0));

    return true;
}

static bool compile_neq(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    if (!expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL)) {
        return false;
    }

    if (!compile_all_args(expr, ctx, code)) {
        return false;
    }

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_NEQ(0));

    return true;
}

static bool compile_lte(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    if (!expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL)) {
        return false;
    }

    if (!compile_all_args(expr, ctx, code)) {
        return false;
    }

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_NEQ(1));

    return true;
}

static bool compile_gte(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    if (!expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL)) {
        return false;
    }

    if (!compile_all_args(expr, ctx, code)) {
        return false;
    }

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_NEQ((uint64_t)-1));

    return true;
}

static bool compile_gt(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    if (!expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL)) {
        return false;
    }

    if (!compile_all_args(expr, ctx, code)) {
        return false;
    }

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_EQ(1));

    return true;
}

static bool compile_lt(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    if (!expect_arity(expr->token.location, 3, expr->list.len, P_EQUAL)) {
        return false;
    }

    if (!compile_all_args(expr, ctx, code)) {
        return false;
    }

    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_EQ((uint64_t)-1));

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

    kokos_string_t* string = KOKOS_ALLOC(sizeof(kokos_string_t));
    string->ptr = str;
    string->len = value.size;
    DA_ADD(&ctx->string_store, string);

    uint64_t string_value = STRING_BITS | (uint64_t)string;
    DA_ADD(code, INSTR_PUSH(string_value));
}

bool kokos_expr_compile(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    switch (expr->type) {
    case EXPR_FLOAT_LIT:
    case EXPR_INT_LIT:   {
        uint64_t value = to_double_bytes(expr);
        DA_ADD(code, INSTR_PUSH(value));
        break;
    }
    case EXPR_LIST: {
        kokos_list_t list = expr->list;
        KOKOS_VERIFY(list.len > 0); // TODO: handle empty list
        string_view head = list.items[0]->token.value;

        kokos_sform_t sform = get_sform(head);
        if (sform) {
            if (!sform(expr, ctx, code)) {
                return false;
            }
            break;
        }

        kokos_native_proc_t native = kokos_find_native(head);
        if (native) {
            compile_all_args(expr, ctx, code);
            DA_ADD(code, INSTR_CALL_NATIVE((uint64_t)native));
            break;
        }

        KOKOS_VERIFY(head.size < 256);

        char head_buf[256];
        sprintf(head_buf, SV_FMT, (int)head.size, head.ptr);
        head_buf[head.size] = '\0';

        kokos_compiled_proc_t* proc = kokos_ctx_get_proc(ctx, head_buf);
        if (!proc) {
            set_error(
                expr->token.location, "undefined procedure '" SV_FMT "'", (int)head.size, head.ptr);
            return false;
        }

        if (proc->params.len != list.len - 1) {
            set_error(expr->token.location,
                "expected %lu arguments for procedure '" SV_FMT "', got %lu instead",
                proc->params.len, (int)head.size, head.ptr, list.len - 1);
            return false;
        }

        for (size_t i = 1; i < list.len; i++) {
            if (!kokos_expr_compile(list.items[i], ctx, code)) {
                return false;
            }
        }

        DA_ADD(code, INSTR_CALL((proc->params.len << 32) | proc->ip));
        break;
    }
    case EXPR_IDENT: {
        uint64_t special;
        if (get_special_value(expr->token.value, &special)) {
            DA_ADD(code, INSTR_PUSH(special));
            break;
        }

        int64_t local_idx = kokos_ctx_get_var_idx(ctx, expr->token.value);
        KOKOS_VERIFY(local_idx != (size_t)-1);

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
            if (!kokos_expr_compile(map.keys[i], ctx, code)) {
                return false;
            }
            if (!kokos_expr_compile(map.values[i], ctx, code)) {
                return false;
            }
        }

        DA_ADD(code, INSTR_ALLOC(MAP_BITS, map.len));
        break;
    }
    case EXPR_VECTOR: {
        kokos_vec_t vec = expr->vec;
        for (ssize_t i = vec.len - 1; i >= 0; i--) {
            if (!kokos_expr_compile(vec.items[i], ctx, code)) {
                return false;
            }
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
    hash_table functions = ht_make(string_hash_func, string_eq_func, 1);
    kokos_variable_list_t vars;
    kokos_string_store_t strings;
    kokos_code_t code;
    DA_INIT(&vars, 0, 0);
    DA_INIT(&strings, 0, 0);
    DA_INIT(&code, 0, 0);

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
