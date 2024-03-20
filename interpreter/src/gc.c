#include "gc.h"
#include "src/map.h"

kokos_obj_t* kokos_gc_alloc(kokos_gc_t* gc)
{
    kokos_obj_t* obj = malloc(sizeof(kokos_obj_t));
    obj->marked = 0;
    obj->quoted = 0;
    obj->next = gc->root;
    gc->root = obj;
    gc->obj_count++;
    return obj;
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

static inline void sweep(kokos_gc_t* gc)
{
    kokos_obj_t** cur = &gc->root;
    while (*cur) {
        if (!(*cur)->marked) {
            kokos_obj_t* obj = *cur;
            *cur = (*cur)->next;
            kokos_obj_free(obj);
            gc->obj_count--;
        } else {
            (*cur)->marked = 0;
            cur = &(*cur)->next;
        }
    }
}

void kokos_gc_run(kokos_gc_t* gc, kokos_env_t* env)
{
    mark_all(env);
    sweep(gc);
}
