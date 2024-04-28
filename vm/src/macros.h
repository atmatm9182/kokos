#ifndef MACROS_H_
#define MACROS_H_

#include <stdio.h>

#ifndef KOKOS_ALLOC
#include <stdlib.h>
#define KOKOS_ALLOC malloc
#endif

#define KOKOS_VERIFY(val)                                                                          \
    do {                                                                                           \
        if (!(val)) {                                                                              \
            fprintf(stderr, "VERIFICATION FAILED: %s%d\n", __FILE__, __LINE__);                    \
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

#endif // MACROS_H_
