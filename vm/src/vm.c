#include "vm.h"
#include "base.h"
#include "compile.h"
#include "gc.h"
#include "macros.h"
#include "native.h"
#include "runtime.h"
#include "value.h"
#include <stdarg.h>
#include <stdio.h>

#define INSTR_OP_ARG_MASK 0xFFFFFFFF

static bool exec(kokos_vm_t* vm);

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

// TODO: refactor those to just use regular substraction like on x86_64
static int cmp_ints(uint32_t lhs, uint32_t rhs)
{
    if (lhs == rhs) {
        return 0;
    }

    return lhs < rhs ? -1 : 1;
}

static bool kokos_cmp_values(kokos_vm_t* vm, kokos_value_t lhs, kokos_value_t rhs)
{
    kokos_frame_t* frame = current_frame(vm);

    if (lhs.as_int == rhs.as_int) {
        STACK_PUSH(&frame->stack, TO_VALUE(0));
        return true;
    }

    uint16_t ltag = VALUE_TAG(lhs);
    uint16_t rtag = VALUE_TAG(rhs);

    // is there a better way to do this?
    if (ltag != rtag) {
        STACK_PUSH(&frame->stack, TO_VALUE(-1));
        return true;
    }

    if (ltag == INT_TAG) {
        int v = cmp_ints(GET_INT(lhs), GET_INT(rhs));
        STACK_PUSH(&frame->stack, TO_VALUE(v));
        return true;
    }

    CHECK_DOUBLE(lhs);
    CHECK_DOUBLE(rhs);

    uint64_t res = lhs.as_double < rhs.as_double ? -1 : 1;
    STACK_PUSH(&current_frame(vm)->stack, TO_VALUE(res));

    return true;
}

kokos_value_t kokos_alloc_value(kokos_vm_t* vm, uint64_t params)
{
    kokos_frame_t* frame = STACK_PEEK(&vm->frames);

    switch (GET_TAG(params)) {
    case VECTOR_TAG: {
        uint32_t count = params & INSTR_OP_ARG_MASK;
        kokos_runtime_vector_t* vec
            = (kokos_runtime_vector_t*)kokos_vm_gc_alloc(vm, VECTOR_TAG, count);

        for (size_t i = 0; i < count; i++) {
            DA_ADD(vec, STACK_POP(&frame->stack));
        }

        return TO_VECTOR(vec);
    }
    case MAP_TAG: {
        uint32_t count = params & INSTR_OP_ARG_MASK;
        kokos_runtime_map_t* map = (kokos_runtime_map_t*)kokos_vm_gc_alloc(vm, MAP_TAG, count);

        for (size_t i = 0; i < count; i++) {
            kokos_value_t value = STACK_POP(&frame->stack);
            kokos_value_t key = STACK_POP(&frame->stack);
            kokos_runtime_map_add(map, key, value);
        }

        return TO_MAP(map);
    }
    case LIST_TAG: {
        uint32_t count = params & INSTR_OP_ARG_MASK;
        kokos_runtime_list_t* list = (kokos_runtime_list_t*)kokos_vm_gc_alloc(vm, LIST_TAG, count);

        for (size_t i = 0; i < count; i++) {
            list->items[i] = STACK_POP(&frame->stack);
        }

        return TO_LIST(list);
    }
    default: {
        char buf[128] = { 0 };
        printf("0x%lx\n", VECTOR_TAG);
        sprintf(buf, "allocation of type with tag 0x%lx not implemented", GET_TAG(params));
        KOKOS_TODO(buf);
    }
    }
}

static kokos_frame_t* push_frame(kokos_vm_t* vm, size_t ret_location)
{
    if (vm->frames.sp >= vm->frames.cap) {
        kokos_frame_t* new_frame = alloc_frame(ret_location);
        STACK_PUSH(&vm->frames, new_frame);
        vm->frames.cap++;

        return new_frame;
    }

    // reuse the old frame if we have enough capacity
    kokos_frame_t* old_frame = vm->frames.data[vm->frames.sp];
    old_frame->ret_location = ret_location;
    old_frame->stack.sp = 0;

    STACK_PUSH(&vm->frames, old_frame);
    return old_frame;
}

static kokos_value_t nan = TO_VALUE(NAN_BITS);

