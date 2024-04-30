#include "macros.h"
#include <stdio.h>

#define BASE_ALLOC KOKOS_ALLOC
#include "base.h"

#include "compile.h"
#include "instruction.h"
#include "value.h"

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

static void compile_procedure_def(const kokos_expr_t* expr, kokos_compiler_context_t* ctx)
{
    kokos_list_t list = expr->list;
    string_view name = list.items[1]->token.value;

    const kokos_expr_t* params_expr = list.items[2];
    KOKOS_VERIFY(params_expr->type == EXPR_LIST);

    kokos_params_t params = expr_to_params(params_expr);

    kokos_compiler_context_t new_ctx = kokos_empty_compiler_context();
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

static void compile_push_expr(
    const kokos_expr_t* expr, const kokos_compiler_context_t* ctx, kokos_code_t* code)
{
    switch (expr->type) {
    case EXPR_FLOAT_LIT:
    case EXPR_INT_LIT:   {
        uint64_t num = to_double_bytes(expr);
        DA_ADD(code, INSTR_PUSH(num));
        break;
    }
    case EXPR_IDENT: {
        int64_t local_idx = kokos_ctx_get_var_idx(ctx, expr->token.value);
        KOKOS_VERIFY(local_idx != (size_t)-1);

        DA_ADD(code, INSTR_PUSH_LOCAL(local_idx));
        break;
    }
    default: KOKOS_TODO();
    }
}

static void compile_add(kokos_list_t list, kokos_compiler_context_t* ctx, kokos_code_t* code)
{

    for (size_t i = 1; i < list.len; i++) {
        const kokos_expr_t* elem = list.items[i];
        compile_push_expr(elem, ctx, code);
    }

    DA_ADD(code, INSTR_ADD(list.len - 1));
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

        if (sv_eq_cstr(head, "proc")) {
            compile_procedure_def(expr, ctx);
            break;
        }

        if (sv_eq_cstr(head, "+")) {
            compile_add(list, ctx, code);
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
            compile_push_expr(list.items[i], ctx, code);
        }

        DA_ADD(code, INSTR_CALL((uint64_t)proc->name));

        break;
    }
    default: KOKOS_TODO();
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

kokos_compiler_context_t kokos_empty_compiler_context(void)
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
