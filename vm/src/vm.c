#include "vm.h"
#include "base.h"
#include "compile.h"
#include "gc.h"
#include "hash.h"
#include "instruction.h"
#include "macros.h"
#include "native.h"
#include "runtime.h"
#include "string-store.h"
#include "value.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool kokos_vm_exec_cur(kokos_vm_t* vm);

static kokos_frame_t* alloc_frame(
                                  size_t ret_location,
                                  size_t locals_count,
                                  kokos_code_t instructions,
                                  kokos_env_t* parent_env
                                  )
{
    kokos_frame_t* frame = KOKOS_ZALLOC(sizeof(kokos_frame_t));
    frame->ret_location = ret_location;
    frame->env = kokos_env_create(parent_env, locals_count);
    frame->instructions = instructions;
    return frame;
}

static kokos_frame_t* current_frame(kokos_vm_t* vm)
{
    if (vm->frames.sp == 0) {
        KOKOS_VERIFY(0);
    }

    return STACK_PEEK(&vm->frames);
}

static kokos_instruction_t current_instruction(kokos_vm_t const* vm)
{
    return STACK_PEEK(&vm->frames)->instructions.items[vm->ip];
}

static bool kokos_value_to_bool(kokos_value_t value)
{
    return !IS_FALSE(value) && !IS_NIL(value);
}

// TODO: refactor those to just use regular substraction like on x86_64
static int cmp_ints(int32_t lhs, int32_t rhs)
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
    kokos_frame_t* frame = current_frame(vm);

    switch (GET_TAG(params)) {
    case VECTOR_TAG: {
        uint32_t count = params & INSTR_ALLOC_ARG_MASK;
        kokos_runtime_vector_t* vec
            = (kokos_runtime_vector_t*)kokos_vm_gc_alloc(vm, VECTOR_TAG, count);

        for (size_t i = 0; i < count; i++) {
            kokos_value_t value;
            STACK_POP(&frame->stack, &value);
            DA_ADD(vec, value);
        }

        return TO_VECTOR(vec);
    }
    case MAP_TAG: {
        uint32_t count = params & INSTR_ALLOC_ARG_MASK;
        kokos_runtime_map_t* map = (kokos_runtime_map_t*)kokos_vm_gc_alloc(vm, MAP_TAG, count);

        for (size_t i = 0; i < count; i++) {
            kokos_value_t value, key;
            STACK_POP(&frame->stack, &value);
            STACK_POP(&frame->stack, &key);
            kokos_runtime_map_add(map, key, value);
        }

        return TO_MAP(map);
    }
    case LIST_TAG: {
        uint32_t count = params & INSTR_ALLOC_ARG_MASK;
        kokos_runtime_list_t* list = (kokos_runtime_list_t*)kokos_vm_gc_alloc(vm, LIST_TAG, count);

        for (size_t i = 0; i < count; i++) {
            kokos_value_t item;
            STACK_POP(&frame->stack, &item);
            list->items[i] = item;
        }

        return TO_LIST(list);
    }
    default: {
        char buf[128] = { 0 };
        sprintf(buf, "allocation of type with tag 0x%lx not implemented", GET_TAG(params));
        KOKOS_TODO(buf);
    }
    }
}

static kokos_frame_t* kokos_make_frame(
                                       kokos_vm_t* vm, kokos_runtime_proc_t const* proc, size_t ret_location, kokos_token_t where, kokos_env_t* parent_env)
{
    // if it native, we fucked up
    KOKOS_ASSERT(proc->type == PROC_KOKOS);

    size_t locals_count = kokos_runtime_proc_locals_count(proc) || 1;

    if (vm->frames.sp >= vm->frames.cap) {
        kokos_frame_t* new_frame = alloc_frame(ret_location, locals_count, proc->kokos.code, parent_env);

        new_frame->where = where;
        STACK_PUSH(&vm->frames, new_frame);
        vm->frames.cap++;

        return new_frame;
    }

    // reuse the old frame if we have enough capacity
    kokos_frame_t* old_frame = vm->frames.data[vm->frames.sp];
    old_frame->stack.sp = 0;
    old_frame->where = where;
    old_frame->ret_location = ret_location;

    kokos_env_destroy(old_frame->env);
    old_frame->env = kokos_env_create(parent_env, locals_count);
    old_frame->instructions = proc->kokos.code;

    STACK_PUSH(&vm->frames, old_frame);

    return old_frame;
}

