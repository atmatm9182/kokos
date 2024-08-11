#ifndef RUNTIME_H_
#define RUNTIME_H_

#include "base.h"
#include "native.h"
#include "value.h"
#include <stddef.h>

typedef struct {
    kokos_value_t* items;
    size_t len;
} kokos_runtime_list_t;

typedef struct {
    char* ptr;
    size_t len;
} kokos_runtime_string_t;

typedef struct {
    kokos_value_t* items;
    size_t len;
    size_t cap;
} kokos_runtime_vector_t;

typedef struct {
    hash_table table;
} kokos_runtime_map_t;

typedef struct {
    string_view* names;
    size_t len;
    bool variadic;
} kokos_params_t;

typedef enum {
    PROC_KOKOS,
    PROC_NATIVE,
} kokos_runtime_proc_type_e;

typedef size_t kokos_label_t;

typedef struct {
    kokos_label_t label;
    size_t code_len;
    kokos_params_t params;
} kokos_proc_t;

typedef struct {
    kokos_runtime_proc_type_e type;

    union {
        kokos_proc_t kokos;
        kokos_native_proc_t native;
    };
} kokos_runtime_proc_t;

typedef kokos_runtime_map_t kokos_runtime_MAP_t;
typedef kokos_runtime_string_t kokos_runtime_STRING_t;
typedef kokos_runtime_vector_t kokos_runtime_VECTOR_t;
typedef kokos_runtime_list_t kokos_runtime_LIST_t;

#define X(t)                                                                                       \
    static inline kokos_runtime_##t##_t* GET_##t##_INT(uintptr_t ptr)                              \
    {                                                                                              \
        return (kokos_runtime_##t##_t*)GET_PTR_INT(ptr);                                           \
    }                                                                                              \
                                                                                                   \
    static inline kokos_runtime_##t##_t* GET_##t(kokos_value_t val)                                \
    {                                                                                              \
        return GET_##t##_INT(val.as_int);                                                          \
    }
ENUMERATE_HEAP_TYPES
#undef X

#define GET_STRING_INT(i) ((kokos_runtime_string_t*)GET_PTR_INT((i)))
#define GET_STRING(val) (GET_STRING_INT((val).as_int))

#define GET_VECTOR_INT(i) ((kokos_runtime_vector_t*)GET_PTR_INT((i)))
#define GET_VECTOR(val) (GET_VECTOR_INT((val).as_int))

#define GET_LIST_INT(i) ((kokos_runtime_list_t*)GET_PTR_INT((i)))
#define GET_LIST(val) (GET_LIST_INT((val).as_int))

extern ht_hash_func kokos_default_map_hash_func;
extern ht_eq_func kokos_default_map_eq_func;

void kokos_runtime_map_add(kokos_runtime_map_t* map, kokos_value_t key, kokos_value_t value);
kokos_value_t kokos_runtime_map_find(kokos_runtime_map_t* map, kokos_value_t key);

kokos_runtime_string_t* kokos_runtime_string_new(char const* data, size_t len);

#endif // RUNTIME_H_
