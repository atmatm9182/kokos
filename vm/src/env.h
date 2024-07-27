#ifndef ENV_H_
#define ENV_H_

#include "base.h"
#include "value.h"

typedef struct kokos_env {
    struct kokos_env* parent;
    hash_table values;
} kokos_env_t;

kokos_env_t kokos_env_new(kokos_env_t* parent);
void kokos_env_add(kokos_env_t* env, char const* key, kokos_value_t value);
bool kokos_env_find(kokos_env_t* env, char const* key, kokos_value_t* out);

#endif // ENV_H_