static kokos_value_t nan = TO_VALUE(NAN_BITS);

// this looks sooo ugly
static inline bool vm_exec_add(kokos_vm_t* vm, uint64_t count)
{
    kokos_frame_t* frame = current_frame(vm);

    int32_t acc = 0;
    double dacc = nan.as_double;

    for (size_t i = 0; i < count; i++) {
        kokos_value_t val;
        STACK_POP(&frame->stack, &val);

        if (VALUE_TAG(val) == INT_TAG) {
            int32_t iv = GET_INT(val);
            if (IS_NAN_DOUBLE(dacc)) {
                acc += iv;
                continue;
            }

            dacc += (double)iv;
            continue;
        }

        CHECK_DOUBLE(val);

        if (IS_NAN_DOUBLE(dacc)) {
            dacc = (double)acc + val.as_double;
            continue;
        }

        dacc += val.as_double;
    }

    if (IS_NAN_DOUBLE(dacc)) {
        STACK_PUSH(&frame->stack, TO_VALUE(TO_INT(acc)));
    } else {
        STACK_PUSH(&frame->stack, TO_VALUE(dacc));
    }

    return true;
}

static inline bool vm_exec_sub(kokos_vm_t* vm, uint64_t count)
{
    kokos_frame_t* frame = current_frame(vm);

    int32_t acc = 0;
    double dacc = nan.as_double;

    for (size_t i = 0; i < count - 1; i++) {
        kokos_value_t val;
        STACK_POP(&frame->stack, &val);

        if (VALUE_TAG(val) == INT_TAG) {
            int32_t i = GET_INT(val);
            if (IS_NAN_DOUBLE(dacc)) {
                acc -= i;
                continue;
            }

            dacc += (double)i;
            continue;
        }

        CHECK_DOUBLE(val);

        if (IS_NAN_DOUBLE(dacc)) {
            dacc = (double)acc - val.as_double;
            continue;
        }

        dacc += val.as_double;
    }

    kokos_value_t val;
    STACK_POP(&frame->stack, &val);
    if (VALUE_TAG(val) == INT_TAG) {
        if (IS_NAN_DOUBLE(dacc)) {
            acc += GET_INT(val);
            STACK_PUSH(&frame->stack, TO_VALUE(TO_INT(acc)));
            goto success;
        }

        dacc += (double)GET_INT(val);
        STACK_PUSH(&frame->stack, TO_VALUE(dacc));
        goto success;
    }

    CHECK_DOUBLE(val);

    if (IS_NAN_DOUBLE(dacc)) {
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

    int32_t acc = 1;
    double dacc = nan.as_double;

    for (size_t i = 0; i < count; i++) {
        kokos_value_t val;
        STACK_POP(&frame->stack, &val);

        if (VALUE_TAG(val) == INT_TAG) {
            int32_t i = GET_INT(val);
            if (IS_NAN_DOUBLE(dacc)) {
                acc *= i;
                continue;
            }

            dacc *= (double)i;
            continue;
        }

        CHECK_DOUBLE(val);

        if (IS_NAN_DOUBLE(dacc)) {
            dacc = (double)acc * val.as_double;
            continue;
        }

        dacc *= val.as_double;
    }

    if (IS_NAN_DOUBLE(dacc)) {
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

    int32_t divisor = 1;
    double d_divisor = nan.as_double;

    for (size_t i = 0; i < count - 1; i++) {
        kokos_value_t val;
        STACK_POP(&frame->stack, &val);

        if (VALUE_TAG(val) == INT_TAG) {
            int32_t i = GET_INT(val);
            if (IS_NAN_DOUBLE(d_divisor)) {
                divisor *= i;
                continue;
            }

            d_divisor *= (double)i;
            continue;
        }

        CHECK_DOUBLE(val);

        if (IS_NAN_DOUBLE(d_divisor)) {
            d_divisor = (double)divisor;
        }

        d_divisor *= val.as_double;
    }

    kokos_value_t divident;
    STACK_POP(&frame->stack, &divident);
    if (VALUE_TAG(divident) == INT_TAG) {
        if (IS_NAN_DOUBLE(d_divisor)) {
            int64_t res = TO_INT(GET_INT(divident) / divisor);
            STACK_PUSH(&frame->stack, TO_VALUE(res));
            goto success;
        }

        double res = (double)GET_INT(divident) / d_divisor;
        STACK_PUSH(&frame->stack, TO_VALUE(res));
        goto success;
    }

    CHECK_DOUBLE(divident);

    if (IS_NAN_DOUBLE(d_divisor)) {
        double res = divident.as_double / (double)divisor;
        STACK_PUSH(&frame->stack, TO_VALUE(res));
        goto success;
    }

    STACK_PUSH(&frame->stack, TO_VALUE(divident.as_double / d_divisor));

success:
    return true;
}

static kokos_token_t get_call_location(kokos_vm_t* vm, size_t ip)
{
    kokos_token_t* tok = ht_find(&vm->store.call_locations, (void*)ip);
    KOKOS_ASSERT(tok);

    return *tok;
}

static kokos_frame_t* bottom_frame(kokos_vm_t* vm)
{
    KOKOS_VERIFY(vm->frames.sp != 0);
    return vm->frames.data[0];
}

static bool kokos_vm_exec_cur(kokos_vm_t* vm)
{
    kokos_frame_t* frame = current_frame(vm);
    kokos_instruction_t instruction = current_instruction(vm);

    /* kokos_instruction_dump(instruction); */
    /* printf("\n"); */

    switch (instruction.type) {
    case I_PUSH: {
        STACK_PUSH(&frame->stack, TO_VALUE(instruction.operand));
        vm->ip++;
        break;
    }
    case I_POP: {
        kokos_value_t foo;
        STACK_POP(&frame->stack, &foo);
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
        uint16_t nargs = instruction.operand >> 48;
        kokos_runtime_string_t* pname = GET_STRING_INT(instruction.operand);

        kokos_value_t local;
        if (!kokos_env_lookup(frame->env, pname, &local)) {
            kokos_vm_ex_set_undefined_variable(vm, pname);
            return false;
        }

        CHECK_TYPE(local, PROC_TAG);

        kokos_runtime_proc_t* proc = GET_PROC(local);

        if (proc->type == PROC_NATIVE) {
            kokos_value_t ret = KOKOS_NIL;
            proc->native(vm, nargs, &ret);
            STACK_PUSH(&frame->stack, ret);
            vm->ip++;
            break;
        }

        kokos_proc_t kokos = proc->kokos;
        size_t ret_location = vm->ip + 1;

        kokos_frame_t* bot_frame = bottom_frame(vm);
        kokos_frame_t* new_frame = kokos_make_frame(vm, proc, ret_location, (kokos_token_t) { 0 }, bot_frame->env);

        if (!kokos.params.variadic) {
            if (kokos.params.len != nargs) {
                kokos_vm_ex_set_arity_mismatch(vm, kokos.params.len, nargs);
                return false;
            }

            for (size_t i = 0; i < nargs; i++) {
                kokos_value_t value;
                STACK_POP(&frame->stack, &value);
                kokos_env_add(new_frame->env, proc->kokos.params.names[i], value);
            }
        } else {
            size_t reg_count = kokos.params.len - 1;

            if (nargs < reg_count) {
                kokos_vm_ex_set_arity_mismatch(vm, reg_count, nargs);
                return false;
            }

            // push all non-variadic args
            for (size_t i = 0; i < reg_count; i++) {
                kokos_value_t value;
                STACK_POP(&frame->stack, &value);
                kokos_env_add(new_frame->env, kokos.params.names[i], value);
            }

            size_t var_count = nargs - reg_count;

            kokos_runtime_vector_t* variadics = kokos_vm_gc_alloc(vm, VECTOR_TAG, var_count);

            for (size_t i = 0; i < var_count; i++) {
                kokos_value_t value;
                STACK_POP(&frame->stack, &value);
                DA_ADD(variadics, value);
            }

            kokos_env_add(new_frame->env, kokos.params.names[kokos.params.len - 1], TO_VECTOR(variadics));
        }

        // set this to 0 so it points to the first instruction of the called procedure
        vm->ip = 0;

        break;
    }
    case I_RET: {
        // NOTE: use peek here so we can examine the top-level stack frame
        // of the vm after it has ran
        kokos_frame_t* proc_frame = current_frame(vm);
        vm->ip = proc_frame->ret_location;

        if (vm->frames.sp == 1) {
            break;
        }

        kokos_frame_t* _foo;
        STACK_POP(&vm->frames, &_foo);

        kokos_value_t ret_value
            = proc_frame->stack.sp == 0 ? TO_VALUE(NIL_BITS) : STACK_PEEK(&proc_frame->stack);

        STACK_PUSH(&current_frame(vm)->stack, ret_value);

        break;
    }
    case I_ADD_LOCAL: {
        kokos_runtime_string_t* name = GET_STRING_INT(instruction.operand);
        kokos_value_t value;
        STACK_POP(&frame->stack, &value);
        kokos_env_add(frame->env, name, value);
        vm->ip++;
        break;
    }
    case I_GET_LOCAL: {
        kokos_runtime_string_t* name = GET_STRING_INT(instruction.operand);
        kokos_value_t local;
        if (!kokos_env_lookup(frame->env, name, &local)) {
            kokos_vm_ex_set_undefined_variable(vm, name);
            return false;
        }

        STACK_PUSH(&frame->stack, local);

        vm->ip++;
        break;
    }
    case I_BRANCH: {
        vm->ip += (int32_t)instruction.operand;
        break;
    }
    case I_JZ: {
        kokos_value_t test;
        STACK_POP(&frame->stack, &test);
        if (!kokos_value_to_bool(test)) {
            vm->ip += (int32_t)instruction.operand;
            break;
        }

        vm->ip++;
        break;
    }
    case I_JNZ: {
        kokos_value_t test;
        STACK_POP(&frame->stack, &test);
        if (kokos_value_to_bool(test)) {
            vm->ip += (int32_t)instruction.operand;
            break;
        }

        vm->ip++;
        break;
    }
    case I_CMP: {
        kokos_value_t rhs;
        STACK_POP(&frame->stack, &rhs);
        kokos_value_t lhs;
        STACK_POP(&frame->stack, &lhs);

        TRY(kokos_cmp_values(vm, lhs, rhs));

        vm->ip++;
        break;
    }
    case I_EQ: {
        kokos_value_t top;
        STACK_POP(&frame->stack, &top);
        STACK_PUSH(&frame->stack, TO_BOOL(top.as_int == instruction.operand));

        vm->ip++;
        break;
    }
    case I_NEQ: {
        kokos_value_t top;
        STACK_POP(&frame->stack, &top);
        STACK_PUSH(&frame->stack, TO_BOOL(top.as_int != instruction.operand));

        vm->ip++;
        break;
    }
    case I_ALLOC: {
        kokos_value_t value = kokos_alloc_value(vm, instruction.operand);
        STACK_PUSH(&frame->stack, value);
        vm->ip++;
        break;
    }
    case I_PUSH_SCOPE: {
        kokos_env_t* env = kokos_env_create(frame->env, instruction.operand);
        frame->env = env;
        vm->ip++;
        break;
    }
    case I_POP_SCOPE: {
        KOKOS_ASSERT(frame->env->parent != NULL);

        kokos_env_t* env = frame->env;

        frame->env = frame->env->parent;
        kokos_env_destroy(env);
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

static char const* kokos_tag_str(uint16_t tag)
{
    switch (tag) {
    case STRING_TAG: return "string";
    case MAP_TAG:    return "map";
    case VECTOR_TAG: return "vector";
    case LIST_TAG:   return "list";
    case DOUBLE_TAG: return "double";
    case INT_TAG:    return "int";
    default:
        KOKOS_VERIFY(tag == OBJ_BITS >> 48);
        return "bool (or nil)";
    }

    KOKOS_VERIFY(false);
}

static void kokos_vm_dump_stack_trace(kokos_vm_t* vm)
{
    while (vm->frames.sp != 1) {
        kokos_frame_t* frame;
        STACK_POP(&vm->frames, &frame);
        kokos_location_t where = frame->where.location;
        printf("%s:%lu:%lu\n", where.filename, where.row, where.col);
    }
}

static void kokos_vm_report_exception(kokos_vm_t* vm)
{
    switch (vm->registers.exception.type) {
    case EX_TYPE_MISMATCH: {
        char const* expected = kokos_tag_str(vm->registers.exception.type_mismatch.expected);
        char const* got = kokos_tag_str(vm->registers.exception.type_mismatch.got);
        fprintf(stderr, "type mismatch: expected %s, but got %s\n", expected, got);
        break;
    }
    case EX_ARITY_MISMATCH: {
        fprintf(stderr, "arity mismatch: expected %lu arguments, but got %lu\n",
            vm->registers.exception.arity_mismatch.expected,
            vm->registers.exception.arity_mismatch.got);
        break;
    }
    case EX_CUSTOM: {
        fprintf(stderr, "error: %s\n", vm->registers.exception.custom);
        break;
    }
    case EX_UNDEFINED_VARIABLE: {
        kokos_runtime_string_t* name = vm->registers.exception.undefined_variable.name;
        fprintf(stderr, "undefined variable " SV_FMT, (int)name->len, name->ptr);
    }
    }

    kokos_vm_dump_stack_trace(vm);
}

static void kokos_vm_run_until_completion(kokos_vm_t* vm, kokos_code_t instructions)
{
    if (vm->frames.sp == 0) {
        kokos_frame_t* frame = alloc_frame(instructions.len, 79, instructions, NULL);
        vm->frames.cap++;
        STACK_PUSH(&vm->frames, frame);
    }

    while (vm->ip < current_frame(vm)->instructions.len) {
        if (!kokos_vm_exec_cur(vm)) {
            kokos_vm_dump(vm);
            kokos_vm_report_exception(vm);
            exit(1);
        }
    }
}

// TODO: setup a new context for each loaded module
void kokos_vm_load_module(kokos_vm_t* vm, kokos_compiled_module_t const* module)
{
    for (size_t i = 0; i < module->string_store.length; i++) {
        kokos_runtime_string_t const* cur = module->string_store.items[i];
        if (!cur) {
            continue;
        }

        kokos_string_store_add(vm->store.strings, cur);
    }

    kokos_vm_run_until_completion(vm, module->instructions);
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

kokos_vm_t* kokos_vm_create(kokos_scope_t* scope)
{
    kokos_vm_t* vm = KOKOS_ZALLOC(sizeof(kokos_vm_t));
    vm->store = (kokos_runtime_store_t) { .strings = scope->string_store,
        .call_locations = scope->call_locations };

    vm->root_scope = scope;
    vm->gc = kokos_gc_new(GC_INITIAL_CAP);
    return vm;
}

void kokos_vm_destroy(kokos_vm_t* vm)
{
    for (size_t i = 0; i < vm->frames.cap; i++) {
        kokos_env_destroy(vm->frames.data[i]->env);
        KOKOS_FREE(vm->frames.data[i]);
    }

    KOKOS_FREE(vm);
}

static void kokos_gc_mark_obj(kokos_gc_t* gc, kokos_gc_obj_t* obj)
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

            kokos_gc_mark_obj(gc, gobj);
        }

        break;
    }
    case MAP_TAG: {
        kokos_runtime_map_t* map = (kokos_runtime_map_t*)(GET_PTR(obj->value));
        HT_ITER(map->table, {
            kokos_gc_obj_t* key = kokos_gc_find(gc, TO_VALUE((uint64_t)kv.key));
            kokos_gc_obj_t* value = kokos_gc_find(gc, TO_VALUE((uint64_t)kv.value));

            if (key) {
                kokos_gc_mark_obj(gc, key);
            }

            if (value) {
                kokos_gc_mark_obj(gc, value);
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

static void kokos_gc_mark_frame(kokos_gc_t* gc, kokos_frame_t const* frame)
{
    HT_ITER(frame->env->vars, {
        kokos_value_t local = FROM_PTR(kv.value);
        kokos_gc_obj_t* obj = kokos_gc_find(gc, local);

        if (obj) {
            KOKOS_ASSERT(IS_OCCUPIED(*obj));
            kokos_gc_mark_obj(gc, obj);
        }
    });

    for (size_t i = 0; i < frame->stack.sp; i++) {
        kokos_value_t value = frame->stack.data[i];
        kokos_gc_obj_t* obj = kokos_gc_find(gc, value);

        if (obj) {
            KOKOS_ASSERT(IS_OCCUPIED(*obj));
            kokos_gc_mark_obj(gc, obj);
        }
    }
}

static void kokos_obj_free(kokos_gc_obj_t* obj)
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
        kokos_gc_mark_frame(&vm->gc, frame);
    }

    for (size_t i = 0; i < vm->gc.objects.cap; i++) {
        kokos_gc_obj_t* obj = &vm->gc.objects.values[i];
        if (!IS_OCCUPIED(*obj) || IS_MARKED(*obj)) {
            continue;
        }

        obj->flags = 0;

        kokos_obj_free(obj);

        vm->gc.objects.len--;
    }
}

void* kokos_vm_gc_alloc(kokos_vm_t* vm, uint64_t tag, size_t cap)
{
#define DEFAULT_CAP 11
    cap = cap || DEFAULT_CAP;
#undef DEFAULT_CAP

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
        addr = kokos_runtime_string_new(NULL, 0);
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
    vm->registers.exception = (kokos_exception_t) {
        .type = EX_TYPE_MISMATCH,
    };
    vm->registers.exception.type_mismatch.expected = expected;
    vm->registers.exception.type_mismatch.got = got;
}

void kokos_vm_ex_set_arity_mismatch(kokos_vm_t* vm, size_t expected, size_t got)
{
    vm->registers.exception = (kokos_exception_t) {
        .type = EX_ARITY_MISMATCH,
    };
    vm->registers.exception.arity_mismatch.expected = expected;
    vm->registers.exception.arity_mismatch.got = got;
}

void kokos_vm_ex_set_undefined_variable(kokos_vm_t* vm, kokos_runtime_string_t* name)
{
    vm->registers.exception = (kokos_exception_t) {
        .type = EX_UNDEFINED_VARIABLE,
    };
    vm->registers.exception.undefined_variable.name = name;
}

void kokos_vm_ex_custom_printf(kokos_vm_t* vm, char const* fmt, ...)
{
    vm->registers.exception = (kokos_exception_t) {
        .type = EX_CUSTOM,
    };

    string_builder sb = sb_new(256);

    va_list args;
    va_start(args, fmt);
    sb_sprintf(&sb, fmt, args);
    va_end(args);

    vm->registers.exception.custom = sb_to_cstr(&sb);
}
