#ifndef GC_H_
#define GC_H_

#include "value.h"
#include <stdbool.h>
#include <stdlib.h>

typedef struct marked_value {
    kokos_value_t value;
    uint8_t flags; // 0x02 - marked
                   // 0x01 - occupied
} kokos_gc_obj_t;

#define OBJ_FLAG_MARKED 0x02
#define OBJ_FLAG_OCCUPIED 0x01

#define IS_MARKED(v) ((v).flags & OBJ_FLAG_MARKED)
#define IS_OCCUPIED(v) ((v).flags & OBJ_FLAG_OCCUPIED)

typedef struct objs {
    kokos_gc_obj_t* values;
    size_t cap;
    size_t len;
} kokos_gc_objs_t;

typedef struct kokos_gc {
    kokos_gc_objs_t objects;
    size_t max_objs;
} kokos_gc_t;

kokos_gc_t kokos_gc_new(size_t max_objs);
void kokos_gc_add_obj(kokos_gc_t* gc, kokos_value_t value);
kokos_gc_obj_t* kokos_gc_find(kokos_gc_t* gc, kokos_value_t value);

#endif // GC_H_
