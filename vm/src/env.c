#include "env.h"
#include "macros.h"
#include "hash.h"

kokos_env_t* kokos_env_create(kokos_env_t* parent, size_t cap)
{
    kokos_env_t* env = KOKOS_ZALLOC(sizeof(*parent));
    env->vars = ht_make(hash_runtime_string_func, hash_runtime_string_eq_func, cap || 1);
    env->parent = parent;
    return env;
}

bool kokos_env_lookup(kokos_env_t* env, const kokos_runtime_string_t* name, kokos_value_t* out)
{
    if (!env) {
        return false;
    }

    void* value = ht_find(&env->vars, name);
    if (!value) {
        return kokos_env_lookup(env->parent, name, out);
    }

    *out = FROM_PTR(value);
    return true;
}

void kokos_env_add(kokos_env_t* env, const kokos_runtime_string_t* name, kokos_value_t value)
{
    ht_add(&env->vars, (kokos_runtime_string_t*)name, TO_PTR(value));
}

void kokos_env_destroy(kokos_env_t* env)
{
    ht_destroy(&env->vars);
    KOKOS_FREE(env);
}
