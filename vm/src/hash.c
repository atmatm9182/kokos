#include "hash.h"

uint64_t hash_djb2_len(const char* str, size_t len)
{
    int64_t hash = 5381;

    for (size_t i = 0; i < len; i++) {
        int c = str[i];
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}
uint64_t hash_djb2(const char* str)
{
    int64_t hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}
