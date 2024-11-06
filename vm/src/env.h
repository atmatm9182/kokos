#ifndef KOKOS_ENV_H_
#define KOKOS_ENV_H

#include "base.h"
#include "runtime.h"

typedef struct kokos_env {
    hash_table vars;
    struct kokos_env* parent;
} kokos_env_t;

kokos_env_t* kokos_env_create(kokos_env_t* parent, size_t cap);
bool kokos_env_lookup(kokos_env_t* env, const kokos_runtime_string_t* name, kokos_value_t* out);
void kokos_env_add(kokos_env_t* env, const kokos_runtime_string_t* name, kokos_value_t value);
void kokos_env_destroy(kokos_env_t* env);

#endif // KOKOS_ENV_H
