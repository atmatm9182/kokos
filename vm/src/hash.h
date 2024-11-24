#ifndef HASH_H_
#define HASH_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint64_t hash_djb2(const char* str);
uint64_t hash_djb2_len(const char* str, size_t len);

uint64_t hash_cstring_func(const void* ptr);
bool hash_cstring_eq_func(const void* lhs, const void* rhs);

uint64_t hash_runtime_string_func(const void* ptr);
bool hash_runtime_string_eq_func(const void* lhs, const void* rhs);

uint64_t hash_sizet_func(const void* ptr);
bool hash_sizet_eq_func(const void* lhs, const void* rhs);

#endif // HASH_H_
