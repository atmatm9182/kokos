#ifndef INTERPRETER_H_
#define INTERPRETER_H_

#include "env.h"
#include "gc.h"
#include "obj.h"

#ifndef KOKOS_DEFAULT_GC_THRESHOLD
#define KOKOS_DEFAULT_GC_THRESHOLD 1024
#endif

struct kokos_interp {
    kokos_gc_t gc;
    kokos_env_t global_env;
    kokos_env_t* current_env;
};

typedef struct kokos_interp kokos_interp_t;

kokos_obj_t* kokos_interp_eval(kokos_interp_t* interp, kokos_obj_t* obj, bool top_level);
kokos_interp_t* kokos_interp_new(size_t gc_threshold);
void kokos_interp_destroy(kokos_interp_t* interp);
void kokos_interp_print_stat(const kokos_interp_t* interp);
const char* kokos_interp_get_error(void);

#endif // INTERPRETER_H_
