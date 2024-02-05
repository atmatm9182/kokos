#include "interpreter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

kokos_obj_t* kokos_interp_alloc(kokos_interp_t* interp)
{
    kokos_obj_t* obj = malloc(sizeof(kokos_obj_t));
    obj->marked = 0;
    obj->next = interp->obj_head;
    interp->obj_head = obj;
    interp->obj_count++;
    return obj;
}

static kokos_obj_list_t list_to_args(kokos_obj_list_t list)
{
    return (kokos_obj_list_t) { .objs = list.objs + 1, .len = list.len - 1 };
}

kokos_obj_t* kokos_interp_eval(kokos_interp_t* interp, kokos_obj_t* obj)
{
    kokos_obj_t* result = NULL;

    switch (obj->type) {
    case OBJ_INT:
    case OBJ_STRING:
    case OBJ_BUILTIN_PROC:
    case OBJ_PROCEDURE:
        result = obj;
        break;
    case OBJ_SYMBOL: {
        kokos_env_pair_t* pair = kokos_env_find(interp->current_env, obj->symbol);
        assert(pair);
        result = pair->value;
        break;
    }
    case OBJ_LIST: {
        kokos_obj_t* head = kokos_interp_eval(interp, obj->list.objs[0]);
        if (head->type == OBJ_BUILTIN_PROC) {
            struct {
                kokos_obj_t** items;
                size_t len;
                size_t cap;
            } args_arr;
            DA_INIT(&args_arr, 0, obj->list.len - 1);
            for (size_t i = 1; i < obj->list.len; i++) {
                kokos_obj_t* evaluated_arg = kokos_interp_eval(interp, obj->list.objs[i]);
                DA_ADD(&args_arr, evaluated_arg);
            }

            kokos_obj_list_t args = { .objs = args_arr.items, .len = args_arr.len };

            kokos_builtin_procedure_t func = head->builtin;
            result = func(interp, args);
            break;
        }

        if (head->type == OBJ_SPECIAL_FORM) {
            kokos_obj_list_t args = list_to_args(obj->list);
            kokos_builtin_procedure_t proc = head->builtin;
            result = proc(interp, args);
            break;
        }

        if (head->type == OBJ_PROCEDURE) {
            kokos_obj_procedure_t proc = head->procedure;

            kokos_obj_list_t args = list_to_args(obj->list);
            assert(proc.params.len == args.len);

            kokos_env_t call_env = kokos_env_empty(args.len);
            for (size_t i = 0; i < args.len; i++) {
                kokos_obj_t* obj = kokos_interp_eval(interp, args.objs[i]);
                kokos_env_add(&call_env, proc.params.objs[i]->symbol, obj);
            }

            kokos_env_t* call_parent = interp->current_env;
            call_env.parent = call_parent;
            interp->current_env = &call_env;

            for (size_t i = 0; i < proc.body.len - 1; i++) {
                kokos_interp_eval(interp, proc.body.objs[i]);
            }

            result = kokos_interp_eval(interp, proc.body.objs[proc.body.len - 1]);
            interp->current_env = call_parent;
            break;
        }

        assert(0
            && "This should result in an error saying that we cannot call a symbol that is not "
               "callable");
        break;
    }
    case OBJ_SPECIAL_FORM:
        assert(0 && "This should result in an error since evaluating a special form is illegal");
        break;
    }

    if (!interp->current_env->parent && interp->obj_count > interp->gc_threshold) {
        kokos_obj_mark(result);
        kokos_gc_run(interp);
    }

    return result;
}

static kokos_obj_t* builtin_plus(kokos_interp_t* interp, kokos_obj_list_t args)
{
    int64_t num = 0;
    for (size_t i = 0; i < args.len; i++) {
        num += args.objs[i]->integer;
    }
    kokos_obj_t* obj = kokos_interp_alloc(interp);
    obj->type = OBJ_INT;
    obj->integer = num;
    return obj;
}

static kokos_obj_t* builtin_minus(kokos_interp_t* interp, kokos_obj_list_t args)
{
    int64_t num = args.objs[0]->integer;
    kokos_obj_t* obj = kokos_interp_alloc(interp);
    obj->type = OBJ_INT;

    if (args.len == 1) {
        obj->integer = -num;
        return obj;
    }

    for (size_t i = 1; i < args.len; i++) {
        num -= args.objs[i]->integer;
    }
    obj->integer = num;
    return obj;
}

static kokos_obj_t* builtin_star(kokos_interp_t* interp, kokos_obj_list_t args)
{
    int64_t num = args.objs[0]->integer;
    for (size_t i = 1; i < args.len; i++) {
        num *= args.objs[i]->integer;
    }
    kokos_obj_t* obj = kokos_interp_alloc(interp);
    obj->type = OBJ_INT;
    obj->integer = num;
    return obj;
}

