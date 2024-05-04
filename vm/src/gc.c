#include "gc.h"
#include "base.h"
#include "macros.h"
#include "runtime.h"

static void kokos_gc_collect(kokos_gc_t* gc)
{
    KOKOS_TODO();
}

void* kokos_gc_alloc(kokos_gc_t* gc, uint64_t tag)
{
    if (gc->objects.len > gc->max_objs) {
        kokos_gc_collect(gc);
    }

    void* addr;
    switch (tag) {
    case VECTOR_TAG: {
        kokos_runtime_vector_t* vec = KOKOS_ALLOC(sizeof(kokos_runtime_vector_t));
        DA_INIT(vec, 0, 3);
        addr = vec;
        break;
    }
    case MAP_TAG: {
        hash_table table = ht_make(kokos_default_map_hash_func, kokos_default_map_eq_func, 10);
        kokos_runtime_map_t* map = KOKOS_ALLOC(sizeof(kokos_runtime_map_t));
        map->table = table;
        addr = map;
        break;
    }
    default: KOKOS_TODO();
    }

    DA_ADD(&gc->objects, TO_VALUE((uint64_t)addr | tag));
    return addr;
}
