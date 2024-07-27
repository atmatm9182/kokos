#ifndef HASH_H_
#define HASH_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

uint64_t hash_djb2(const char* str);
uint64_t hash_djb2_len(const char* str, size_t len);

uint64_t hash_string_func(void const* ptr);
bool hash_string_eq_func(void const* lhs, void const* rhs);

uint64_t hash_sizet_func(void const* ptr);
bool hash_sizet_eq_func(void const* lhs, void const* rhs);

#endif // HASH_H_
