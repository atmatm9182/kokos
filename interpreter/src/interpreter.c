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

kokos_obj_t* kokos_interp_eval(kokos_interp_t* interp, kokos_obj_t* obj, int top_level)
{
    kokos_obj_t* result = NULL;

    switch (obj->type) {
    case OBJ_INT:
    case OBJ_STRING:
    case OBJ_BUILTIN_FUNC:
        result = obj;
        break;
    case OBJ_SYMBOL: {
        printf("searching for %s\n", obj->symbol);
        kokos_env_pair_t* pair = kokos_env_find(&interp->current_env, obj->symbol);
        if (!pair) {
            pair = kokos_env_find(&interp->global_env, obj->symbol);
            assert(pair);
        }
        result = pair->value;
        break;
    }
    case OBJ_LIST: {
        kokos_obj_t* head = kokos_interp_eval(interp, obj->list.objs[0], 0);
        if (head->type == OBJ_BUILTIN_FUNC) {
            for (size_t i = 1; i < obj->list.len; i++) {
                obj->list.objs[i] = kokos_interp_eval(interp, obj->list.objs[i], 0);
            }

            kokos_obj_list_t args = { .objs = obj->list.objs + 1, .len = obj->list.len - 1 };
            kokos_builtin_func_t func = head->builtin;
            result = func(interp, args);
            break;
        }

        if (head->type == OBJ_SPECIAL_FORM) {
            kokos_obj_list_t args = { .objs = obj->list.objs + 1, .len = obj->list.len - 1 };
            kokos_builtin_func_t func = head->builtin;
            result = func(interp, args);
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

    if (top_level && interp->obj_count > interp->gc_threshold) {
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

static kokos_obj_t* sform_def(kokos_interp_t* interp, kokos_obj_list_t args)
{
    assert(args.objs[0]->type == OBJ_SYMBOL && "The variable name should be a symbol");
    for (size_t i = 1; i < args.len - 1; i++) {
        kokos_interp_eval(interp, args.objs[i], 1);
    }

    kokos_obj_t* def = kokos_interp_eval(interp, args.objs[args.len - 1], 1);
    kokos_env_add(&interp->global_env, args.objs[0]->symbol, def);
    return &kokos_obj_nil;
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

static kokos_obj_t* make_builtin(kokos_interp_t* interp, kokos_builtin_func_t func)
{
    kokos_obj_t* obj = kokos_interp_alloc(interp);
    obj->type = OBJ_BUILTIN_FUNC;
    obj->builtin = func;
    return obj;
}

static kokos_obj_t* make_special_form(kokos_interp_t* interp, kokos_builtin_func_t sform)
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
    kokos_env_add(&env, "/", star);

    kokos_obj_t* slash = make_builtin(interp, builtin_slash);
    kokos_env_add(&env, "*", slash);

    // special forms
    kokos_obj_t* def = make_special_form(interp, sform_def);
    kokos_env_add(&env, "def", def);

    return env;
}

kokos_interp_t* kokos_interp_new(size_t gc_threshold)
{
    kokos_interp_t* interp = malloc(sizeof(kokos_interp_t));
    *interp = (kokos_interp_t) { .gc_threshold = gc_threshold, .obj_count = 0, .obj_head = NULL };
    interp->global_env = default_env(interp);
    interp->current_env
        = (kokos_env_t) { .parent = &interp->global_env, .items = NULL, .len = 0, .cap = 0 };
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
    mark_all(&interp->current_env);
    sweep(interp);
    interp->gc_threshold = num_of_objs * 2;
}

void kokos_interp_print_stat(const kokos_interp_t* interp)
{
    printf("ALLOCATED OBJECTS: %lu\n", interp->obj_count);
    printf("GC THRESHOLD: %lu\n", interp->gc_threshold);
}
