#include "bytecode-assembler.h"

#define VERIFY_ARGS_COUNT(f, c)                                                                    \
    do {                                                                                           \
        if (args.len != (c)) {                                                                     \
            set_error(where, "expected %d forms for '" #f "'", (c));                               \
        }                                                                                          \
    } while (0);

#define COMP_ARGS() TRY(kokos_compile_all(args, scope, kokos_expr_compile))

KOKOS_DEFINE_SFORM(lambda, {
    VERIFY_ARGS_COUNT(lambda, 2);
    kokos_scope_t* lambda_scope = kokos_scope_derived(scope);

    kokos_runtime_proc_t* proc = KOKOS_ZALLOC(sizeof(kokos_runtime_proc_t));
    proc->type = PROC_KOKOS;

    TRY(kokos_expr_to_params(&args.items[0], &proc->kokos.params, scope));

    TRY(kokos_expr_compile(&args.items[1], lambda_scope));

    proc->kokos.code = lambda_scope->code;

    SET_SCOPE(lambda_scope);

    RET();

    SET_SCOPE(scope);

    /* DA_ADD(&proc->kokos.code, INSTR_PUSH(TO_PROC(proc).as_int)); */
    PUSH(TO_PROC(proc));
})

KOKOS_DEFINE_SFORM(var, {
    VERIFY_ARGS_COUNT(var, 2);

    VERIFY_TYPE(&args.items[0], EXPR_IDENT);

    TRY(kokos_expr_compile(&args.items[1], scope));

    kokos_runtime_string_t* name
        = (void*)kokos_string_store_add_sv(scope->string_store, args.items[0].token.value);
    ADD_LOCAL(name);
})

KOKOS_DEFINE_SFORM(proc, {
    if (args.len < 3) {
        KOKOS_VERIFY(0);
    }

    VERIFY_TYPE(&args.items[0], EXPR_IDENT);

    kokos_scope_t* lambda_scope = kokos_scope_derived(scope);

    kokos_runtime_proc_t* proc = KOKOS_ZALLOC(sizeof(kokos_runtime_proc_t));
    proc->type = PROC_KOKOS;

    TRY(kokos_expr_to_params(&args.items[1], &proc->kokos.params, scope));

    for (size_t i = 2; i < args.len; i++) {
        TRY(kokos_expr_compile(&args.items[i], lambda_scope));
    }

    SET_SCOPE(lambda_scope);

    RET();

    proc->kokos.code = lambda_scope->code;

    SET_SCOPE(scope);

    /* DA_ADD(&proc->kokos.code, INSTR_PUSH(TO_PROC(proc).as_int)); */
    PUSH(TO_PROC(proc));

    kokos_runtime_string_t* name
        = (void*)kokos_string_store_add_sv(scope->string_store, args.items[0].token.value);
    ADD_LOCAL(name);

    ht_add(&scope->procs, name, proc);
})

KOKOS_DEFINE_SFORM(macro, {
    if (args.len < 2) {
        set_error(expr->token.location,
            "macro definition must have at least a name and an argument list");
        return false;
    }

    const kokos_expr_t* params_expr = &args.items[1];
    if (params_expr->type != EXPR_LIST) {
        set_error(params_expr->token.location,
            "cannot use value of type %s as an argument list for a procedure",
            kokos_expr_type_str(params_expr->type));
        return false;
    }

    kokos_params_t params;
    TRY(kokos_expr_to_params(params_expr, &params, scope));

    kokos_scope_t* macro_scope = kokos_scope_derived(scope);

    // Add the macro before the body compilation to allow recursion
    kokos_macro_t* macro = KOKOS_ALLOC(sizeof(kokos_macro_t));
    macro->params = params;
    macro->name = (void*)kokos_string_store_add_sv(scope->string_store, args.items[0].token.value);

    ht_add(&scope->macros, macro->name, macro);

    for (size_t i = 2; i < args.len; i++) {
        TRY(kokos_expr_compile(&args.items[i], macro_scope));
    }

    SET_SCOPE(macro_scope);
    RET();

    macro->instructions = macro_scope->code;
})

KOKOS_DEFINE_SFORM(let, {
    VERIFY_TYPE(&args.items[0], EXPR_LIST);
    kokos_list_t vars = args.items[0].list;

    PUSH_SCOPE(vars.len / 2);

    for (size_t i = 0; i < vars.len; i += 2) {
        const kokos_expr_t* key = &vars.items[i];
        VERIFY_TYPE(key, EXPR_IDENT);

        const kokos_expr_t* value = &vars.items[i + 1];
        TRY(kokos_expr_compile(value, scope));

        kokos_runtime_string_t* var_name
            = (void*)kokos_string_store_add_sv(scope->string_store, key->token.value);
        ADD_LOCAL(var_name);
    }

    // compile body with the new bindings
    for (size_t i = 0; i < args.len - 1; i++) {
        TRY(kokos_expr_compile(&args.items[i + 1], scope));
    }

    POP_SCOPE();
})

KOKOS_DEFINE_SFORM(plus, {
    COMP_ARGS();
    ADD(args.len);
})

KOKOS_DEFINE_SFORM(minus, {
    COMP_ARGS();
    SUB(args.len);
})

KOKOS_DEFINE_SFORM(mul, {
    COMP_ARGS();
    MUL(args.len);
})

KOKOS_DEFINE_SFORM(div, {
    COMP_ARGS();
    DIV(args.len);
})

KOKOS_DEFINE_SFORM(if, {
    VERIFY_ARGS_COUNT(if, 3);
    TRY(kokos_expr_compile(&args.items[0], scope));

    kokos_label_t alt_label = LABEL();

    JZ(alt_label);

    TRY(kokos_expr_compile(&args.items[1], scope));

    kokos_label_t end_label = LABEL();

    BRANCH(end_label);

    LINK(alt_label);

    TRY(kokos_expr_compile(&args.items[2], scope));

    LINK(end_label);
})

KOKOS_DEFINE_SFORM(lt, {
    VERIFY_ARGS_COUNT(lt, 2);

    COMP_ARGS();

    LT();
})

KOKOS_DEFINE_SFORM(gt, {
    VERIFY_ARGS_COUNT(gt, 2);

    COMP_ARGS();

    GT();
})

KOKOS_DEFINE_SFORM(lte, {
    VERIFY_ARGS_COUNT(lte, 2);

    COMP_ARGS();

    LTE();
})

KOKOS_DEFINE_SFORM(gte, {
    VERIFY_ARGS_COUNT(gte, 2);

    COMP_ARGS();

    GTE();
})

KOKOS_DEFINE_SFORM(eq, {
    VERIFY_ARGS_COUNT(eq, 2);

    COMP_ARGS();

    CMP();
    EQ(0);
})

KOKOS_DEFINE_SFORM(neq, {
    VERIFY_ARGS_COUNT(neq, 2);

    COMP_ARGS();

    CMP();
    NEQ(0);
})
