#ifndef LOCATION_H_
#define LOCATION_H_

#include <stddef.h>

struct kokos_location {
    const char* filename;
    size_t row;
    size_t col;
};

typedef struct kokos_location kokos_location_t;

#endif // LOCATION_H_
