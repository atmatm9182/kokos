#ifndef AST_H_
#define AST_H_

#include "base.h"
#include "macros.h"
#include "token.h"

#include <stddef.h>
#include <stdio.h>

#define EXPR_FLAGS_NONE 0
#define EXPR_FLAG_QUOTE 1

#define EXPR_QUOTED(e) ((e)->flags & EXPR_FLAG_QUOTE)

typedef enum {
    EXPR_INT_LIT,
    EXPR_FLOAT_LIT,
    EXPR_STRING_LIT,
    EXPR_LIST,
    EXPR_VECTOR,
    EXPR_MAP,
    EXPR_IDENT,
} kokos_expr_type_e;

typedef struct kokos_list {
    struct kokos_expr* items;
    size_t len;
} kokos_list_t;

typedef struct {
    struct kokos_expr* items;
    size_t len;
    size_t cap;
} kokos_vec_t;

typedef struct kokos_map {
    struct kokos_expr* keys;
    struct kokos_expr* values;
    size_t len;
} kokos_map_t;

typedef struct kokos_expr {
    kokos_token_t token;
    kokos_expr_type_e type;
    int flags;
    union {
        kokos_list_t list;
        kokos_vec_t vec;
        kokos_map_t map;
    };
} kokos_expr_t;

typedef struct {
    kokos_expr_t* items;
    size_t len;
    size_t cap;
} kokos_module_t;

static inline void kokos_expr_dump(const kokos_expr_t* expr)
{
    if (EXPR_QUOTED(expr)) {
        printf("'");
    }

    switch (expr->type) {
    case EXPR_FLOAT_LIT:
    case EXPR_INT_LIT:
    case EXPR_IDENT:     sv_print(expr->token.value); break;
    case EXPR_STRING_LIT:
        printf("\"");
        sv_print(expr->token.value);
        printf("\"");
        break;
    case EXPR_LIST: {
        kokos_list_t list = expr->list;
        printf("(");
        for (size_t i = 0; i < list.len; i++) {
            kokos_expr_dump(&list.items[i]);
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
            kokos_expr_dump(&vec.items[i]);
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
            kokos_expr_dump(&map.keys[i]);
            printf(" ");
            kokos_expr_dump(&map.values[i]);

            if (i != map.len - 1) {
                printf(" ");
            }
        }
        printf("}");
        break;
    }
    default: KOKOS_TODO();
    }
}

static inline const char* kokos_expr_type_str(kokos_expr_type_e type)
{
    switch (type) {
    case EXPR_INT_LIT:    return "EXPR_INT_LIT";
    case EXPR_FLOAT_LIT:  return "EXPR_FLOAT_LIT";
    case EXPR_STRING_LIT: return "EXPR_STRING_LIT";
    case EXPR_LIST:       return "EXPR_LIST";
    case EXPR_VECTOR:     return "EXPR_VECTOR";
    case EXPR_MAP:        return "EXPR_MAP";
    case EXPR_IDENT:      return "EXPR_IDENT";
    default:              KOKOS_TODO();
    }
}

static void kokos_module_dump(kokos_module_t module)
{
    for (size_t i = 0; i < module.len; i++) {
        kokos_expr_dump(&module.items[i]);
        printf("\n");
    }
}

static inline void kokos_expr_destroy(kokos_expr_t* expr)
{
    switch (expr->type) {
    case EXPR_INT_LIT:
    case EXPR_FLOAT_LIT:
    case EXPR_STRING_LIT:
    case EXPR_IDENT:      break;

    case EXPR_VECTOR:
    case EXPR_LIST:
        _Static_assert(offsetof(kokos_list_t, items) == offsetof(kokos_vec_t, items));

        for (size_t i = 0; i < expr->list.len; i++) {
            kokos_expr_destroy(&expr->list.items[i]);
        }

        KOKOS_FREE(expr->list.items);
        break;

    case EXPR_MAP:
        for (size_t i = 0; i < expr->map.len; i++) {
            kokos_expr_destroy(&expr->map.keys[i]);
            kokos_expr_destroy(&expr->map.values[i]);
        }

        KOKOS_FREE(expr->map.keys);
        KOKOS_FREE(expr->map.values);
        break;
    default: {
        char buf[512] = { 0 };
        sprintf(buf, "freeing of expressions with type %s", kokos_expr_type_str(expr->type));
        KOKOS_TODO(buf);
    }
    }
}

static inline void kokos_exprs_destroy_recursively(kokos_expr_t* exprs, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        kokos_expr_destroy(&exprs[i]);
    }

    KOKOS_FREE(exprs);
}

static void kokos_module_destroy(kokos_module_t module)
{
    for (size_t i = 0; i < module.len; i++) {
        kokos_expr_destroy(&module.items[i]);
    }

    KOKOS_FREE(module.items);
}

#endif // AST_H_
