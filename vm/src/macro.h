#ifndef MACRO_H_
#define MACRO_H_

#include "instruction.h"
#include "runtime.h"

typedef struct {
    kokos_params_t params;
    kokos_code_t instructions;
} kokos_macro_t;

#endif // MACRO_H_
