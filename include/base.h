#ifndef INCLUDE_BASE_H
#define INCLUDE_BASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifndef BASEDEF
#ifdef BASE_STATIC
#define BASEDEF static
#else
#define BASEDEF extern
#endif  // BASE_STATIC
#endif  // BASEDEF

#ifndef BASE_ALLOC
#define BASE_ALLOC malloc
#endif  // BASE_ALLOC

// DYNAMIC ARRAYS
#define DA_INIT(arr, l, c)                                                                       \
    do {                                                                                         \
        __typeof__(c) _cap = c ? c : 1;                                                          \
        (arr)->items = (__typeof__((arr)->items[0])*)BASE_ALLOC(sizeof((arr)->items[0]) * _cap); \
        (arr)->cap = _cap;                                                                       \
        (arr)->len = l;                                                                          \
        memset((arr)->items, 0, l * sizeof((arr)->items[0]));                                    \
    } while (0)

#define DA_INIT_ZEROED(arr, c) \
    do {                       \
        DA_INIT((arr), c, c);  \
        (arr)->len = 0;        \
    } while (0)

#define DA_RESIZE(arr, c)                                                                   \
    do {                                                                                    \
        (arr)->cap = (c);                                                                   \
        (arr)->items =                                                                      \
            (__typeof__((arr)->items))realloc((arr)->items, (c) * sizeof((arr)->items[0])); \
    } while (0)

#define DA_GROW(arr)                  \
    do {                              \
        (arr)->cap *= 2;              \
        DA_RESIZE((arr), (arr)->cap); \
    } while (0)

#define DA_ADD(arr, elem)                    \
    do {                                     \
        if ((arr)->len == (arr)->cap) {      \
            DA_GROW(arr);                    \
        }                                    \
        (arr)->items[(arr)->len++] = (elem); \
    } while (0)

#define DA_FREE(arr)        \
    do {                    \
        free((arr)->items); \
    } while (0)

#define DA_REMOVE(arr, idx)                                                                \
    do {                                                                                   \
        if ((idx) < (arr)->len) {                                                          \
            memmove((arr)->items + idx, (arr)->items + idx + 1, (arr)->len - ((idx) + 1)); \
            (arr)->len--;                                                                  \
        }                                                                                  \
    } while (0)

// HASH TABLE
#define HT_ITER(ht, body)                              \
    do {                                               \
        for (size_t i = 0; i < (ht).cap; i++) {        \
            ht_bucket* bucket = (ht).buckets[i];       \
            if (!bucket) {                             \
                continue;                              \
            }                                          \
            for (size_t j = 0; j < bucket->len; j++) { \
                ht_kv_pair kv = bucket->items[j];      \
                body                                   \
            }                                          \
        }                                              \
    } while (0)

typedef struct {
    void* key;
    void* value;
} ht_kv_pair;

typedef struct {
    ht_kv_pair* items;
    size_t len;
    size_t cap;
} ht_bucket;

typedef uint64_t (*ht_hash_func)(const void*);
typedef bool (*ht_eq_func)(const void*, const void*);

typedef struct {
    ht_bucket** buckets;
    size_t len;
    size_t cap;

    /// the hash function
    ht_hash_func hash_function;
    /// the key comparison function
    ht_eq_func equality_function;
} hash_table;

BASEDEF hash_table ht_make(ht_hash_func hash_func, ht_eq_func eq_func, size_t cap);
BASEDEF void ht_add(hash_table* ht, void* key, void* value);
BASEDEF void* ht_find(hash_table* ht, const void* key);
BASEDEF void* ht_delete(hash_table* ht, const void* key);
BASEDEF void ht_destroy(hash_table* ht);

// STRING VIEW

#define SV_FMT "%.*s"

typedef struct {
    const char* ptr;
    size_t size;
} string_view;

BASEDEF string_view sv_make(const char* str, size_t size);
BASEDEF void sv_print(string_view sv);
BASEDEF string_view sv_slice(string_view sv, size_t start, size_t len);
BASEDEF string_view sv_slice_end(string_view sv, size_t start);
BASEDEF bool sv_starts_with(string_view sv, const char* prefix);
BASEDEF bool sv_starts_with_sv(string_view sv, string_view prefix);
BASEDEF bool sv_eq(string_view a, string_view b);
BASEDEF bool sv_eq_cstr(string_view sv, const char* str);
BASEDEF int64_t sv_atoi(string_view sv);
BASEDEF double sv_atof(string_view sv);
BASEDEF const char* sv_dup(string_view sv);
BASEDEF ssize_t sv_find_sub(string_view sv, string_view substr);
BASEDEF ssize_t sv_find_sub_cstr(string_view sv, const char* substr);
BASEDEF ssize_t sv_find(string_view sv, char c);

typedef struct {
    char* items;
    size_t len;
    size_t cap;
} string_builder;

