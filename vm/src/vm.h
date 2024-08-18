#ifndef VM_H_
#define VM_H_

#include "base.h"
#include "compile.h"
#include "gc.h"
#include "instruction.h"
#include "value.h"
#include "vmconstants.h"

#define STACK_PUSH(stack, value)                                                                   \
    do {                                                                                           \
        (stack)->data[(stack)->sp++] = (value);                                                    \
    } while (0)

#define STACK_POP(stack) ((stack)->data[--((stack)->sp)])

#define STACK_PEEK(stack) ((stack)->data[(stack)->sp - 1])

#define TO_BOOL(b) (TO_VALUE(b ? TRUE_BITS : FALSE_BITS))

#define VM_CTX(vm) ((vm)->contexes.items[(vm)->ctx_idx])

// use this only for macros and exceptions below
#define DOUBLE_TAG 0

#define CHECK_DOUBLE(val)                                                                          \
    do {                                                                                           \
        if (!IS_DOUBLE((val))) {                                                                   \
            kokos_vm_ex_set_type_mismatch(vm, DOUBLE_TAG, VALUE_TAG((val)));                       \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_TYPE(val, t)                                                                         \
    do {                                                                                           \
        if (VALUE_TAG((val)) != (t)) {                                                             \
            kokos_vm_ex_set_type_mismatch(vm, (t), VALUE_TAG((val)));                              \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_ARITY(e, g)                                                                          \
    do {                                                                                           \
        if ((e) != (g)) {                                                                          \
            kokos_vm_ex_set_arity_mismatch(vm, (e), (g));                                          \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_CUSTOM(cond, fmt)                                                                    \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            kokos_vm_ex_custom_printf(vm, fmt);                                                    \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

#define CHECK_CUSTOM_PRINT(cond, fmt, ...)                                                         \
    do {                                                                                           \
        if (!(cond)) {                                                                             \
            kokos_vm_ex_custom_printf(vm, fmt, __VA_ARGS__);                                       \
            return false;                                                                          \
        }                                                                                          \
    } while (0)

typedef struct {
    hash_table* call_locations;
    kokos_string_store_t* strings;
} kokos_runtime_store_t;

typedef struct {
    kokos_value_t data[OP_STACK_SIZE];
    size_t sp;
} kokos_op_stack_t;

typedef struct {
    kokos_value_t locals[MAX_LOCALS];
    kokos_op_stack_t stack;
    kokos_token_t where;
    size_t ret_location; // set the highest bit to indicate whether to return to procedure code
} kokos_frame_t;

typedef struct {
    kokos_frame_t* data[FRAME_STACK_SIZE];
    size_t sp;
    size_t cap; // store this so we can reuse `popped` frames
} kokos_frame_stack_t;

typedef enum {
    EX_TYPE_MISMATCH,
    EX_ARITY_MISMATCH,
    EX_CUSTOM,
} kokos_exception_type_e;

typedef struct kokos_exception {
    kokos_exception_type_e type;

    union {
        // identify the types of values using the tag, use 0 for doubles,
        // since they are not tagged
        struct {
            uint16_t expected;
            uint16_t got;
        } type_mismatch;

        struct {
            size_t expected;
            size_t got;
        } arity_mismatch;

        char const* custom;
    };
} kokos_exception_t;

typedef struct {
    kokos_frame_stack_t frames;

    struct {
        kokos_exception_t exception;
    } registers;

    size_t ip;
    size_t top_level_ip;
    kokos_code_t instructions;
} kokos_vm_context_t;

typedef struct {
    kokos_vm_context_t* items;
    size_t len;
    size_t cap;
} kokos_vm_context_stack_t;

typedef struct kokos_vm {
    kokos_gc_t gc;
    kokos_runtime_store_t store;

    kokos_vm_context_stack_t contexes;
    size_t ctx_idx;
} kokos_vm_t;

kokos_vm_t* kokos_vm_create(kokos_scope_t* ctx);

/// Load the module, adding it's strings to the runtime store, and execute it's code
void kokos_vm_load_module(kokos_vm_t* vm, kokos_compiled_module_t const* module);

void kokos_vm_dump(kokos_vm_t* vm);

/// Allocates a new value of the provided tag on the heap and returns a pointer to it
void* kokos_vm_gc_alloc(kokos_vm_t* vm, uint64_t tag, size_t cap);

void kokos_vm_ex_set_type_mismatch(kokos_vm_t* vm, uint16_t expected, uint16_t got);
void kokos_vm_ex_set_arity_mismatch(kokos_vm_t* vm, size_t expected, size_t got);
void kokos_vm_ex_custom_printf(kokos_vm_t* vm, char const* fmt, ...)
    __attribute__((format(printf, 2, 3)));

void kokos_vm_run_code(kokos_vm_t* vm, kokos_code_t code);

#endif // VM_H_
