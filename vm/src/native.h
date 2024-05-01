#ifndef NATIVE_H_
#define NATIVE_H_

#include "base.h"
#include <stdint.h>

typedef struct kokos_vm kokos_vm_t;

typedef void (*kokos_native_proc_t)(kokos_vm_t* vm);

kokos_native_proc_t kokos_find_native(string_view name);

#endif // NATIVE_H_
