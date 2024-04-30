#include "macros.h"
#include "token.h"

#include "base.h"

#include "compile.h"
#include "instruction.h"
#include "value.h"

#include <stdio.h>
#include <string.h>

static uint64_t to_double_bytes(const kokos_expr_t* expr)
{
    double parsed = sv_atof(expr->token.value);
    return TO_VALUE(parsed).as_int;
}

// TODO: handle variadics
kokos_params_t expr_to_params(const kokos_expr_t* expr)
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
        KOKOS_VERIFY(param->type == EXPR_IDENT);

        DA_ADD(&names, param->token.value);
    }

    return (kokos_params_t) { .names = names.items, .len = names.len, .variadic = false };
}

void kokos_ctx_add_proc(
    kokos_compiler_context_t* ctx, const char* name, kokos_compiled_proc_t* proc)
{
    ht_add(&ctx->functions, (void*)name, proc);
}

void kokos_ctx_add_local(kokos_compiler_context_t* ctx, string_view local)
{
    DA_ADD(&ctx->locals, local);
}

static void compile_procedure_def(
    const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    (void)code;

    kokos_list_t list = expr->list;
    string_view name = list.items[1]->token.value;

    const kokos_expr_t* params_expr = list.items[2];
    KOKOS_VERIFY(params_expr->type == EXPR_LIST);

    kokos_params_t params = expr_to_params(params_expr);

    kokos_compiler_context_t new_ctx = kokos_ctx_empty();
    new_ctx.parent = ctx;
    for (size_t i = 0; i < params.len; i++) {
        kokos_ctx_add_local(&new_ctx, params.names[i]);
    }

    // Add the procedure before the body compilation to allow recursion
    kokos_compiled_proc_t* proc = KOKOS_ALLOC(sizeof(kokos_compiled_proc_t));
    const char* name_str = sv_dup(name);
    proc->name = name_str;
    proc->params = params;

    kokos_ctx_add_proc(ctx, proc->name, proc);

    kokos_code_t body;
    DA_INIT(&body, 0, 5);

    for (size_t i = 3; i < list.len; i++) {
        kokos_expr_compile(list.items[i], &new_ctx, &body);
    }

    proc->body = body;
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

static void push_all_args(
    const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    kokos_list_t elements = expr->list;
    for (size_t i = 1; i < elements.len; i++) {
        const kokos_expr_t* elem = elements.items[i];
        kokos_expr_compile(elem, ctx, code);
    }
}

static void compile_sub(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    push_all_args(expr, ctx, code);
    DA_ADD(code, INSTR_SUB(expr->list.len - 1));
}

static void compile_add(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    push_all_args(expr, ctx, code);
    DA_ADD(code, INSTR_ADD(expr->list.len - 1));
}

static void compile_if(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    // TODO: support other forms of if expression and handle an error
    KOKOS_VERIFY(expr->list.len == 4);

    kokos_list_t exprs = expr->list;

    kokos_expr_compile(exprs.items[1], ctx, code);
    DA_ADD(code, INSTR_JZ(0));

    size_t ip = code->len;
    kokos_expr_compile(exprs.items[2], ctx, code);
    DA_ADD(code, INSTR_BRANCH(0));

    size_t nzip = code->len - ip;
    kokos_expr_compile(exprs.items[3], ctx, code);

    KOKOS_ASSERT(code->items[ip - 1].type == I_JZ);
    code->items[ip - 1].operand = nzip + 1; // patch the jump instruction

    KOKOS_ASSERT(code->items[nzip + ip - 1].type == I_BRANCH);
    code->items[nzip + ip - 1].operand = code->len - nzip - 1; // patch the branch instruction
}

static void compile_eq(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    KOKOS_ASSERT(expr->list.len == 3);
    push_all_args(expr, ctx, code);
    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_EQ(0));
}

static void compile_neq(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    KOKOS_ASSERT(expr->list.len == 3);
    push_all_args(expr, ctx, code);
    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_NEQ(0));
}

static void compile_lte(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    KOKOS_ASSERT(expr->list.len == 3);
    push_all_args(expr, ctx, code);
    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_NEQ(1));
}

static void compile_gte(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    KOKOS_ASSERT(expr->list.len == 3);
    push_all_args(expr, ctx, code);
    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_NEQ((uint64_t)-1));
}

static void compile_gt(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    KOKOS_ASSERT(expr->list.len == 3);
    push_all_args(expr, ctx, code);
    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_EQ(1));
}

static void compile_lt(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    KOKOS_ASSERT(expr->list.len == 3);
    push_all_args(expr, ctx, code);
    DA_ADD(code, INSTR_CMP);
    DA_ADD(code, INSTR_EQ((uint64_t)-1));
}

typedef void (*kokos_sform_t)(
    const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code);

typedef struct {
    const char* name;
    kokos_sform_t sform;
} kokos_sform_pair_t;

static kokos_sform_pair_t sforms[] = {
    { "proc", compile_procedure_def },
    { "+", compile_add },
    { "-", compile_sub },
    { "if", compile_if },
    { "<", compile_lt },
    { ">", compile_gt },
    { "<=", compile_lte },
    { ">=", compile_gte },
    { "=", compile_eq },
    { "!=", compile_neq },
};

#define SFORMS_COUNT (sizeof(sforms) / sizeof(kokos_sform_pair_t))

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

void kokos_expr_compile(const kokos_expr_t* expr, kokos_compiler_context_t* ctx, kokos_code_t* code)
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
        KOKOS_VERIFY(list.len > 0);
        string_view head = list.items[0]->token.value;

        kokos_sform_t sform = get_sform(head);
        if (sform) {
            sform(expr, ctx, code);
            break;
        }

        KOKOS_VERIFY(head.size < 256);

        char head_buf[256];
        sprintf(head_buf, SV_FMT, (int)head.size, head.ptr);
        head_buf[head.size] = '\0';

        kokos_compiled_proc_t* proc = kokos_ctx_get_proc(ctx, head_buf);
        KOKOS_ASSERT(proc);
        KOKOS_ASSERT(proc->params.len == list.len - 1);

        for (size_t i = 1; i < list.len; i++) {
            kokos_expr_compile(list.items[i], ctx, code);
        }

        DA_ADD(code, INSTR_CALL((uint64_t)proc->name));
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

        DA_ADD(code, INSTR_PUSH_LOCAL(local_idx));
        break;
    }
    default: {
        char buf[128] = { 0 };
        sprintf(buf, "compilation of expression %s is not implemented",
            kokos_expr_type_str(expr->type));
        KOKOS_TODO(buf);
    }
    }
}

kokos_code_t kokos_compile_program(kokos_program_t program, kokos_compiler_context_t* ctx)
{
    kokos_code_t code;
    DA_INIT(&code, 0, 10);

    for (size_t i = 0; i < program.len; i++) {
        kokos_expr_compile(program.items[i], ctx, &code);
    }

    return code;
}

// TODO: implement this function
static int64_t string_hash_func(const void* str)
{
    return 1337;
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
    DA_INIT(&vars, 0, 0);

    return (kokos_compiler_context_t) { .functions = functions, .locals = vars, .parent = NULL };
}

kokos_compiled_proc_t* kokos_ctx_get_proc(kokos_compiler_context_t* ctx, const char* name)
{
    if (!ctx)
        return NULL;

    kokos_compiled_proc_t* proc = ht_find(&ctx->functions, name);
    if (!proc)
        return kokos_ctx_get_proc(ctx->parent, name);

    return proc;
}
