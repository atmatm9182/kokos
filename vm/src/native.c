#include "native.h"
#include "base.h"
#include "vm.h"

static void native_print(kokos_vm_t* vm)
{
    kokos_frame_t* frame = STACK_PEEK(&vm->frames);
    kokos_value_t value = STACK_POP(&frame->stack);

    if (IS_DOUBLE(value)) {
        printf("%f\n", value.as_double);
        return;
    }

    if (IS_STRING(value)) {
        size_t idx = value.as_int & ~STRING_BITS;
        kokos_string_t string = vm->store.string_store.items[idx];
        printf("%.*s\n", (int)string.len, string.ptr);
        return;
    }

    KOKOS_TODO();
}

typedef struct {
    const char* name;
    kokos_native_proc_t proc;
} kokos_named_native_proc_t;

static kokos_named_native_proc_t natives[] = { { "print", native_print } };

#define NATIVES_COUNT (sizeof(natives) / sizeof(natives[0]))

kokos_native_proc_t kokos_find_native(string_view name)
{
    for (size_t i = 0; i < NATIVES_COUNT; i++) {
        if (sv_eq_cstr(name, natives[i].name)) {
            return natives[i].proc;
        }
    }

    return NULL;
}
