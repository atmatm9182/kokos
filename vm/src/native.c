#include "native.h"
#include "base.h"
#include "src/gc.h"
#include "src/runtime.h"
#include "src/value.h"
#include "vm.h"

static void native_print(kokos_vm_t* vm, uint16_t nargs)
{
    // do this so we don't peek an empty stack
    if (nargs == 0) {
        printf("\n");
        return;
    }

    kokos_frame_t* frame = STACK_PEEK(&vm->frames);

    for (uint16_t i = 0; i < nargs; i++) {
        kokos_value_t value = STACK_POP(&frame->stack);

        kokos_value_print(value);
        if (i != nargs - 1) {
            printf(" ");
        }
    }
    printf("\n");
}

static void native_make_vec(kokos_vm_t* vm, uint16_t nargs)
{
    kokos_frame_t* frame = STACK_PEEK(&vm->frames);

    kokos_runtime_vector_t* vector = kokos_gc_alloc(&vm->gc, VECTOR_TAG);

    for (size_t i = 0; i < nargs; i++) {
        DA_ADD(vector, STACK_POP(&frame->stack));
    }

    STACK_PUSH(&frame->stack, TO_VECTOR(vector));
}

static void native_make_map(kokos_vm_t* vm, uint16_t nargs)
{
    KOKOS_VERIFY(nargs % 2 == 0);

    kokos_frame_t* frame = STACK_PEEK(&vm->frames);

    kokos_runtime_map_t* map = kokos_gc_alloc(&vm->gc, MAP_TAG);

    for (size_t i = 0; i < nargs / 2; i++) {
        kokos_value_t key = STACK_POP(&frame->stack);
        kokos_value_t value = STACK_POP(&frame->stack);
        kokos_runtime_map_add(map, key, value);
    }

    STACK_PUSH(&frame->stack, TO_MAP(map));
}

typedef struct {
    const char* name;
    kokos_native_proc_t proc;
} kokos_named_native_proc_t;

static kokos_named_native_proc_t natives[] = {
    { "print", native_print },
    { "make-vec", native_make_vec },
    { "make-map", native_make_map },
};

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
