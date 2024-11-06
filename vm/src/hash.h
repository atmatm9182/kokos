#ifndef HASH_H_
#define HASH_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint64_t hash_djb2(char const* str);
uint64_t hash_djb2_len(char const* str, size_t len);

uint64_t hash_cstring_func(void const* ptr);
bool hash_cstring_eq_func(void const* lhs, void const* rhs);

uint64_t hash_runtime_string_func(void const* ptr);
bool hash_runtime_string_eq_func(void const* lhs, void const* rhs);

uint64_t hash_sizet_func(void const* ptr);
bool hash_sizet_eq_func(void const* lhs, void const* rhs);

#endif // HASH_H_
