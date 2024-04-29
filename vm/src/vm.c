#include "vm.h"
#include "macros.h"

static void stack_push(stack_t* stack, value_t value)
{
    stack->data[stack->sp++] = value;
}

static value_t stack_pop(stack_t* stack)
{
    return stack->data[--stack->sp];
}

static void exec(vm_t* vm, instruction_t instruction)
{
    switch (instruction.type) {
    case I_PUSH: stack_push(&vm->stack, TO_VALUE(instruction.operand)); break;
    case I_POP:  stack_pop(&vm->stack); break;
    case I_ADD:  {
        double acc = 0.0;
        for (size_t i = 0; i < instruction.operand; i++) {
            value_t val = stack_pop(&vm->stack);
            KOKOS_VERIFY(IS_DOUBLE(val));
            acc += val.as_double;
        }
        stack_push(&vm->stack, TO_VALUE(acc));
    }
    default: KOKOS_TODO();
    }
}

void kokos_vm_run(vm_t* vm, code_t code)
{
    for (size_t i = 0; i < code.len; i++) {
        instruction_t instr = code.items[i];
        exec(vm, instr);
    }
}

void kokos_vm_dump(const vm_t* vm)
{
    printf("stack:\n");
    for (size_t i = 0; i < vm->stack.sp; i++) {
        printf("\t[%lu] %f\n", i, vm->stack.data[i].as_double);
    }
}