static kokos_obj_t* builtin_slash(kokos_interp_t* interp, kokos_obj_list_t args)
{
    int64_t num = args.objs[0]->integer;
    for (size_t i = 1; i < args.len; i++) {
        num /= args.objs[i]->integer;
    }
    kokos_obj_t* obj = kokos_interp_alloc(interp);
    obj->type = OBJ_INT;
    obj->integer = num;
    return obj;
}

static kokos_obj_t* sform_def(kokos_interp_t* interp, kokos_obj_list_t args)
{
    assert(args.objs[0]->type == OBJ_SYMBOL && "The variable name should be a symbol");
    for (size_t i = 1; i < args.len - 1; i++) {
        kokos_interp_eval(interp, args.objs[i]);
    }

    kokos_obj_t* def = kokos_interp_eval(interp, args.objs[args.len - 1]);
    kokos_env_add(&interp->global_env, args.objs[0]->symbol, def);
    return &kokos_obj_nil;
}

static kokos_obj_t* sform_proc(kokos_interp_t* interp, kokos_obj_list_t args)
{
    assert(args.objs[0]->type == OBJ_SYMBOL && "The procedure name should be a symbol");

    kokos_obj_t* params = args.objs[1];
    assert(params->type == OBJ_LIST && "The params should be a list of symbols");

    kokos_obj_list_t body = { .objs = args.objs + 2, .len = args.len - 2 };
    kokos_obj_procedure_t proc = { .params = params->list, .body = body };

    kokos_obj_t* result = kokos_interp_alloc(interp);
    result->type = OBJ_PROCEDURE;
    result->procedure = proc;

    kokos_env_add(interp->current_env, args.objs[0]->symbol, result);
    return result;
}

static kokos_obj_t* make_builtin(kokos_interp_t* interp, kokos_builtin_procedure_t func)
{
    kokos_obj_t* obj = kokos_interp_alloc(interp);
    obj->type = OBJ_BUILTIN_PROC;
    obj->builtin = func;
    return obj;
}

static kokos_obj_t* make_special_form(kokos_interp_t* interp, kokos_builtin_procedure_t sform)
{
    kokos_obj_t* obj = kokos_interp_alloc(interp);
    obj->type = OBJ_SPECIAL_FORM;
    obj->builtin = sform;
    return obj;
}

static kokos_env_t default_env(kokos_interp_t* interp)
{
    kokos_env_t env = kokos_env_empty(10);

    // builtins
    kokos_obj_t* plus = make_builtin(interp, builtin_plus);
    kokos_env_add(&env, "+", plus);

    kokos_obj_t* minus = make_builtin(interp, builtin_minus);
    kokos_env_add(&env, "-", minus);

    kokos_obj_t* star = make_builtin(interp, builtin_star);
    kokos_env_add(&env, "*", star);

    kokos_obj_t* slash = make_builtin(interp, builtin_slash);
    kokos_env_add(&env, "/", slash);

    // special forms
    kokos_obj_t* def = make_special_form(interp, sform_def);
    kokos_env_add(&env, "def", def);

    kokos_obj_t* proc = make_special_form(interp, sform_proc);
    kokos_env_add(&env, "proc", proc);

    return env;
}

kokos_interp_t* kokos_interp_new(size_t gc_threshold)
{
    kokos_interp_t* interp = malloc(sizeof(kokos_interp_t));
    *interp = (kokos_interp_t) { .gc_threshold = gc_threshold, .obj_count = 0, .obj_head = NULL };
    interp->global_env = default_env(interp);
    interp->current_env = &interp->global_env;
    return interp;
}

static inline void mark_all(kokos_env_t* env)
{
    if (env == NULL)
        return;

    for (size_t i = 0; i < env->len; i++) {
        kokos_obj_mark(env->items[i].value);
    }

    mark_all(env->parent);
}

static inline void sweep(kokos_interp_t* interp)
{
    kokos_obj_t** cur = &interp->obj_head;
    while (*cur) {
        if (!(*cur)->marked) {
            kokos_obj_t* obj = *cur;
            *cur = (*cur)->next;
            free(obj);
            interp->obj_count--;
        } else {
            (*cur)->marked = 0;
            cur = &(*cur)->next;
        }
    }
}

void kokos_gc_run(kokos_interp_t* interp)
{
    size_t num_of_objs = interp->obj_count;
    mark_all(interp->current_env);
    sweep(interp);
    interp->gc_threshold = num_of_objs * 2;
}

void kokos_interp_print_stat(const kokos_interp_t* interp)
{
    printf("ALLOCATED OBJECTS: %lu\n", interp->obj_count);
    printf("GC THRESHOLD: %lu\n", interp->gc_threshold);
}
