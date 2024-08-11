#ifndef NATIVE_H_
#define NATIVE_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct kokos_vm kokos_vm_t;

typedef bool (*kokos_native_proc_t)(kokos_vm_t* vm, uint16_t nargs);

typedef struct {
    char const** names;
    kokos_native_proc_t* procs;
    size_t count;
} kokos_native_proc_list_t;

kokos_native_proc_list_t kokos_natives_get(void);

#endif // NATIVE_H_
