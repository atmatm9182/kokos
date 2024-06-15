#include "gc.h"
#include "macros.h"

kokos_gc_t kokos_gc_new(size_t max_objs)
{
    return (kokos_gc_t) {
        .max_objs = max_objs,
        .objects = { 0 },
    };
}

static inline kokos_gc_object_node_t* new_node(kokos_value_t value)
{
    kokos_gc_object_node_t* node = KOKOS_ALLOC(sizeof(kokos_gc_object_node_t));
    node->marked = false;
    node->next = NULL;
    node->value = value;

    return node;
}

void kokos_gc_add_obj(kokos_gc_t *gc, kokos_value_t value)
{
    kokos_gc_object_node_t* node = new_node(value);
    if (!gc->objects.root) {
        gc->objects.root = node;
        gc->objects.len++;
        return;
    }

    node->next = gc->objects.root;
    gc->objects.root = node;
    gc->objects.len++;
}
