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

kokos_obj_t* kokos_interp_eval(kokos_interp_t* interp, kokos_obj_t* obj, kokos_env_t* env, int top_level)
{
    env = env ? env : &interp->global_env;

    kokos_obj_t* result = NULL;
    switch (obj->type) {
    case OBJ_INT:
    case OBJ_STRING:
    case OBJ_BUILTIN_FUNC:
        result = obj;
        break;
    case OBJ_SYMBOL: {
        kokos_env_pair_t* pair = kenv_find(*env, obj->symbol);
        assert(pair);
        result = pair->value;
        break;
    }
    case OBJ_LIST: {
        for (size_t i = 0; i < obj->list.len; i++) {
            obj->list.objs[i] = kokos_interp_eval(interp, obj->list.objs[i], env, 0);
        }

        kokos_obj_list_t list = { .objs = obj->list.objs + 1, .len = obj->list.len - 1 };
        kokos_builtin_func_t func = obj->list.objs[0]->builtin_func;
        result = func(interp, list);
        break;
    }
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

static kokos_obj_t* interp_builtin(kokos_interp_t* interp, kokos_builtin_func_t func)
{
    kokos_obj_t* obj = kokos_interp_alloc(interp);
    obj->type = OBJ_BUILTIN_FUNC;
    obj->builtin_func = func;
    return obj;
}

static kokos_env_t default_env(kokos_interp_t* interp)
{
    kokos_env_t env;
    DA_INIT(env, 0, 1);

    kokos_obj_t* plus = interp_builtin(interp, builtin_plus);
    DA_ADD(env, kokos_env_new("+", plus));

    kokos_obj_t* minus = interp_builtin(interp, builtin_minus);
    DA_ADD(env, kokos_env_new("-", minus));

    kokos_obj_t* star = interp_builtin(interp, builtin_star);
    DA_ADD(env, kokos_env_new("*", star));

    kokos_obj_t* slash = interp_builtin(interp, builtin_slash);
    DA_ADD(env, kokos_env_new("/", slash));
    return env;
}

kokos_interp_t kokos_interp_new(size_t gc_threshold)
{
    kokos_interp_t interp
        = (kokos_interp_t) { .gc_threshold = gc_threshold, .obj_count = 0, .obj_head = NULL };
    interp.global_env = default_env(&interp);
    return interp;
}

kokos_env_pair_t kokos_env_new(const char* name, kokos_obj_t* obj)
{
    return (kokos_env_pair_t) { .name = name, .value = obj };
}

kokos_env_pair_t* kenv_find(kokos_env_t env, const char* name)
{
    for (size_t i = 0; i < env.len; i++) {
        if (strcmp(name, env.items[i].name) == 0) {
            return &env.items[i];
        }
    }

    return NULL;
}

static inline void mark_all(kokos_env_t env)
{
    for (size_t i = 0; i < env.len; i++) {
        kokos_obj_mark(env.items[i].value);
    }
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
    mark_all(interp->global_env);
    sweep(interp);
    interp->gc_threshold = num_of_objs * 2;
}

void kokos_interp_print_stat(const kokos_interp_t* interp)
{
    printf("ALLOCATED OBJECTS: %lu\n", interp->obj_count);
    printf("GC THRESHOLD: %lu\n", interp->gc_threshold);
}
