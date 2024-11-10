#ifndef MACRO_H_
#define MACRO_H_

#include "instruction.h"
#include "runtime.h"
#include "macros.h"

typedef struct {
    kokos_runtime_string_t* name;
    kokos_params_t params;
    kokos_code_t instructions;
} kokos_macro_t;

static inline void kokos_macro_destroy(kokos_macro_t* macro)
{
    KOKOS_FREE(macro->params.names);
    KOKOS_FREE(macro);
}

#endif // MACRO_H_
