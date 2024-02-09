#include "obj.h"

#include <assert.h>
#include <stdio.h>

void kokos_obj_mark(kokos_obj_t* obj)
{
    obj->marked = 1;
    switch (obj->type) {
    case OBJ_INT:
    case OBJ_STRING:
    case OBJ_FLOAT:
    case OBJ_BOOL:
    case OBJ_SYMBOL:
    case OBJ_BUILTIN_PROC:
    case OBJ_SPECIAL_FORM: break;
    case OBJ_LIST:
        for (size_t i = 0; i < obj->list.len; i++) {
            kokos_obj_mark(obj->list.objs[i]);
        }
        break;
    case OBJ_PROCEDURE:
        for (size_t i = 0; i < obj->procedure.params.len; i++) {
            kokos_obj_mark(obj->procedure.params.objs[i]);
        }

        for (size_t i = 0; i < obj->procedure.body.len; i++) {
            kokos_obj_mark(obj->procedure.body.objs[i]);
        }
        break;
    }
}

void kokos_obj_print(kokos_obj_t* obj)
{
    if (obj == &kokos_obj_nil) {
        printf("nil");
        return;
    }

    switch (obj->type) {
    case OBJ_INT:    printf("%ld", obj->integer); break;
    case OBJ_STRING: printf("%s", obj->string); break;
    case OBJ_FLOAT:  printf("%lf", obj->floating); break;
    case OBJ_BOOL:
        if (obj == &kokos_obj_false)
            printf("false");
        else
            printf("true");
        break;
    case OBJ_SYMBOL: printf("%s", obj->symbol); break;
    case OBJ_LIST:
        printf("(");
        for (size_t i = 0; i < obj->list.len; i++) {
            kokos_obj_print(obj->list.objs[i]);
            if (i != obj->list.len - 1) {
                printf(" ");
            }
        }
        printf(")");
        break;
    case OBJ_BUILTIN_PROC: printf("<builtin function> addr %p", (void*)obj->builtin); break;
    case OBJ_PROCEDURE:    printf("<procedure> %p", (void*)obj); break;
    case OBJ_SPECIAL_FORM: assert(0 && "something went completely wrong");
    }
}

kokos_obj_t* kokos_interp_alloc(struct kokos_interp*);

kokos_obj_list_t kokos_list_dup(struct kokos_interp* interp, kokos_obj_list_t list)
{
    kokos_obj_list_t result;
    struct {
        kokos_obj_t** items;
        size_t len;
        size_t cap;
    } objs_arr;
    DA_INIT(&objs_arr, 0, list.len);

    for (size_t i = 0; i < list.len; i++)
        DA_ADD(&objs_arr, kokos_obj_dup(interp, list.objs[i]));

    result.len = objs_arr.len;
    result.objs = objs_arr.items;

    return result;
}

kokos_obj_t* kokos_obj_dup(struct kokos_interp* interp, kokos_obj_t* obj)
{
    if (obj->type == OBJ_BOOL) // we don't clone anything because all boolean values are variables
                               // with static lifetime
        return obj;

    kokos_obj_t* result = kokos_interp_alloc(interp);
    result->type = obj->type;

    switch (obj->type) {
    case OBJ_INT:    result->integer = obj->integer; break;
    case OBJ_FLOAT:  result->floating = obj->floating; break;
    case OBJ_SYMBOL: result->symbol = strdup(obj->symbol); break;
    case OBJ_STRING: result->string = strdup(obj->symbol); break;
    case OBJ_LIST:   result->list = kokos_list_dup(interp, obj->list); break;
    case OBJ_PROCEDURE:
        result->procedure.body = kokos_list_dup(interp, obj->procedure.body);
        result->procedure.params = kokos_list_dup(interp, obj->procedure.params);
        break;
    default: assert(0 && "unreachable!");
    }

    return result;
}

kokos_obj_t kokos_obj_nil = { 0 };
kokos_obj_t kokos_obj_false = { .type = OBJ_BOOL };
kokos_obj_t kokos_obj_true = { .type = OBJ_BOOL };