// this looks sooo ugly
static inline bool vm_exec_add(kokos_vm_t* vm, uint64_t count)
{
    kokos_frame_t* frame = current_frame(vm);

    uint32_t acc = 0;
    double dacc = nan.as_double;

    for (size_t i = 0; i < count; i++) {
        kokos_value_t val = STACK_POP(&frame->stack);

        if (VALUE_TAG(val) == INT_TAG) {
            uint32_t i = GET_INT(STACK_POP(&frame->stack));
            if (dacc == nan.as_double) {
                acc += i;
                break;
            }

            dacc += (double)i;
            continue;
        }

        CHECK_DOUBLE(val);

        if (dacc == nan.as_double) {
            dacc = (double)acc + val.as_double;
            continue;
        }

        dacc += val.as_double;
    }

    if (dacc == nan.as_double) {
        STACK_PUSH(&frame->stack, TO_VALUE(TO_INT(acc)));
    } else {
        STACK_PUSH(&frame->stack, TO_VALUE(dacc));
    }

    return true;
}

static inline bool vm_exec_sub(kokos_vm_t* vm, uint64_t count)
{
    kokos_frame_t* frame = current_frame(vm);

    uint32_t acc = 0;
    double dacc = nan.as_double;

    for (size_t i = 0; i < count - 1; i++) {
        kokos_value_t val = STACK_POP(&frame->stack);

        if (VALUE_TAG(val) == INT_TAG) {
            uint32_t i = GET_INT(STACK_POP(&frame->stack));
            if (dacc == nan.as_double) {
                acc -= i;
                break;
            }

            dacc += (double)i;
            continue;
        }

        CHECK_DOUBLE(val);

        if (dacc == nan.as_double) {
            dacc = (double)acc - val.as_double;
            continue;
        }

        dacc += val.as_double;
    }

    kokos_value_t val = STACK_POP(&frame->stack);
    if (VALUE_TAG(val) == INT_TAG) {
        if (dacc == nan.as_double) {
            acc += GET_INT(val);
            STACK_PUSH(&frame->stack, TO_VALUE(TO_INT(acc)));
            goto success;
        }

        dacc += (double)GET_INT(val);
        STACK_PUSH(&frame->stack, TO_VALUE(dacc));
        goto success;
    }

    CHECK_DOUBLE(val);

    if (dacc == nan.as_double) {
        dacc = (double)acc + val.as_double;
        STACK_PUSH(&frame->stack, TO_VALUE(dacc));
        goto success;
    }

    STACK_PUSH(&frame->stack, TO_VALUE(dacc + val.as_double));

success:
    return true;
}

static inline bool vm_exec_mul(kokos_vm_t* vm, uint64_t count)
{
    kokos_frame_t* frame = current_frame(vm);

    uint32_t acc = 1;
    double dacc = nan.as_double;

    for (size_t i = 0; i < count; i++) {
        kokos_value_t val = STACK_POP(&frame->stack);

        if (VALUE_TAG(val) == INT_TAG) {
            uint32_t i = GET_INT(STACK_POP(&frame->stack));
            if (dacc == nan.as_double) {
                acc *= i;
                break;
            }

            dacc *= (double)i;
            continue;
        }

        CHECK_DOUBLE(val);

        if (dacc == nan.as_double) {
            dacc = (double)acc * val.as_double;
            continue;
        }

        dacc *= val.as_double;
    }

    if (dacc == nan.as_double) {
        STACK_PUSH(&frame->stack, TO_VALUE(TO_INT(acc)));
    } else {
        STACK_PUSH(&frame->stack, TO_VALUE(dacc));
    }

    return true;
}

static inline bool vm_exec_div(kokos_vm_t* vm, uint64_t count)
{
    kokos_frame_t* frame = current_frame(vm);

    if (count == 0) {
        STACK_PUSH(&frame->stack, TO_VALUE(NAN_BITS));
        return true;
    }

    uint32_t divisor = 1;
    double d_divisor = nan.as_double;

    for (size_t i = 0; i < count - 1; i++) {
        kokos_value_t val = STACK_POP(&frame->stack);

        if (VALUE_TAG(val) == INT_TAG) {
            uint32_t i = GET_INT(STACK_POP(&frame->stack));
            if (d_divisor == nan.as_double) {
                divisor *= i;
                break;
            }

            d_divisor *= (double)i;
            continue;
        }

        CHECK_DOUBLE(val);

        if (d_divisor == nan.as_double) {
            d_divisor = (double)divisor * val.as_double;
            continue;
        }

        d_divisor *= val.as_double;
    }

    kokos_value_t divident = STACK_POP(&frame->stack);
    if (VALUE_TAG(divident) == INT_TAG) {
        if (d_divisor == nan.as_double) {
            uint32_t res = TO_INT(GET_INT(divident) / divisor);
            STACK_PUSH(&frame->stack, TO_VALUE(res));
            goto success;
        }

        double res = (double)GET_INT(divident) / d_divisor;
        STACK_PUSH(&frame->stack, TO_VALUE(res));
        goto success;
    }

    CHECK_DOUBLE(divident);

    if (d_divisor == nan.as_double) {
        double res = divident.as_double / (double)divisor;
        STACK_PUSH(&frame->stack, TO_VALUE(res));
        goto success;
    }

    STACK_PUSH(&frame->stack, TO_VALUE(divident.as_double / d_divisor));

success:
    return true;
}

