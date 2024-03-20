#ifndef KOKOS_GC_H
#define KOKOS_GC_H

#include "env.h"
#include "obj.h"

struct kokos_gc {
    kokos_obj_t* root;
    size_t obj_threshold;
    size_t obj_count;
};

typedef struct kokos_gc kokos_gc_t;

void kokos_gc_run(kokos_gc_t* gc, kokos_env_t* env);
kokos_obj_t* kokos_gc_alloc(kokos_gc_t* gc);

#endif // KOKOS_GC_H
