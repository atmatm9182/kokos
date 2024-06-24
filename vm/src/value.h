// TODO: refactor all of this
#ifndef VALUE_H_
#define VALUE_H_

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
    "kokos_value_t shoud have the size of platform's pointer"); // TODO: make this true for 32-bit
                                                                // targets

#define STRING_BITS 0x7FFE000000000000
#define MAP_BITS 0x7FFF000000000000
#define LIST_BITS 0xFFFC000000000000
#define VECTOR_BITS 0x7FFD000000000000

#define STRING_TAG (STRING_BITS >> 48)
#define MAP_TAG (MAP_BITS >> 48)
#define LIST_TAG (LIST_BITS >> 48)
#define VECTOR_TAG (VECTOR_BITS >> 48)

#define TRUE_BITS (OBJ_BITS | 1)
#define FALSE_BITS (OBJ_BITS | 2)
#define NIL_BITS (OBJ_BITS | 4)

#define IS_DOUBLE_INT(i) (((i) & OBJ_BITS) != OBJ_BITS)
#define IS_DOUBLE(val) (IS_DOUBLE_INT((val).as_int))

#define IS_STRING(val) (((val).as_int & STRING_BITS) == STRING_BITS)
#define IS_LIST(val) (((val).as_int & LIST_BITS) == LIST_BITS)
#define IS_MAP(val) (((val).as_int & MAP_BITS) == MAP_BITS)
#define IS_VECTOR(val) (((val).as_int & VECTOR_BITS) == VECTOR_BITS)

#define IS_TRUE(val) ((val).as_int == TRUE_BITS)
#define IS_FALSE(val) ((val).as_int == FALSE_BITS)
#define IS_BOOL(val) (IS_TRUE((val)) || IS_FALSE((val)))
#define IS_NIL(val) ((val).as_int == NIL_BITS)

#define TO_VALUE(i)                                                                                \
    (_Generic((i),                                                                                 \
        int: (kokos_value_t) { .as_int = (uint64_t)(i) },                                          \
        long: (kokos_value_t) { .as_int = (uint64_t)(i) },                                         \
        uint64_t: (kokos_value_t) { .as_int = (i) },                                               \
        double: (kokos_value_t) { .as_double = (i) }))

#define FROM_PTR(p) ((kokos_value_t) { .as_int = (uintptr_t)(p) })

#define TO_VECTOR(val) (TO_VALUE((uint64_t)(val) | VECTOR_BITS))
#define TO_MAP(val) (TO_VALUE((uint64_t)(val) | MAP_BITS))
#define TO_LIST(val) (TO_VALUE((uint64_t)(val) | LIST_BITS))

#define TO_PTR(val) ((void*)(val).as_int)

#define GET_TAG(i) ((i) >> 48)
#define VALUE_TAG(val) (GET_TAG((val).as_int))

#define GET_PTR_INT(i) ((i) & 0x0000FFFFFFFFFFFF)
#define GET_PTR(v) (GET_PTR_INT((v).as_int))

#define GET_STRING_INT(i) ((kokos_runtime_string_t*)GET_PTR_INT((i)))
#define GET_STRING(val) (GET_STRING_INT((val).as_int))

#define GET_VECTOR_INT(i) ((kokos_runtime_vector_t*)GET_PTR_INT((i)))
#define GET_VECTOR(val) (GET_VECTOR_INT((val).as_int))

#define GET_LIST_INT(i) ((kokos_runtime_list_t*)GET_PTR_INT((i)))
#define GET_LIST(val) (GET_LIST_INT((val).as_int))

void kokos_value_print(kokos_value_t value);

#endif // VALUE_H_
