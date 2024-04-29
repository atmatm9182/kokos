#ifndef AST_H_
#define AST_H_

#include "base.h"
#include "token.h"
#include <stdio.h>

typedef enum {
    EXPR_INT_LIT,
    EXPR_FLOAT_LIT,
    EXPR_STRING_LIG,
    EXPR_LIST,
    EXPR_VECTOR,
    EXPR_MAP,
    EXPR_IDENT,
} kokos_expr_type_e;

typedef struct kokos_list {
    struct kokos_expr const** items;
    size_t len;
} kokos_list_t;

typedef struct {
    struct kokos_expr const** items;
    size_t len, cap;
} kokos_vec_t;

typedef struct kokos_map {
    struct kokos_expr const** keys;
    struct kokos_expr const** values;
    size_t len;
} kokos_map_t;

typedef struct kokos_expr {
    kokos_token_t token;
    kokos_expr_type_e type;
    union {
        kokos_list_t list;
        kokos_vec_t vec;
        kokos_map_t map;
    };
} kokos_expr_t;

DA_DECLARE(kokos_program_t, kokos_expr_t*);

static inline void kokos_expr_dump(const kokos_expr_t* expr)
{
    switch (expr->type) {
    case EXPR_FLOAT_LIT:
    case EXPR_INT_LIT:
    case EXPR_IDENT:     sv_print(expr->token.value); break;
    case EXPR_STRING_LIG:
        printf("\"");
        sv_print(expr->token.value);
        printf("\"");
        break;
    case EXPR_LIST: {
        kokos_list_t list = expr->list;
        printf("(");
        for (size_t i = 0; i < list.len; i++) {
            kokos_expr_dump(list.items[i]);
            if (i != list.len - 1) {
                printf(" ");
            }
        }
        printf(")");
        break;
    }
    case EXPR_VECTOR: {
        kokos_vec_t vec = expr->vec;
        printf("[");
        for (size_t i = 0; i < vec.len; i++) {
            kokos_expr_dump(vec.items[i]);
            if (i != vec.len - 1) {
                printf(" ");
            }
        }
        printf("]");
        break;
    }
    case EXPR_MAP: {
        kokos_map_t map = expr->map;
        printf("{");
        for (size_t i = 0; i < map.len; i++) {
            kokos_expr_dump(map.keys[i]);
            printf(" ");
            kokos_expr_dump(map.values[i]);

            if (i != map.len - 1) {
                printf(" ");
            }
        }
        printf("}");
        break;
    }
    }
}

#endif // AST_H_
