#include "vm.h"
#include "base.h"
#include "compile.h"
#include "macros.h"
#include "native.h"
#include "runtime.h"

static void exec(kokos_vm_t* vm);

kokos_compiled_proc_t* kokos_store_get_proc(kokos_runtime_store_t* store, const char* name)
{
    return ht_find(&store->procedures, name);
}

static kokos_frame_t* alloc_frame(size_t ret_location)
{
    kokos_frame_t* frame = KOKOS_ALLOC(sizeof(kokos_frame_t));
    frame->ret_location = ret_location;
    return frame;
}

static kokos_frame_t* current_frame(kokos_vm_t* vm)
{
    if (vm->frames.sp == 0) {
        KOKOS_VERIFY(0);
    }

    return STACK_PEEK(&vm->frames);
}

static kokos_instruction_t current_instruction(const kokos_vm_t* vm)
{
    return vm->current_instructions->items[vm->ip];
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

kokos_value_t kokos_alloc_value(kokos_vm_t* vm, uint64_t params)
{
    kokos_frame_t* frame = STACK_PEEK(&vm->frames);

    switch (GET_TAG(params)) {
    case VECTOR_TAG: {
        kokos_runtime_vector_t* vec = (kokos_runtime_vector_t*)kokos_gc_alloc(&vm->gc, VECTOR_TAG);

        uint32_t count = params & 0xFFFFFFFF;
        for (size_t i = 0; i < count; i++) {
            DA_ADD(vec, STACK_POP(&frame->stack));
        }

        return TO_VALUE((uint64_t)vec | VECTOR_BITS);
    }
    case MAP_TAG: {
        kokos_runtime_map_t* map = (kokos_runtime_map_t*)kokos_gc_alloc(&vm->gc, MAP_TAG);

        uint32_t count = params & 0xFFFFFFFF;
        for (size_t i = 0; i < count; i++) {
            kokos_value_t value = STACK_POP(&frame->stack);
            kokos_value_t key = STACK_POP(&frame->stack);
            kokos_runtime_map_add(map, key, value);
        }

        return TO_VALUE((uint64_t)map | MAP_BITS);
    }
    default: {
        char buf[128] = { 0 };
        printf("0x%lx\n", VECTOR_TAG);
        sprintf(buf, "allocation of type with tag 0x%lx not implemented", GET_TAG(params));
        KOKOS_TODO(buf);
    }
    }
}

static void exec(kokos_vm_t* vm)
{
    kokos_frame_t* frame = STACK_PEEK(&vm->frames);
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
        size_t ret_location = (vm->ip + 1)
            | ((size_t)(vm->frames.sp != 1) << 63); // FIXME: this should use uint64_t values

        uint32_t arity = instruction.operand >> 32;
        uint32_t ip = instruction.operand & 0xFFFFFFFF;
        vm->ip = ip;
        vm->current_instructions = &vm->store.procedure_code;

        kokos_frame_t* new_frame = alloc_frame(ret_location);
        for (size_t i = 0; i < arity; i++) {
            new_frame->locals[i] = STACK_POP(&frame->stack);
        }
        STACK_PUSH(&vm->frames, new_frame);
        break;
    }
    case I_RET: {
        kokos_frame_t* proc_frame = STACK_POP(&vm->frames);
        kokos_value_t ret_value = STACK_PEEK(&proc_frame->stack);

        const size_t ret_mask = (size_t)1 << 63;
        vm->ip = proc_frame->ret_location & ~ret_mask;

        if (ret_mask & proc_frame->ret_location) {
            vm->current_instructions = &vm->store.procedure_code;
        } else {
            vm->current_instructions = &vm->instructions;
        }

        STACK_PUSH(&current_frame(vm)->stack, ret_value);
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
    case I_CALL_NATIVE: {
        kokos_native_proc_t native = (kokos_native_proc_t)instruction.operand;
        native(vm);
        vm->ip++;
        break;
    }
    case I_ALLOC: {
        kokos_value_t value = kokos_alloc_value(vm, instruction.operand);
        STACK_PUSH(&frame->stack, value);
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

void kokos_vm_run(kokos_vm_t* vm, kokos_code_t code)
{
    vm->instructions = code;
    vm->current_instructions = &vm->instructions;

    kokos_frame_t* frame = alloc_frame(0);
    STACK_PUSH(&vm->frames, frame);

    while (vm->ip < vm->current_instructions->len) {
        exec(vm);
    }

    // NOTE: we do not pop the frame here after the vm has ran
}

void kokos_vm_dump(kokos_vm_t* vm)
{
    kokos_frame_t* frame = current_frame(vm);

    printf("stack:\n");
    for (size_t i = 0; i < frame->stack.sp; i++) {
        printf("\t[%lu] ", i);

        kokos_value_t cur = frame->stack.data[i];
        kokos_value_print(cur);

        printf("\n");
    }
}

kokos_vm_t kokos_vm_create(kokos_compiler_context_t* ctx)
{
    kokos_vm_t vm = { 0 };
    vm.store = (kokos_runtime_store_t) { .procedures = ctx->procedures,
        .string_store = ctx->string_store,
        .procedure_code = ctx->procedure_code };
    vm.frames.sp = 0;

    kokos_gc_object_list_t gc_objs;
    DA_INIT(&gc_objs, 0, 100);
    vm.gc = (kokos_gc_t) { .objects = gc_objs, .max_objs = 200 };

    return vm;
}
