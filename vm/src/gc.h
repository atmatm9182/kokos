#ifndef GC_H_
#define GC_H_

#include "value.h"
#include "macros.h"
#include "runtime.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

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

void kokos_gc_destroy(kokos_gc_t*);

static void kokos_gc_obj_free(kokos_gc_obj_t* obj)
{
    switch (VALUE_TAG(obj->value)) {
    case STRING_TAG: {
        kokos_runtime_string_t* str = GET_STRING(obj->value);
        KOKOS_FREE(str->ptr);
        break;
    }
    case LIST_TAG: {
        kokos_runtime_list_t* list = GET_LIST(obj->value);
        KOKOS_FREE(list->items);
        break;
    }
    case VECTOR_TAG: {
        kokos_runtime_vector_t* vec = GET_VECTOR(obj->value);
        KOKOS_FREE(vec->items);
        break;
    }
    case PROC_TAG: {
        KOKOS_TODO();
    }
    case MAP_TAG: {
        ht_destroy(&GET_MAP(obj->value)->table);
        break;
    }
    default: {
        char buf[512];
        sprintf(buf, "gc object value tag %ld", VALUE_TAG(obj->value));
        KOKOS_TODO(buf);
    }
    }

    KOKOS_FREE((void*)GET_PTR(obj->value));
}

#endif // GC_H_
