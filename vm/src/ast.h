#ifndef AST_H_
#define AST_H_

#include "base.h"
#include "macros.h"
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

typedef struct kokos_map {
    struct kokos_expr** keys;
    struct kokos_expr** values;
    size_t len;
} kokos_map_t;

typedef struct kokos_expr {
    kokos_token_t token;
    kokos_expr_type_e type;
    union {
        kokos_list_t list;
        kokos_list_t vec;
        kokos_map_t map;
    };
} kokos_expr_t;

static inline void kokos_expr_dump(const kokos_expr_t* expr)
{
    switch (expr->type) {
    case EXPR_FLOAT_LIT:
        printf("float: ");
        sv_print(expr->token.value);
        break;
    case EXPR_INT_LIT:
        printf("int: ");
        sv_print(expr->token.value);
        break;
    case EXPR_IDENT:
        printf("ident: ");
        sv_print(expr->token.value);
        break;
    case EXPR_STRING_LIG:
        printf("string: \"");
        sv_print(expr->token.value);
        printf("\"");
        break;

    case EXPR_LIST: {
        kokos_list_t list = expr->list;
        printf("list(%ld):\n", list.len);
        printf("\t");
        for (size_t i = 0; i < list.len; i++) {
            kokos_expr_dump(list.items[i]);
            printf(" ");
        }
        break;
    }
    default: KOKOS_TODO();
    }
}

#endif // AST_H_
