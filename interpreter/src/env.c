#include "env.h"

kokos_env_pair_t kokos_env_make_pair(const char* name, kokos_obj_t* obj)
{
    return (kokos_env_pair_t) { .name = strdup(name), .value = obj };
}

kokos_env_pair_t* kokos_env_find(kokos_env_t* env, const char* name)
{
    if (env == NULL)
        return NULL;

    for (size_t i = 0; i < env->len; i++) {
        if (strcmp(name, env->items[i].name) == 0) {
            return &env->items[i];
        }
    }

    return kokos_env_find(env->parent, name);
}

void kokos_env_add(kokos_env_t* env, const char* name, kokos_obj_t* obj)
{
    kokos_env_pair_t* found = kokos_env_find(env, name);

    if (found != NULL) {
        found->value = obj;
        return;
    }

    kokos_env_pair_t pair = kokos_env_make_pair(name, obj);
    DA_ADD(env, pair);
}

kokos_env_t kokos_env_empty(size_t cap)
{
    kokos_env_t env;
    DA_INIT(&env, 0, cap);
    env.parent = NULL;
    return env;
}

void kokos_env_destroy(kokos_env_t* env)
{
    for (size_t i = 0; i < env->len; i++) {
        free(env->items[i].name);
    }
    DA_FREE(env);
}
