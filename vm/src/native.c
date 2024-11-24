#include "native.h"
#include "macros.h"
#include "runtime.h"
#include "value.h"
#include "vm.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

static bool native_print(kokos_vm_t* vm, uint16_t nargs, kokos_value_t* ret)
{
    // do this so we don't peek an empty stack
    if (nargs == 0) {
        printf("\n");
        return true;
    }

    kokos_frame_t* frame = STACK_PEEK(&vm->frames);

    for (uint16_t i = 0; i < nargs; i++) {
        kokos_value_t value;
        STACK_POP(&frame->stack, &value);

        kokos_value_print(value);
        if (i != nargs - 1) {
            printf(" ");
        }
    }
    printf("\n");

    return true;
}

static bool native_make_vec(kokos_vm_t* vm, uint16_t nargs, kokos_value_t* ret)
{
    kokos_frame_t* frame = STACK_PEEK(&vm->frames);

    kokos_runtime_vector_t* vector = kokos_vm_gc_alloc(vm, VECTOR_TAG, nargs);

    for (size_t i = 0; i < nargs; i++) {
        kokos_value_t elem;
        STACK_POP(&frame->stack, &elem);
        DA_ADD(vector, elem);
    }

    *ret = TO_VECTOR(vector);

    return true;
}

static bool native_make_map(kokos_vm_t* vm, uint16_t nargs, kokos_value_t* ret)
{
    CHECK_CUSTOM(nargs % 2 == 0, "expected the number of arguments to be even");

    kokos_frame_t* frame = STACK_PEEK(&vm->frames);

    kokos_runtime_map_t* map = kokos_vm_gc_alloc(vm, MAP_TAG, nargs);

    for (size_t i = 0; i < nargs / 2; i++) {
        kokos_value_t key;
        STACK_POP(&frame->stack, &key);
        kokos_value_t value;
        STACK_POP(&frame->stack, &value);
        kokos_runtime_map_add(map, key, value);
    }

    *ret = TO_MAP(map);

    return true;
}

// TODO: handle relative filepaths
static bool native_read_file(kokos_vm_t* vm, uint16_t nargs, kokos_value_t* ret)
{
    CHECK_ARITY(1, nargs);

    kokos_frame_t* frame = STACK_PEEK(&vm->frames);
    kokos_value_t filename;
    STACK_POP(&frame->stack, &filename);
    CHECK_TYPE(filename, STRING_TAG);

    kokos_runtime_string_t* filename_string = (kokos_runtime_string_t*)GET_PTR(filename);
    char fname[filename_string->len + 1];
    sprintf(fname, "%.*s", (int)filename_string->len, filename_string->ptr);

    FILE* f = fopen(fname, "rb");
    if (!f) {
        goto fail;
    }

    // FIXME: check for errors here and report them in some way
    CHECK_CUSTOM_PRINT(fseek(f, 0, SEEK_END) == 0, "could not fseek the file: %s", strerror(errno));
    size_t fsize = ftell(f);
    rewind(f); // this never fails according to the documentation

    char* buf = KOKOS_ALLOC(sizeof(char) * fsize);
    fread(buf, sizeof(char), fsize, f);

    kokos_runtime_string_t* str = kokos_vm_gc_alloc(vm, STRING_TAG, fsize);
    str->ptr = buf;
    str->len = fsize;

    *ret = TO_STRING(str);

    fclose(f);
    return true;

fail:
    if (f) {
        fclose(f);
    }

    return true;
}

// TODO: handle relative filepaths
static bool native_write_file(kokos_vm_t* vm, uint16_t nargs, kokos_value_t* ret)
{
    CHECK_ARITY(2, nargs);

    kokos_frame_t* frame = STACK_PEEK(&vm->frames);
    kokos_value_t filename;
    STACK_POP(&frame->stack, &filename);
    CHECK_TYPE(filename, STRING_TAG);

    kokos_value_t data;
    STACK_POP(&frame->stack, &data);
    CHECK_TYPE(filename, STRING_TAG);

    kokos_runtime_string_t* filename_string = (kokos_runtime_string_t*)GET_PTR(filename);
    char fname[filename_string->len + 1];
    sprintf(fname, "%.*s", (int)filename_string->len, filename_string->ptr);

    FILE* f = fopen(fname, "wb");
    if (!f) {
        goto fail;
    }

    kokos_runtime_string_t* data_str = (kokos_runtime_string_t*)GET_PTR(data);
    fwrite(data_str->ptr, sizeof(char), data_str->len, f);
    *ret = KOKOS_TRUE;

    fclose(f);
    return true;

fail:
    if (f) {
        fclose(f);
    }
    *ret = KOKOS_FALSE;
    return true;
}

typedef struct {
    const char* name;
    kokos_native_proc_t proc;
} kokos_named_native_proc_t;

static kokos_named_native_proc_t natives[] = {
    { "print", native_print },
    { "make-vec", native_make_vec },
    { "make-map", native_make_map },
    { "read-file", native_read_file },
    { "write-file", native_write_file },
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

kokos_native_proc_list_t kokos_natives_get(void)
{
    string_view* names = KOKOS_CALLOC(NATIVES_COUNT, sizeof(string_view));
    kokos_native_proc_t* procs = KOKOS_CALLOC(NATIVES_COUNT, sizeof(kokos_native_proc_t));

    for (size_t i = 0; i < NATIVES_COUNT; i++) {
        names[i] = sv_make_cstr(natives[i].name);
        procs[i] = natives[i].proc;
    }

    return (kokos_native_proc_list_t) { .procs = procs, .names = names, .count = NATIVES_COUNT };
}

void kokos_natives_free(kokos_native_proc_list_t* natives)
{
    KOKOS_FREE(natives->names);
    KOKOS_FREE(natives->procs);
}
