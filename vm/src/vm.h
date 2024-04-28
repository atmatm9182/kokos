#ifndef VM_H_
#define VM_H_

#include "base.h"
#include "instruction.h"
#include "value.h"

#define STACK_SIZE 4096

typedef struct {
    value_t data[STACK_SIZE];
    size_t sp;
} stack_t;

typedef struct {
    stack_t stack;
} vm_t;

void kokos_vm_run(vm_t* vm, code_t code);
void kokos_vm_dump(const vm_t* vm);

#endif // VM_H_