BASEDEF string_builder sb_new(size_t cap);
BASEDEF void sb_destroy(string_builder* sb);
BASEDEF void sb_push_cstr(string_builder* sb, const char* str);
BASEDEF void sb_sprintf(string_builder* sb, const char* fmt, ...)
    __attribute__((format(printf, 2, 3)));
BASEDEF char* sb_to_cstr(string_builder* sb);
BASEDEF void sb_push(string_builder* sb, char c);
BASEDEF void sb_clear(string_builder* sb);

// MISC

typedef struct {
    char* contents;
    const char* name;
    struct stat stat;
} base_file;

BASEDEF char* base_read_whole_file_to_cstr(const char* filepath);
BASEDEF base_file base_read_whole_file(const char* filepath);
BASEDEF int base_read_whole_file_buf(const char* filepath, char* buf, size_t buf_size);

#ifdef BASE_IMPLEMENTATION

#include <stdarg.h>
#include <stdio.h>

// STRING VIEW
BASEDEF string_view sv_make(const char* str, size_t size) {
    return (string_view){.ptr = str, .size = size};
}

BASEDEF void sv_print(string_view sv) {
    printf(SV_FMT, (int)sv.size, sv.ptr);
}

BASEDEF string_view sv_slice(string_view sv, size_t start, size_t len) {
    return (string_view){.ptr = sv.ptr + start, .size = len};
}

BASEDEF string_view sv_slice_end(string_view sv, size_t start) {
    return sv_slice(sv, start, sv.size - start);
}

BASEDEF bool sv_starts_with(string_view sv, const char* prefix) {
    size_t len = strlen(prefix);
    for (size_t i = 0; i < sv.size && i < len; i++) {
        if (sv.ptr[i] != prefix[i]) {
            return false;
        }
    }

    return true;
}

BASEDEF bool sv_starts_with_sv(string_view sv, string_view prefix) {
    for (size_t i = 0; i < sv.size && i < prefix.size; i++) {
        if (sv.ptr[i] != prefix.ptr[i]) {
            return false;
        }
    }

    return true;
}

BASEDEF bool sv_eq(string_view a, string_view b) {
    if (a.size != b.size) return false;
    return strncmp(a.ptr, b.ptr, a.size) == 0;
}

BASEDEF bool sv_eq_cstr(string_view sv, const char* str) {
    if (sv.size != strlen(str)) return false;
    return strncmp(sv.ptr, str, sv.size) == 0;
}

BASEDEF int64_t sv_atoi(string_view sv) {
    int64_t num = 0;
    for (size_t i = 0; i < sv.size; i++) {
        num = num * 10 + (sv.ptr[i] - '0');
    }
    return num;
}

#define DOUBLE_LIT_MAX_LEN 316

BASEDEF double sv_atof(string_view sv) {
    char tmp[DOUBLE_LIT_MAX_LEN + 1];
    memcpy(tmp, sv.ptr, sv.size);
    tmp[sv.size] = '\0';
    return atof(tmp);
}

#undef DOUBLE_LIT_MAX_LEN

BASEDEF const char* sv_dup(string_view sv) {
    char* ptr = (char*)BASE_ALLOC(sizeof(char) * (sv.size + 1));
    memcpy(ptr, sv.ptr, sizeof(char) * sv.size);
    ptr[sv.size] = '\0';
    return ptr;
}

BASEDEF ssize_t sv_find(string_view sv, char c) {
    for (size_t i = 0; i < sv.size; i++) {
        if (c == sv.ptr[i]) {
            return i;
        }
    }

    return -1;
}

BASEDEF ssize_t sv_find_sub(string_view sv, string_view substr) {
    for (size_t i = 0; i < sv.size - substr.size + 1; i++) {
        size_t saved_i = i;

        for (size_t j = 0; j < substr.size; j++) {
            if (substr.ptr[j] != sv.ptr[i++]) {
                goto loopend;
            }
        }

        return saved_i;
    loopend:
        i = saved_i;
    }

    return -1;
}

BASEDEF ssize_t sv_find_sub_cstr(string_view sv, const char* substr) {
    size_t substr_len = strlen(substr);
    ssize_t len = sv.size - substr_len + 1;

    for (ssize_t i = 0; i < len; i++) {
        size_t saved_i = i;

        for (size_t j = 0; j < substr_len; j++) {
            if (substr[j] != sv.ptr[i++]) {
                goto loopend;
            }
        }

        return saved_i;
    loopend:
        i = saved_i;
    }

    return -1;
}

// STRING BUILDER
BASEDEF string_builder sb_new(size_t cap) {
    string_builder sb;
    DA_INIT(&sb, 0, cap);
    return sb;
}

BASEDEF void sb_push(string_builder* sb, char c) {
    DA_ADD(sb, c);
}

BASEDEF void sb_push_cstr(string_builder* sb, const char* str) {
    while (*str) {
        sb_push(sb, *str++);
    }
}

