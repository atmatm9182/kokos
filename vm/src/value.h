#ifndef VALUE_H_
#define VALUE_H_

#include <stdint.h>

// here we use the technique called 'NaN boxing'
// we can abuse the representation of NaN as defined by IEEE754
// the top 2 bits of NaN are always set to one, and at least one
// bit in the mantissa set to one, giving us a bitmask 0x7FF8

#define OBJ_BITS 0x7FFC000000000000

typedef union {
    double as_double;
    uint64_t as_int;
} value_t;

#define STRING_BITS 0x7FFE000000000000
#define MAP_BITS 0x7FFF000000000000
#define LIST_BITS 0x7FFF800000000000
#define VECTOR_BITS 0x7FFD000000000000

#define TRUE_BITS (OBJ_BITS | 1)
#define FALSE_BITS (OBJ_BITS | 2)
#define NIL_BITS (OBJ_BITS | 4)

#define IS_DOUBLE(val) (((val).as_int & OBJ_BITS) != OBJ_BITS)
#define IS_STRING(val) (((val).as_int & STRING_BITS) == STRING_BITS)
#define IS_LIST(val) (((val).as_int & LIST_BITS) == LIST_BITS)
#define IS_MAP(val) (((val).as_int & MAP_BITS) == MAP_BITS)
#define IS_VECTOR(val) (((val).as_int & VECTOR_BITS) == VECTOR_BITS)

#define IS_TRUE(val) ((val) == TRUE_BITS)
#define IS_FALSE(val) ((val) == FALSE_BITS)
#define IS_NIL(val) ((val) == NIL_BITS)

#define TO_VALUE(i)                                                                                \
    (_Generic((i), uint64_t: (value_t) { .as_int = (i) }, double: (value_t) { .as_double = (i) }))

#endif // VALUE_H_
