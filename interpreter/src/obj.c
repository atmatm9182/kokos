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

kokos_obj_t kokos_obj_nil = { 0 };

void kokos_obj_print(kokos_obj_t* obj)
{
    if (obj == &kokos_obj_nil) {
        printf("nil");
        return;
    }

    switch (obj->type) {
    case OBJ_INT:    printf("%ld", obj->integer); break;
    case OBJ_STRING: printf("\"%s\"", obj->string); break;
    case OBJ_FLOAT:  printf("%lf", obj->floating); break;
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
