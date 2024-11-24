#include "gc.h"
#include "macros.h"
#include <stdio.h>

kokos_gc_objs_t objs_new(size_t cap)
{
    kokos_gc_obj_t* values = KOKOS_CALLOC(cap, sizeof(kokos_gc_obj_t));
    return (kokos_gc_objs_t) { .values = values, .len = 0, .cap = cap };
}

kokos_gc_t kokos_gc_new(size_t max_objs)
{
    return (kokos_gc_t) {
        .max_objs = max_objs,
        .objects = objs_new(max_objs),
    };
}

void kokos_gc_destroy(kokos_gc_t* gc)
{
    for (size_t i = 0; i < gc->objects.cap; i++) {
        kokos_gc_obj_t obj = gc->objects.values[i];
        if (!IS_OCCUPIED(obj)) {
            continue;
        }

        kokos_gc_obj_free(&obj);
    }

    KOKOS_FREE(gc->objects.values);
}

size_t objs_load(const kokos_gc_objs_t* objs)
{
    return ((float)objs->len / (float)objs->cap) * 100;
}

size_t value_hash(kokos_value_t value)
{
    return value.as_int;
}

void objs_add(kokos_gc_objs_t* objs, kokos_gc_obj_t obj)
{
    if (objs_load(objs) >= 70) {
        kokos_gc_objs_t new_objs = objs_new(objs->cap * 2);

        for (size_t i = 0; i < objs->cap; i++) {
            if (IS_OCCUPIED(objs->values[i])) {
                objs_add(&new_objs, objs->values[i]);
            }
        }

        KOKOS_FREE(objs->values);
        *objs = new_objs;
    }

    size_t idx = value_hash(obj.value) % objs->cap;
    kokos_gc_obj_t iv = objs->values[idx];

    while (IS_OCCUPIED(iv)) {
        idx = (idx + 1) % objs->cap;
        iv = objs->values[idx];
    }

    objs->values[idx] = obj;
    objs->len++;
}

kokos_gc_obj_t* objs_find(kokos_gc_objs_t* objs, kokos_value_t value)
{
    size_t idx = value_hash(value) % objs->cap;
    kokos_gc_obj_t* iv = &objs->values[idx];
    if (!IS_OCCUPIED(*iv)) {
        return NULL;
    }

    while (iv->value.as_int != value.as_int) {
        idx = (idx + 1) % objs->cap;
        iv = &objs->values[idx];

        if (!IS_OCCUPIED(*iv)) {
            return NULL;
        }
    }

    return iv;
}

kokos_gc_obj_t* kokos_gc_find(kokos_gc_t* gc, kokos_value_t value)
{
    return objs_find(&gc->objects, value);
}

void kokos_gc_add_obj(kokos_gc_t* gc, kokos_value_t value)
{
    kokos_gc_obj_t m = { .value = value, .flags = OBJ_FLAG_OCCUPIED };
    objs_add(&gc->objects, m);
}
