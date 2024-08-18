#ifndef VALUE_H_
#define VALUE_H_

#include <stdbool.h>
#include <stdint.h>

// here we use the technique called 'NaN boxing'
// we can abuse the representation of NaN as defined by IEEE754
// the top 2 bits of NaN are always set to one, and at least one
// bit in the mantissa set to one, giving us a bitmask 0x7FF8

#define NAN_BITS 0x7FF8000000000000
#define OBJ_BITS 0x7FFC000000000000

typedef union {
    double as_double;
    uint64_t as_int;
} kokos_value_t;

_Static_assert(sizeof(kokos_value_t) == sizeof(uintptr_t),
    "kokos_value_t shoud have the size of platform's pointer");

#define ENUMERATE_HEAP_TYPES                                                                       \
    X(STRING)                                                                                      \
    X(VECTOR)                                                                                      \
    X(LIST)                                                                                        \
    X(MAP)

#define ENUMERATE_TAGGED_TYPES                                                                     \
    ENUMERATE_HEAP_TYPES                                                                           \
    X(INT)                                                                                         \
    X(PROC)

#define STRING_BITS 0x7FFE000000000000
#define MAP_BITS 0x7FFF000000000000
#define LIST_BITS 0xFFFC000000000000
#define VECTOR_BITS 0x7FFD000000000000
#define INT_BITS 0xFFFE000000000000
#define PROC_BITS 0xFFFF000000000000

#define STRING_TAG (STRING_BITS >> 48)
#define MAP_TAG (MAP_BITS >> 48)
#define LIST_TAG (LIST_BITS >> 48)
#define VECTOR_TAG (VECTOR_BITS >> 48)
#define INT_TAG (INT_BITS >> 48)
#define PROC_TAG (PROC_BITS >> 48)

#define TRUE_BITS (OBJ_BITS | 1)
#define FALSE_BITS (OBJ_BITS | 2)
#define NIL_BITS (OBJ_BITS | 4)

#define IS_DOUBLE_INT(i) (((i) & OBJ_BITS) != OBJ_BITS)
#define IS_DOUBLE(val) (IS_DOUBLE_INT((val).as_int))

#define IS_TRUE(val) ((val).as_int == TRUE_BITS)
#define IS_FALSE(val) ((val).as_int == FALSE_BITS)
#define IS_BOOL(val) (IS_TRUE((val)) || IS_FALSE((val)))
#define IS_NIL(val) ((val).as_int == NIL_BITS)

#define TO_VALUE(i)                                                                                \
    (_Generic((i),                                                                                 \
        int: (kokos_value_t) { .as_int = (uint64_t)(i) },                                          \
        uint32_t: (kokos_value_t) { .as_int = (uint64_t)(i) },                                     \
        long: (kokos_value_t) { .as_int = (uint64_t)(i) },                                         \
        uint64_t: (kokos_value_t) { .as_int = (i) },                                               \
        double: (kokos_value_t) { .as_double = (i) }))

#define FROM_PTR(p) ((kokos_value_t) { .as_int = (uintptr_t)(p) })

#define X(t)                                                                                       \
    static inline kokos_value_t TO_##t(void* ptr)                                                  \
    {                                                                                              \
        return TO_VALUE((uint64_t)ptr | t##_BITS);                                                 \
    }

ENUMERATE_HEAP_TYPES
#undef X

#define IS_NAN_DOUBLE(d) (TO_VALUE((d)).as_int == NAN_BITS)

#define TO_PTR(val) ((void*)(val).as_int)

#define TO_INT(i) ((uint64_t)(i) | INT_BITS)
#define GET_INT(val) ((int32_t)((val).as_int & ~INT_BITS))

#define GET_TAG(i) ((i) >> 48)
#define VALUE_TAG(val) (GET_TAG((val).as_int))

#define GET_PTR_INT(i) ((i) & 0x0000FFFFFFFFFFFF)
#define GET_PTR(v) ((void*)GET_PTR_INT((v).as_int))

void kokos_value_print(kokos_value_t value);

#endif // VALUE_H_
