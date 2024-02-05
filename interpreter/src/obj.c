#include "obj.h"

void kokos_obj_mark(kokos_obj_t* obj)
{
    obj->marked = 1;
    switch (obj->type) {
    case OBJ_INT:
    case OBJ_STRING:
    case OBJ_SYMBOL:
    case OBJ_BUILTIN_FUNC:
    case OBJ_SPECIAL_FORM:
        break;
    case OBJ_LIST:
        for (size_t i = 0; i < obj->list.len; i++) {
            kokos_obj_mark(obj->list.objs[i]);
        }
        break;
    }
}

kokos_obj_t kokos_obj_nil = { 0 };
