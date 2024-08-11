#ifndef MACROS_H_
#define MACROS_H_

#include <stdio.h>

#ifndef KOKOS_ALLOC
#include <stdlib.h>
#define KOKOS_ALLOC malloc
#endif // KOKOS_ALLOC

#ifndef KOKOS_FREE
#define KOKOS_FREE free
#endif // KOKOS_FREE

#ifndef KOKOS_REALLOC
#define KOKOS_REALLOC realloc
#endif

#ifndef KOKOS_CALLOC
#define KOKOS_CALLOC calloc
#endif

#define __ESC_RED "\e[31m"
#define __ESC_RESET "\e[39;49m"

#ifdef KOKOS_DEBUG_BUILD
#define KOKOS_ASSERT(val)                                                                          \
    do {                                                                                           \
        if (!(val)) {                                                                              \
            fprintf(                                                                               \
                stderr, __ESC_RED "ASSERTION FAILED:" __ESC_RESET " %s:%d\n", __FILE__, __LINE__); \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)
#else
#define KOKOS_ASSERT(...)
#endif // KOKOS_DEBUG_BUILD

#define KOKOS_VERIFY(val)                                                                          \
    do {                                                                                           \
        if (!(val)) {                                                                              \
            fprintf(stderr, __ESC_RED "VERIFICATION FAILED:" __ESC_RESET " %s:%d\n", __FILE__,     \
                __LINE__);                                                                         \
            exit(1);                                                                               \
        }                                                                                          \
    } while (0)

#define KOKOS_TODO(...)                                                                            \
    do {                                                                                           \
        const char* __todo_args[] = { __VA_ARGS__ };                                               \
        const char* __todo_msg = sizeof(__todo_args) ? __todo_args[0] : "not implemented";         \
        fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, __todo_msg);                            \
        exit(1);                                                                                   \
    } while (0)

#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define TRY(e)                                                                                     \
    do {                                                                                           \
        if (UNLIKELY(!(e)))                                                                        \
            return false;                                                                          \
    } while (0)

#endif // MACROS_H_
