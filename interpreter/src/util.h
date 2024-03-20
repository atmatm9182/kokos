#ifndef UTIL_H_
#define UTIL_H_

#include <stdio.h>
#include <stdlib.h>

#define KOKOS_FAIL_WITH(msg)                                                                       \
    do {                                                                                           \
        fprintf(stderr, "%s%d: ", __FILE__, __LINE__);                                             \
        fprintf(stderr, msg);                                                                      \
        exit(1);                                                                                   \
    } while (0)

char* read_whole_file(const char* filepath);

#endif // UTIL_H_
