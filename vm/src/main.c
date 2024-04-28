#include "ast.h"
#include "base.h"
#include "lexer.h"
#include "parser.h"
#include "src/macros.h"
#include <stdint.h>

#define STACK_SIZE 4096

// here we use the technique called 'NaN boxing'
// we can abuse the representation of NaN as defined by IEEE754
// the top 2 bits of NaN are always set to one, and at least one
// bit in the mantissa set to one, giving us a bitmask 0x7FF8

#define OBJ_BITS 0x7FFC000000000000

typedef union {
    double as_double;
    uint64_t as_int;
} value_t;

#define STRING_BITS 0x7FFE000000000000
#define MAP_BITS 0x7FFF000000000000
#define LIST_BITS 0x7FFF800000000000
#define VECTOR_BITS 0x7FFD000000000000

#define IS_DOUBLE(val) (((val).as_int & OBJ_BITS) != OBJ_BITS)
#define IS_STRING(val) (((val).as_int & STRING_BITS) == STRING_BITS)
#define IS_LIST(val) (((val).as_int & LIST_BITS) == LIST_BITS)
#define IS_MAP(val) (((val).as_int & MAP_BITS) == MAP_BITS)
#define IS_VECTOR(val) (((val).as_int & VECTOR_BITS) == VECTOR_BITS)

typedef struct {
    value_t data[STACK_SIZE];
    size_t sp;
} stack_t;

void stack_push(stack_t* stack, value_t value)
{
    stack->data[stack->sp++] = value;
}

value_t stack_pop(stack_t* stack)
{
    return stack->data[--stack->sp];
}

typedef struct {
    stack_t stack;
} vm_t;

typedef enum { I_PUSH, I_POP, I_ADD } instruction_type_e;

typedef struct {
    instruction_type_e type;
    uint64_t operand;
} instruction_t;

#define INSTR_PUSH(op) ((instruction_t) { .type = I_PUSH, .operand = (op) })
#define INSTR_POP(op) ((instruction_t) { .type = I_POP, .operand = (op) })
#define INSTR_ADD(op) ((instruction_t) { .type = I_ADD, .operand = (op) })

#define TO_VALUE(i)                                                                                \
    (_Generic((i), uint64_t: (value_t) { .as_int = (i) }, double: (value_t) { .as_double = (i) }))

void exec(vm_t* vm, instruction_t instruction)
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
    }
}

DA_DECLARE(code_t, instruction_t);

uint64_t convert(double value)
{
    union {
        double v;
        char bs[8];
    } u;

    u.v = value;
    return (uint64_t)u.bs[0] << 56 | (uint64_t)u.bs[1] << 48 | (uint64_t)u.bs[2] << 40
        | (uint64_t)u.bs[3] << 32 | (uint64_t)u.bs[4] << 24 | (uint64_t)u.bs[5] << 16
        | (uint64_t)u.bs[6] << 8 | (uint64_t)u.bs[7];
}

uint64_t to_double_bytes(const kokos_expr_t* expr)
{
    double parsed = sv_atof(expr->token.value);
    return TO_VALUE(parsed).as_int;
}

code_t compile(const kokos_expr_t* expr)
{
    code_t code;
    DA_INIT(&code, 0, 1);

    switch (expr->type) {
    case EXPR_FLOAT_LIT:
    case EXPR_INT_LIT:   {
        uint64_t value = to_double_bytes(expr);
        DA_ADD(&code, INSTR_PUSH(value));
        break;
    }
    case EXPR_LIST: {
        kokos_list_t list = expr->list;
        KOKOS_VERIFY(list.len > 0);
        string_view head = list.items[0]->token.value;

        KOKOS_VERIFY(sv_eq_cstr(head, "+"));

        for (size_t i = 1; i < list.len; i++) {
            const kokos_expr_t* elem = list.items[i];
            KOKOS_VERIFY(elem->type == EXPR_INT_LIT);

            uint64_t num = to_double_bytes(elem);
            DA_ADD(&code, INSTR_PUSH(num));
        }

        DA_ADD(&code, INSTR_ADD(list.len - 1));
        break;
    }
    default: KOKOS_TODO();
    }

    return code;
}

void dump_code(code_t code)
{
    for (size_t i = 0; i < code.len; i++) {
        instruction_t instr = code.items[i];
        switch (instr.type) {
        case I_PUSH: printf("push %lu", instr.operand); break;
        case I_POP:  printf("pop"); break;
        case I_ADD:  printf("add %lu", instr.operand); break;
        }
        printf("\n");
    }
}

void vm_run(vm_t* vm, code_t code)
{
    for (size_t i = 0; i < code.len; i++) {
        instruction_t instr = code.items[i];
        switch (instr.type) {
        case I_PUSH: stack_push(&vm->stack, TO_VALUE(instr.operand)); break;
        case I_POP:  stack_pop(&vm->stack); break;
        case I_ADD:  {
            double acc = 0;
            for (size_t i = 0; i < instr.operand; i++) {
                value_t val = stack_pop(&vm->stack);
                KOKOS_VERIFY(IS_DOUBLE(val));
                acc += val.as_double;
            }
            stack_push(&vm->stack, TO_VALUE(acc));
            break;
        }
        }
    }
}

void dump_vm(const vm_t* vm)
{
    printf("stack:\n");
    for (size_t i = 0; i < vm->stack.sp; i++) {
        printf("\t[%lu] %f\n", i, vm->stack.data[i].as_double);
    }
}

int main()
{
    char code[] = "(+ 1 2 3 4 5)";

    kokos_lexer_t lexer = kokos_lex_buf(code, sizeof(code));
    kokos_parser_t parser = kokos_parser_init(&lexer);

    kokos_expr_t* expr = kokos_parser_next(&parser);
    KOKOS_VERIFY(expr);

    code_t compiled = compile(expr);
    dump_code(compiled);

    vm_t vm = { 0 };
    vm_run(&vm, compiled);

    dump_vm(&vm);

    return 0;
}
