#ifndef GC_H_
#define GC_H_

#include "value.h"
#include <stdlib.h>

typedef struct {
    kokos_value_t* items;
    size_t len;
    size_t cap;
} kokos_gc_object_list_t;

typedef struct kokos_gc {
    kokos_gc_object_list_t objects;
    size_t max_objs;
} kokos_gc_t;

/// Allocates a new value of the provided tag on the heap and returns a pointer to it
void* kokos_gc_alloc(kokos_gc_t* gc, uint64_t tag);

#endif // GC_H_
