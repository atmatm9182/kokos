#ifndef INTERPRETER_H_
#define INTERPRETER_H_

#include "env.h"
#include "obj.h"

struct kokos_interp {
    kokos_obj_t* obj_head;
    size_t obj_count;
    size_t gc_threshold;
    kokos_env_t global_env;
    kokos_env_t current_env;
};

typedef struct kokos_interp kokos_interp_t;

kokos_obj_t* kokos_interp_eval(kokos_interp_t* interp, kokos_obj_t* obj, int top_level);
kokos_obj_t* kokos_interp_alloc(kokos_interp_t* interp);
kokos_interp_t* kokos_interp_new(size_t gc_threshold);
void kokos_gc_run(kokos_interp_t* interp);
void kokos_interp_print_stat(const kokos_interp_t* interp);

#endif // INTERPRETER_H_
