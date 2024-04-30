#include "vm.h"
#include "macros.h"
#include "src/compile.h"

#define STACK_PUSH(stack, value)                                                                   \
    do {                                                                                           \
        (stack)->data[(stack)->sp++] = (value);                                                    \
    } while (0)

#define STACK_POP(stack) ((stack)->data[--((stack)->sp)])

static kokos_frame_t* alloc_frame(void)
{
    return KOKOS_ALLOC(sizeof(kokos_frame_t));
}

static kokos_frame_t* current_frame(kokos_vm_t* vm)
{
    if (vm->frames.sp == 0) {
        KOKOS_VERIFY(0);
    }

    return vm->frames.data[vm->frames.sp - 1];
}

static void exec(kokos_vm_t* vm, kokos_instruction_t instruction, kokos_compiler_context_t* ctx);

static kokos_value_t call_proc(
    kokos_vm_t* vm, const kokos_compiled_proc_t* proc, kokos_compiler_context_t* ctx)
{
    kokos_frame_t* cur_frame = current_frame(vm);
    kokos_frame_t* new_frame = alloc_frame();

    for (size_t i = 0; i < proc->params.len; i++) {
        new_frame->locals[i] = STACK_POP(&cur_frame->stack);
    }

    STACK_PUSH(&vm->frames, new_frame);

    for (size_t i = 0; i < proc->body.len; i++) {
        exec(vm, proc->body.items[i], ctx);
    }

    kokos_value_t return_value = STACK_POP(&new_frame->stack);
    STACK_POP(&vm->frames);
    return return_value;
}

static void exec(kokos_vm_t* vm, kokos_instruction_t instruction, kokos_compiler_context_t* ctx)
{
    kokos_frame_t* frame = current_frame(vm);

    switch (instruction.type) {
    case I_PUSH: {
        STACK_PUSH(&frame->stack, TO_VALUE(instruction.operand));
        break;
    }
    case I_POP: {
        STACK_POP(&frame->stack);
        break;
    }
    case I_ADD: {
        double acc = 0.0;
        for (size_t i = 0; i < instruction.operand; i++) {
            kokos_value_t val = STACK_POP(&frame->stack);
            KOKOS_VERIFY(IS_DOUBLE(val));
            acc += val.as_double;
        }
        STACK_PUSH(&frame->stack, TO_VALUE(acc));
        break;
    }
    case I_CALL: {
        const char* proc_name = (const char*)instruction.operand;
        kokos_compiled_proc_t* proc = kokos_ctx_get_proc(ctx, proc_name);
        KOKOS_VERIFY(proc);

        kokos_value_t return_value = call_proc(vm, proc, ctx);
        STACK_PUSH(&frame->stack, return_value);
        break;
    }
    case I_PUSH_LOCAL: {
        kokos_value_t local = frame->locals[instruction.operand];
        STACK_PUSH(&frame->stack, local);
        break;
    }
    }
}

void kokos_vm_run(kokos_vm_t* vm, kokos_code_t code, kokos_compiler_context_t* ctx)
{
    kokos_frame_t* frame = alloc_frame();
    STACK_PUSH(&vm->frames, frame);

    for (size_t i = 0; i < code.len; i++) {
        kokos_instruction_t instr = code.items[i];
        exec(vm, instr, ctx);
    }

    // NOTE: we do not pop the frame here after the vm has run
}

void kokos_vm_dump(kokos_vm_t* vm)
{
    kokos_frame_t* frame = current_frame(vm);

    printf("stack:\n");
    for (size_t i = 0; i < frame->stack.sp; i++) {
        printf("\t[%lu] %f\n", i, frame->stack.data[i].as_double);
    }
}
