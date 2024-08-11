#include "hash.h"

#include <string.h>

uint64_t hash_djb2_len(char const* str, size_t len)
{
    int64_t hash = 5381;

    for (size_t i = 0; i < len; i++) {
        int c = str[i];
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}

uint64_t hash_djb2(char const* str)
{
    int64_t hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

uint64_t hash_string_func(void const* ptr)
{
    return hash_djb2((char const*)ptr);
}

bool hash_string_eq_func(void const* lhs, void const* rhs)
{
    char const* ls = (char const*)lhs;
    char const* rs = (char const*)rhs;

    return strcmp(ls, rs) == 0;
}

uint64_t hash_sizet_func(void const* ptr)
{
    return (size_t)ptr;
}

bool hash_sizet_eq_func(void const* lhs, void const* rhs)
{
    return (size_t)lhs == (size_t)rhs;
}
