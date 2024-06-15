#ifndef GC_H_
#define GC_H_

#include "value.h"
#include <stdlib.h>
#include <stdbool.h>

typedef struct kokos_gc_object_node {
    kokos_value_t value;
    struct kokos_gc_object_node* next;
    bool marked;
} kokos_gc_object_node_t;

typedef struct {
    kokos_gc_object_node_t* root;
    size_t len;
} kokos_gc_object_list_t;

typedef struct kokos_gc {
    kokos_gc_object_list_t objects;
    size_t max_objs;
} kokos_gc_t;

kokos_gc_t kokos_gc_new(size_t max_objs);
void kokos_gc_add_obj(kokos_gc_t* gc, kokos_value_t value);

#endif // GC_H_
