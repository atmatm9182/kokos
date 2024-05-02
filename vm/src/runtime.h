#ifndef RUNTIME_H_
#define RUNTIME_H_

#include "base.h"
#include "value.h"
#include <stddef.h>

typedef struct {
    kokos_value_t* items;
    size_t len;
    size_t cap;
} kokos_runtime_vector_t;

typedef struct {
    hash_table table;
} kokos_runtime_map_t;

extern ht_hash_func kokos_default_map_hash_func;
extern ht_eq_func kokos_default_map_eq_func;

void kokos_runtime_map_add(kokos_runtime_map_t* map, kokos_value_t key, kokos_value_t value);
kokos_value_t kokos_runtime_map_find(kokos_runtime_map_t* map, kokos_value_t key);

#endif // RUNTIME_H_
