#ifndef VM_H_
#define VM_H_

#include "base.h"
#include "compile.h"
#include "instruction.h"
#include "value.h"

#define OP_STACK_SIZE 8192
#define FRAME_STACK_SIZE 4096

typedef struct {
    hash_table functions;
    kokos_string_store_t string_store;
} kokos_runtime_store_t;

typedef struct {
    kokos_value_t data[OP_STACK_SIZE];
    size_t sp;
} kokos_op_stack_t;

typedef struct {
    kokos_value_t locals[256];
    kokos_op_stack_t stack;
} kokos_frame_t;

typedef struct {
    kokos_frame_t* data[FRAME_STACK_SIZE];
    size_t sp;
} kokos_frame_stack_t;

typedef struct {
    kokos_runtime_store_t store;
    kokos_frame_stack_t frames;
    kokos_code_t instructions;
    size_t ip;
} kokos_vm_t;

kokos_vm_t kokos_vm_create(kokos_compiler_context_t* ctx);
void kokos_vm_run(kokos_vm_t* vm, kokos_code_t code);
void kokos_vm_dump(kokos_vm_t* vm);

#endif // VM_H_
