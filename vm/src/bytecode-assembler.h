#ifndef KOKOS_BYTECODE_ASSEMBLER_H
#define KOKOS_BYTECODE_ASSEMBLER_H

#include "ast.h"
#include "scope.h"

#include <stdbool.h>

// #define KOKOS_DEFINE_NATIVE_PROC(name) (void)name

typedef struct {
    kokos_scope_t* scope;
} kokos_bytecode_assembler_t;

typedef size_t* kokos_label_t;

static inline kokos_label_t kokos_asm_create_label(kokos_bytecode_assembler_t* ass)
{
    (void)ass;
    return KOKOS_ZALLOC(sizeof(size_t));
}

#define SET_SCOPE(s) \
    do { \
    assembler.scope = (s); \
    } while (0)

#define _I(i) \
    do { \
    DA_ADD(&assembler.scope->code, INSTR_##i); \
    } while (0)

#define PUSH(v) _I(PUSH((v)))
#define POP(n) _I(POP((n)))
#define POP1() POP(1)

#define CALL(name) _I(CALL((name)))
#define RET() _I(RET)

#define JZ(l) _I(JZ((l)))
#define BRANCH(l) _I(BRANCH((l)))

#define PUSH_SCOPE(n) _I(PUSH_SCOPE((n)))
#define POP_SCOPE() _I(POP_SCOPE)

#define ADD_LOCAL(n) _I(ADD_LOCAL((n)))

#define ALLOC(t, c) _I(ALLOC(t##_TAG, (c)))

#define ADD(n) _I(ADD((n)))
#define SUB(n) _I(SUB((n)))
#define MUL(n) _I(MUL((n)))
#define DIV(n) _I(DIV((n)))

#define CMP() _I(CMP)

#define EQ(n) _I(EQ((n)))
#define NEQ(n) _I(NEQ((n)))

#define LT() \
    do { \
    CMP();\
    EQ(-1);\
    } while (0)

#define GT() \
    do { \
    CMP();\
    EQ(1);\
    } while (0)

#define LTE() \
    do { \
    CMP();\
    NEQ(1);\
    } while (0)

#define GTE() \
    do { \
    CMP();\
    NEQ(-1);\
    } while (0)

#define LABEL() (kokos_asm_create_label(&assembler))
#define LINK(l) (*(l) = scope->code.len)

#define KOKOS_DEFINE_SFORM(name, body) \
    bool kokos_sform_##name(const kokos_expr_t* expr, kokos_scope_t* scope) \
    {                                                                               \
        KOKOS_ASSERT(expr->type == EXPR_LIST);                                      \
        KOKOS_ASSERT(expr->list.len >= 1);                                          \
                                                                                    \
        kokos_list_t args = list_slice(expr->list, 1);                              \
        kokos_location_t where = expr->token.location;                              \
        kokos_bytecode_assembler_t assembler = { .scope = scope };                  \
        do {                                                                        \
            body; \
        } while (0);                                                                \
        (void)assembler;                                                            \
        (void)where; \
        (void)args; \
        return true;                                                                \
    }

#define ENUMERATE_SFORMS \
    X(lambda, lambda)    \
    X(macro, macro)      \
    X(var, var)          \
    X(proc, proc)        \
    X(let, let)          \
    X(plus, +)           \
    X(minus, -)          \
    X(mul, *)            \
    X(div, /)            \
    X(if, if)            \
    X(lt, <)             \
    X(gt, >)             \
    X(lte, <=)           \
    X(gte, >=)           \
    X(eq, =)             \
    X(neq, /=)           \

#endif // KOKOS_BYTECODE_ASSEMBLER_H