static bool exec(kokos_vm_t* vm)
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
        TRY(vm_exec_add(vm, instruction.operand));
        vm->ip++;
        break;
    }
    case I_SUB: {
        TRY(vm_exec_sub(vm, instruction.operand));
        vm->ip++;
        break;
    }
    case I_MUL: {
        TRY(vm_exec_mul(vm, instruction.operand));
        vm->ip++;
        break;
    }
    case I_DIV: {
        TRY(vm_exec_div(vm, instruction.operand));
        vm->ip++;
        break;
    }
    case I_CALL: {
        size_t ret_location = (vm->ip + 1) | ((size_t)(vm->frames.sp != 1) << 63);

        uint32_t arity = instruction.operand >> 32;
        uint32_t ip = instruction.operand & 0xFFFFFFFF;
        vm->ip = ip;
        vm->current_instructions = &vm->store.procedure_code;

        kokos_frame_t* new_frame = push_frame(vm, ret_location);

        for (size_t i = 0; i < arity; i++) {
            new_frame->locals[i] = STACK_POP(&frame->stack);
        }

        break;
    }
    case I_RET: {
        kokos_frame_t* proc_frame = STACK_POP(&vm->frames);
        kokos_value_t ret_value
            = proc_frame->stack.sp == 0 ? TO_VALUE(NIL_BITS) : STACK_PEEK(&proc_frame->stack);

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
    case I_STORE_LOCAL: {
        kokos_value_t v = STACK_POP(&frame->stack);
        frame->locals[instruction.operand] = v;

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

        TRY(kokos_cmp_values(vm, lhs, rhs));

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
        uint16_t nargs = instruction.operand >> 48;
        kokos_native_proc_t native
            = (kokos_native_proc_t)(instruction.operand & ~((uint64_t)0xFFFF << 48));
        TRY(native(vm, nargs));
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

    return true;
}

static char const* tag_str(uint16_t tag)
{
    switch (tag) {
    case STRING_TAG: return "string";
    case MAP_TAG:    return "map";
    case VECTOR_TAG: return "vector";
    case LIST_TAG:   return "list";
    case DOUBLE_TAG: return "double";
    case INT_TAG:    return "int";
    }

    KOKOS_VERIFY(false);
}

static void report_exception(kokos_vm_t* vm)
{
    switch (vm->exception_reg.type) {
    case EX_TYPE_MISMATCH: {
        char const* expected = tag_str(vm->exception_reg.type_mismatch.expected);
        char const* got = tag_str(vm->exception_reg.type_mismatch.got);
        fprintf(stderr, "type mismatch: expected %s, but got %s\n", expected, got);
        break;
    }
    case EX_ARITY_MISMATCH: {
        fprintf(stderr, "arity mismatch: expected %lu arguments, but got %lu\n",
            vm->exception_reg.arity_mismatch.expected, vm->exception_reg.arity_mismatch.got);
        break;
    }
    case EX_CUSTOM: {
        fprintf(stderr, "error: %s\n", vm->exception_reg.custom);
        break;
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
        if (!exec(vm)) {
            report_exception(vm);
            exit(1);
        }
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

    vm.gc = kokos_gc_new(GC_INITIAL_CAP);
    return vm;
}

static void mark_obj(kokos_gc_t* gc, kokos_gc_obj_t* obj)
{
    obj->flags |= OBJ_FLAG_MARKED;

    switch (GET_TAG(obj->value.as_int)) {
    case VECTOR_TAG: {
        kokos_runtime_vector_t* vec = (kokos_runtime_vector_t*)(GET_PTR(obj->value));
        for (size_t i = 0; i < vec->len; i++) {
            kokos_gc_obj_t* gobj = kokos_gc_find(gc, vec->items[i]);

            if (!gobj) {
                continue;
            }

            mark_obj(gc, gobj);
        }

        break;
    }
    case MAP_TAG: {
        kokos_runtime_map_t* map = (kokos_runtime_map_t*)(GET_PTR(obj->value));
        HT_ITER(map->table, {
            kokos_gc_obj_t* key = kokos_gc_find(gc, TO_VALUE((uint64_t)kv.key));
            kokos_gc_obj_t* value = kokos_gc_find(gc, TO_VALUE((uint64_t)kv.value));

            if (key) {
                mark_obj(gc, key);
            }

            if (value) {
                mark_obj(gc, value);
            }
        });

        break;
    }
    default: {
        char buf[128];
        sprintf(buf, "tag: %lx, value: %lx", GET_TAG(obj->value.as_int), obj->value.as_int);
        KOKOS_TODO(buf);
    }
    }
}

static void gc_mark_frame(kokos_gc_t* gc, kokos_frame_t const* frame)
{
    // NOTE: we don't mark locals. should we?
    for (size_t l = 0; l < frame->stack.sp; l++) {
        kokos_value_t value = frame->stack.data[l];
        kokos_gc_obj_t* obj = kokos_gc_find(gc, value);

        if (obj) {
            KOKOS_ASSERT(IS_OCCUPIED(*obj));
            mark_obj(gc, obj);
        }
    }
}

static void obj_free(kokos_gc_obj_t* obj)
{
    switch (GET_TAG(obj->value.as_int)) {
    case STRING_TAG: {
        kokos_runtime_string_t* str = (void*)GET_PTR(obj->value);
        KOKOS_FREE(str->ptr);
        break;
    }
    default: {
    }
    }

    KOKOS_FREE((void*)GET_PTR(obj->value));
}

static void kokos_gc_collect(kokos_vm_t* vm)
{
    for (size_t i = 0; i < vm->frames.sp; i++) {
        kokos_frame_t const* frame = vm->frames.data[i];
        gc_mark_frame(&vm->gc, frame);
    }

    for (size_t i = 0; i < vm->gc.objects.cap; i++) {
        kokos_gc_obj_t* obj = &vm->gc.objects.values[i];
        if (!IS_OCCUPIED(*obj) || IS_MARKED(*obj)) {
            continue;
        }

        obj->flags = 0;

        obj_free(obj);

        vm->gc.objects.len--;
    }
}

void* kokos_vm_gc_alloc(kokos_vm_t* vm, uint64_t tag, size_t cap)
{
    kokos_gc_t* gc = &vm->gc;

    if (gc->objects.len >= gc->max_objs) {
        kokos_gc_collect(vm);
    }

    void* addr;
    switch (tag) {
    case VECTOR_TAG: {
        kokos_runtime_vector_t* vec = KOKOS_ALLOC(sizeof(kokos_runtime_vector_t));
        DA_INIT(vec, 0, cap);
        addr = vec;
        break;
    }
    case MAP_TAG: {
        hash_table table = ht_make(kokos_default_map_hash_func, kokos_default_map_eq_func, cap);
        kokos_runtime_map_t* map = KOKOS_ALLOC(sizeof(kokos_runtime_map_t));
        map->table = table;
        addr = map;
        break;
    }
    case STRING_TAG: {
        kokos_runtime_string_t* str = KOKOS_ALLOC(sizeof(kokos_runtime_string_t));
        str->ptr = NULL;
        str->len = 0;
        addr = str;
        break;
    }
    case LIST_TAG: {
        kokos_runtime_list_t* list = KOKOS_ALLOC(sizeof(kokos_runtime_list_t));
        list->len = cap;
        list->items = KOKOS_CALLOC(cap, sizeof(list->items[0]));
        addr = list;
        break;
    }
    default: KOKOS_TODO();
    }

    kokos_gc_add_obj(gc, TO_VALUE((uint64_t)addr | (tag << 48)));
    return addr;
}

void kokos_vm_ex_set_type_mismatch(kokos_vm_t* vm, uint16_t expected, uint16_t got)
{
    vm->exception_reg = (kokos_exception_t) {
        .type = EX_TYPE_MISMATCH,
    };
    vm->exception_reg.type_mismatch.expected = expected;
    vm->exception_reg.type_mismatch.got = got;
}

void kokos_vm_ex_set_arity_mismatch(kokos_vm_t* vm, size_t expected, size_t got)
{
    vm->exception_reg = (kokos_exception_t) {
        .type = EX_ARITY_MISMATCH,
    };
    vm->exception_reg.arity_mismatch.expected = expected;
    vm->exception_reg.arity_mismatch.got = got;
}

void kokos_vm_ex_custom_printf(kokos_vm_t* vm, const char* fmt, ...)
{
    vm->exception_reg = (kokos_exception_t) {
        .type = EX_CUSTOM,
    };

    string_builder sb = sb_new(256);

    va_list args;
    va_start(args, fmt);
    sb_sprintf(&sb, fmt, args);
    va_end(args);

    vm->exception_reg.custom = sb_to_cstr(&sb);
}
