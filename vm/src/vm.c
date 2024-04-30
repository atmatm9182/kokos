#include "vm.h"
#include "compile.h"
#include "macros.h"

#define STACK_PUSH(stack, value)                                                                   \
    do {                                                                                           \
        (stack)->data[(stack)->sp++] = (value);                                                    \
    } while (0)

#define STACK_POP(stack) ((stack)->data[--((stack)->sp)])

#define TO_BOOL(b) (TO_VALUE(b ? TRUE_BITS : FALSE_BITS))

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

static void exec(kokos_vm_t* vm, kokos_compiler_context_t* ctx);

static kokos_value_t call_proc(
    kokos_vm_t* vm, const kokos_compiled_proc_t* proc, kokos_compiler_context_t* ctx)
{
    kokos_frame_t* cur_frame = current_frame(vm);
    kokos_frame_t* new_frame = alloc_frame();

    for (size_t i = 0; i < proc->params.len; i++) {
        new_frame->locals[i] = STACK_POP(&cur_frame->stack);
    }

    STACK_PUSH(&vm->frames, new_frame);

    kokos_code_t vm_instructions = vm->instructions;
    size_t vm_ip = vm->ip;

    vm->instructions = proc->body;
    vm->ip = 0;
    while (vm->ip < vm->instructions.len) {
        exec(vm, ctx);
    }

    vm->instructions = vm_instructions;
    vm->ip = vm_ip;

    kokos_value_t return_value = STACK_POP(&new_frame->stack);
    STACK_POP(&vm->frames);
    return return_value;
}

static kokos_instruction_t current_instruction(const kokos_vm_t* vm)
{
    return vm->instructions.items[vm->ip];
}

static bool kokos_value_to_bool(kokos_value_t value)
{
    return !IS_FALSE(value) && !IS_NIL(value);
}

static uint64_t kokos_cmp_values(kokos_value_t lhs, kokos_value_t rhs)
{
    if (lhs.as_int == rhs.as_int) {
        return 0;
    }

    KOKOS_VERIFY(
        IS_DOUBLE(lhs) && IS_DOUBLE(rhs)); // TODO: allow comparing of any values or throw an error
    return lhs.as_double < rhs.as_double ? -1 : 1;
}

static void exec(kokos_vm_t* vm, kokos_compiler_context_t* ctx)
{
    kokos_frame_t* frame = current_frame(vm);
    kokos_instruction_t instruction = current_instruction(vm);

    switch (instruction.type) {
    case I_PUSH: {
        STACK_PUSH(&frame->stack, TO_VALUE(instruction.operand));
        vm->ip++;
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

        vm->ip++;
        break;
    }
    case I_SUB: {
        double acc = 0.0;
        for (ssize_t i = 0; i < instruction.operand - 1; i++) {
            kokos_value_t val = STACK_POP(&frame->stack);
            KOKOS_VERIFY(IS_DOUBLE(val));
            acc -= val.as_double;
        }

        acc += STACK_POP(&frame->stack).as_double;
        STACK_PUSH(&frame->stack, TO_VALUE(acc));

        vm->ip++;
        break;
    }
    case I_MUL: {
        double acc = 1.0;
        for (size_t i = 0; i < instruction.operand; i++) {
            kokos_value_t val = STACK_POP(&frame->stack);
            KOKOS_VERIFY(IS_DOUBLE(val));
            acc *= val.as_double;
        }
        STACK_PUSH(&frame->stack, TO_VALUE(acc));

        vm->ip++;
        break;
    }
    case I_DIV: {
        if (instruction.operand == 0) {
            STACK_PUSH(&frame->stack, TO_VALUE(NAN_BITS));
            vm->ip++;
            break;
        }

        double divisor = 1.0;
        for (size_t i = 0; i < instruction.operand - 1; i++) {
            kokos_value_t val = STACK_POP(&frame->stack);
            KOKOS_VERIFY(IS_DOUBLE(val));
            divisor *= val.as_double;
        }

        kokos_value_t divident = STACK_POP(&frame->stack);
        KOKOS_VERIFY(IS_DOUBLE(divident));

        STACK_PUSH(&frame->stack, TO_VALUE(divident.as_double / divisor));

        vm->ip++;
        break;
    }
    case I_CALL: {
        const char* proc_name = (const char*)instruction.operand;
        kokos_compiled_proc_t* proc = kokos_ctx_get_proc(ctx, proc_name);
        KOKOS_VERIFY(proc);

        kokos_value_t return_value = call_proc(vm, proc, ctx);
        STACK_PUSH(&frame->stack, return_value);

        vm->ip++;
        break;
    }
    case I_PUSH_LOCAL: {
        kokos_value_t local = frame->locals[instruction.operand];
        STACK_PUSH(&frame->stack, local);

        vm->ip++;
        break;
    }
    case I_BRANCH: {
        vm->ip += (int32_t)instruction.operand;
        break;
    }
    case I_JZ: {
        kokos_value_t test = STACK_POP(&frame->stack);
        if (!kokos_value_to_bool(test)) {
            vm->ip += (int32_t)instruction.operand;
            break;
        }

        vm->ip++;
        break;
    }
    case I_JNZ: {
        kokos_value_t test = STACK_POP(&frame->stack);
        if (kokos_value_to_bool(test)) {
            vm->ip += (int32_t)instruction.operand;
            break;
        }

        vm->ip++;
        break;
    }
    case I_CMP: {
        kokos_value_t rhs = STACK_POP(&frame->stack);
        kokos_value_t lhs = STACK_POP(&frame->stack);

        uint64_t cmp = kokos_cmp_values(lhs, rhs);
        STACK_PUSH(&frame->stack, TO_VALUE(cmp));

        vm->ip++;
        break;
    }
    case I_EQ: {
        kokos_value_t top = STACK_POP(&frame->stack);
        STACK_PUSH(&frame->stack, TO_BOOL(top.as_int == instruction.operand));

        vm->ip++;
        break;
    }
    case I_NEQ: {
        kokos_value_t top = STACK_POP(&frame->stack);
        STACK_PUSH(&frame->stack, TO_BOOL(top.as_int != instruction.operand));

        vm->ip++;
        break;
    }
    default: {

        char buf[128] = { 0 };
        sprintf(buf, "execution of instruction %s is not implemented",
            kokos_instruction_type_str(instruction.type));
        KOKOS_TODO(buf);
    }
    }
}

void kokos_vm_run(kokos_vm_t* vm, kokos_code_t code, kokos_compiler_context_t* ctx)
{
    vm->instructions = code;

    kokos_frame_t* frame = alloc_frame();
    STACK_PUSH(&vm->frames, frame);

    while (vm->ip < code.len) {
        exec(vm, ctx);
    }

    // NOTE: we do not pop the frame here after the vm has run
}

void kokos_vm_dump(kokos_vm_t* vm)
{
    kokos_frame_t* frame = current_frame(vm);

    printf("stack:\n");
    for (size_t i = 0; i < frame->stack.sp; i++) {
        kokos_value_t cur = frame->stack.data[i];

        printf("\t[%lu] ", i);

        if (IS_DOUBLE(cur)) {
            printf("%f", cur.as_double);
        } else if IS_TRUE (cur) {
            printf("true");
        } else if IS_FALSE (cur) {
            printf("false");
        } else {
            KOKOS_TODO();
        }

        printf("\n");
    }
}
