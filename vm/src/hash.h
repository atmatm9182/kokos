#ifndef HASH_H_
#define HASH_H_

#include <stdint.h>
#include <stddef.h>

uint64_t hash_djb2(const char* str);
uint64_t hash_djb2_len(const char* str, size_t len);

#endif // HASH_H_
