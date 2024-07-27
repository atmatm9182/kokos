#include "env.h"
#include "base.h"
#include "hash.h"

#define KOKOS_ENV_DEFAULT_CAP 11

kokos_env_t kokos_env_new(kokos_env_t* parent)
{
    hash_table values = ht_make(hash_string_func, hash_string_eq_func, KOKOS_ENV_DEFAULT_CAP);
    return (kokos_env_t) {
        .parent = parent,
        .values = values,
    };
}

void kokos_env_add(kokos_env_t* env, char const* name, kokos_value_t value)
{
    ht_add(&env->values, (void*)name, (void*)value.as_int);
}

bool kokos_env_find(kokos_env_t* env, char const* name, kokos_value_t* out)
{
    if (!env) {
        return false;
    }

    void* value = ht_find(&env->values, name);
    if (value) {
        *out = TO_VALUE((uint64_t)value);
        return true;
    }

    return kokos_env_find(env->parent, name, out);
}
