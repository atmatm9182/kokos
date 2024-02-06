#ifndef ENV_H_
#define ENV_H_

#include "base.h"
#include "obj.h"

struct kokos_env_pair {
    char* name;
    kokos_obj_t* value;
};

typedef struct kokos_env_pair kokos_env_pair_t;

struct kokos_env {
    struct kokos_env* parent;
    kokos_env_pair_t* items;
    size_t len;
    size_t cap;
};

typedef struct kokos_env kokos_env_t;

kokos_env_t kokos_env_empty(size_t cap);
void kokos_env_add(kokos_env_t* env, const char* name, kokos_obj_t* obj);
kokos_env_pair_t* kokos_env_find(kokos_env_t* env, const char* name);
kokos_env_pair_t kokos_env_make_pair(const char* name, kokos_obj_t* obj);
void kokos_env_destroy(kokos_env_t* env);

#endif // ENV_H_
