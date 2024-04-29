#ifndef VM_H_
#define VM_H_

#include "base.h"
#include "instruction.h"
#include "compile.h"
#include "value.h"

#define STACK_SIZE 8192
#define FRAME_STACK_SIZE 4096

typedef struct {
    value_t data[STACK_SIZE];
    size_t sp;
} stack_t;

typedef struct {
    value_t locals[256];
    stack_t stack;
} kokos_frame_t;

typedef struct {
    kokos_frame_t* data[FRAME_STACK_SIZE];
    size_t sp;
} frame_stack_t;

typedef struct {
    frame_stack_t frames;
} kokos_vm_t;

void kokos_vm_run(kokos_vm_t* vm, code_t code, kokos_compiler_context_t* ctx);
void kokos_vm_dump(kokos_vm_t* vm);

#endif // VM_H_