BASEDEF char* sb_to_cstr(string_builder* sb) {
    DA_ADD(sb, '\0');
    return sb->items;
}

BASEDEF void sb_destroy(string_builder* sb) {
    DA_FREE(sb);
}

BASEDEF void sb_sprintf(string_builder* sb, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int count = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    while (sb->cap - sb->len < count) {
        DA_GROW(sb);
    }

    va_start(args, fmt);
    vsnprintf(sb->items + sb->len, count + 1, fmt,
              args);  // NOTE: add 1 here for the null terminator, but do not add it to the length
                      // of the string builder
    va_end(args);
    sb->len += count;
}

BASEDEF void sb_clear(string_builder* sb) {
    memset(sb->items, 0, sb->len * sizeof(sb->items[0]));
    sb->len = 0;
}

// HASH TABLE

BASEDEF int ht_load(const hash_table* ht) {
    return (ht->len / ht->cap) * 100;
}

BASEDEF hash_table ht_make(ht_hash_func hash_func, ht_eq_func eq_func, size_t cap) {
    ht_bucket** buckets = (ht_bucket**)calloc(cap, sizeof(ht_bucket*));

    return (hash_table){
        .buckets = buckets,
        .len = 0,
        .cap = cap,
        .hash_function = hash_func,
        .equality_function = eq_func,
    };
}

static inline uint64_t __ht_idx_for(const hash_table* ht, const void* key) {
    uint64_t hash = ht->hash_function(key);
    return hash % ht->cap;
}

BASEDEF void ht_add(hash_table* ht, void* key, void* value) {
    if (ht_load(ht) >= 70) {
        ht->cap *= 2;
        ht->buckets = (ht_bucket**)realloc(ht->buckets, ht->cap);
    }

    size_t idx = __ht_idx_for(ht, key);
    ht_bucket* bucket = ht->buckets[idx];
    ht_kv_pair pair = {.key = key, .value = value};

    if (!bucket) {
        bucket = (ht_bucket*)BASE_ALLOC(sizeof(ht_bucket));
        DA_INIT(bucket, 0, 2);
        DA_ADD(bucket, pair);
        ht->buckets[idx] = bucket;
        ht->len++;
        return;
    }

    for (size_t i = 0; i < bucket->len; i++) {
        if (ht->equality_function(key, bucket->items[i].key)) {
            bucket->items[i].value = value;
            return;
        }
    }

    DA_ADD(bucket, pair);
}

BASEDEF void* ht_find(hash_table* ht, const void* key) {
    size_t idx = __ht_idx_for(ht, key);
    ht_bucket* bucket = ht->buckets[idx];
    if (!bucket) return NULL;

    for (size_t i = 0; i < bucket->len; i++) {
        if (ht->equality_function(bucket->items[i].key, key)) return bucket->items[i].value;
    }

    return NULL;
}

BASEDEF void* ht_delete(hash_table* ht, const void* key) {
    size_t idx = __ht_idx_for(ht, key);
    ht_bucket* bucket = ht->buckets[idx];
    if (!bucket) {
        return NULL;
    }

    for (size_t i = 0; i < bucket->len; i++) {
        if (ht->equality_function(bucket->items[i].key, key)) {
            DA_REMOVE(bucket, i);
            ht->len--;
            return bucket->items[i].value;
        }
    }

    return NULL;
}

BASEDEF void ht_destroy(hash_table* ht) {
    for (size_t i = 0; i < ht->cap; i++) {
        if (ht->buckets[i]) {
            DA_FREE(ht->buckets[i]);
            free(ht->buckets[i]);
        }
    }

    free(ht->buckets);
}

// MISC
BASEDEF char* base_read_whole_file_to_cstr(const char* filepath) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        return NULL;
    }

    long fsize = ftell(f);
    rewind(f);

    char* buf = (char*)BASE_ALLOC(sizeof(char) * (fsize + 1));
    fread(buf, fsize, 1, f);
    buf[fsize] = '\0';

    fclose(f);
    return buf;
}

BASEDEF base_file base_read_whole_file(const char* filepath) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        return (base_file){0};
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        return (base_file){0};
    }

    long fsize = ftell(f);
    rewind(f);

    char* buf = (char*)BASE_ALLOC(sizeof(char) * (fsize + 1));
    fread(buf, fsize, 1, f);
    buf[fsize] = '\0';

    fclose(f);

    base_file file = {0};

    if (stat(filepath, &file.stat) != 0) {
        return file;
    }

    file.contents = buf;
    file.name = filepath;

    return file;
}

BASEDEF int base_read_whole_file_buf(const char* filepath, char* buf, size_t buf_size) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        return 1;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        return 1;
    }

    long fsize = ftell(f);
    if (fsize > buf_size - 1) {
        return 1;
    }

    rewind(f);

    fread(buf, fsize, 1, f);
    buf[fsize] = '\0';

    return 0;
}

#endif
#endif
