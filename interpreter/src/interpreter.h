#ifndef INTERPRETER_H_
#define INTERPRETER_H_

#include "obj.h"
#include "base.h"

#include <stddef.h>

struct kokos_env_pair {
    const char* name;
    kokos_obj_t* value;
};

typedef struct kokos_env_pair kokos_env_pair_t;

DA_DECLARE(kokos_env_t, kokos_env_pair_t);

DA_DECLARE(obj_stack_t, kokos_obj_t*);

struct kokos_interp {
    kokos_obj_t* obj_head;
    size_t obj_count;
    size_t gc_threshold;
    kokos_env_t global_env;
};

typedef struct kokos_interp kokos_interp_t;

kokos_env_pair_t* kenv_find(kokos_env_t env, const char* name);
kokos_env_pair_t kokos_env_new(const char* name, kokos_obj_t* obj);

kokos_obj_t* kokos_interp_eval(kokos_interp_t* interp, kokos_obj_t* obj, kokos_env_t* env, int top_level);
kokos_obj_t* kokos_interp_alloc(kokos_interp_t* interp);
kokos_interp_t kokos_interp_new(size_t gc_threshold);
void kokos_gc_run(kokos_interp_t* interp);
void kokos_interp_print_stat(const kokos_interp_t* interp);

#endif // INTERPRETER_H_
