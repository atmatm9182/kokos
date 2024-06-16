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

typedef struct {
    hash_table procedures;
    kokos_code_t procedure_code;
    kokos_string_store_t string_store;
} kokos_runtime_store_t;

typedef struct {
    kokos_value_t data[OP_STACK_SIZE];
    size_t sp;
} kokos_op_stack_t;

typedef struct {
    kokos_value_t locals[MAX_LOCALS];
    kokos_op_stack_t stack;
    size_t ret_location; // set the highest bit to indicate whether to return to procedure code
} kokos_frame_t;

typedef struct {
    kokos_frame_t* data[FRAME_STACK_SIZE];
    size_t sp;
} kokos_frame_stack_t;

typedef struct kokos_vm {
    kokos_gc_t gc;
    kokos_runtime_store_t store;
    kokos_frame_stack_t frames;

    kokos_code_t instructions;
    kokos_code_t* current_instructions;
    size_t ip;
} kokos_vm_t;

kokos_vm_t kokos_vm_create(kokos_compiler_context_t* ctx);
void kokos_vm_run(kokos_vm_t* vm, kokos_code_t code);
void kokos_vm_dump(kokos_vm_t* vm);

/// Allocates a new value of the provided tag on the heap and returns a pointer to it
void* kokos_vm_gc_alloc(kokos_vm_t* vm, uint64_t tag);

#endif // VM_H_
