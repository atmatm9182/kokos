#include "hash.h"
#include "runtime.h"

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

uint64_t hash_cstring_func(void const* ptr)
{
    return hash_djb2((char const*)ptr);
}

bool hash_cstring_eq_func(void const* lhs, void const* rhs)
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

uint64_t hash_runtime_string_func(void const* ptr)
{
    kokos_runtime_string_t const* string = ptr;
    return hash_djb2_len(string->ptr, string->len);
}

bool hash_runtime_string_eq_func(void const* lhs, void const* rhs)
{
    kokos_runtime_string_t const* l = lhs;
    kokos_runtime_string_t const* r = rhs;

    if (l->len != r->len) {
        return false;
    }

    return strncmp(l->ptr, r->ptr, l->len) == 0;
}
