#include "obj.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

void kokos_obj_mark(kokos_obj_t* obj)
{
    obj->marked = 1;
    switch (obj->type) {
    case OBJ_NIL:
    case OBJ_INT:
    case OBJ_STRING:
    case OBJ_FLOAT:
    case OBJ_BOOL:
    case OBJ_SYMBOL:
    case OBJ_BUILTIN_PROC:
    case OBJ_SPECIAL_FORM: break;
    case OBJ_LIST:
        for (size_t i = 0; i < obj->list.len; i++)
            kokos_obj_mark(obj->list.objs[i]);
        break;
    case OBJ_VEC:
        for (size_t i = 0; i < obj->vec.len; i++)
            kokos_obj_mark(obj->vec.items[i]);
        break;
    case OBJ_PROCEDURE:
        for (size_t i = 0; i < obj->procedure.body.len; i++)
            kokos_obj_mark(obj->procedure.body.objs[i]);
        break;
    case OBJ_MACRO:
        for (size_t i = 0; i < obj->macro.body.len; i++)
            kokos_obj_mark(obj->macro.body.objs[i]);
        break;
    case OBJ_MAP:
        for (size_t i = 0; i < obj->map.cap; i++) {
            ht_bucket* bucket = obj->map.buckets[i];
            if (!bucket)
                continue;

            for (size_t j = 0; j < bucket->len; j++) {
                kokos_obj_mark((kokos_obj_t*)bucket->items[j].key);
                kokos_obj_mark((kokos_obj_t*)bucket->items[j].value);
            }
        }
        break;
    }
}

void kokos_obj_print(kokos_obj_t* obj)
{
    switch (obj->type) {
    case OBJ_NIL:    printf("nil"); break;
    case OBJ_INT:    printf("%ld", obj->integer); break;
    case OBJ_STRING: printf("\"%s\"", obj->string); break;
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
    case OBJ_VEC:
        printf("[");
        for (size_t i = 0; i < obj->vec.len; i++) {
            kokos_obj_print(obj->vec.items[i]);
            if (i != obj->vec.len - 1)
                printf(" ");
        }
        printf("]");
        break;
    case OBJ_MAP:
        printf("{");

        size_t iter_count = 0;
        for (size_t i = 0; i < obj->map.cap; i++) {
            ht_bucket* bucket = obj->map.buckets[i];
            if (!bucket)
                continue;

            iter_count++;
            for (size_t j = 0; j < bucket->len; j++) {
                kokos_obj_print((kokos_obj_t*)bucket->items[j].key);
                printf(" ");
                kokos_obj_print((kokos_obj_t*)bucket->items[j].value);
            }
            if (iter_count != obj->map.len)
                printf(" ");
        }
        printf("}");
        break;
    case OBJ_BUILTIN_PROC: printf("<builtin function>"); break;
    case OBJ_PROCEDURE:    printf("<procedure>"); break;
    case OBJ_MACRO:        printf("<macro>"); break;
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
    case OBJ_INT:       result->integer = obj->integer; break;
    case OBJ_FLOAT:     result->floating = obj->floating; break;
    case OBJ_SYMBOL:    result->symbol = strdup(obj->symbol); break;
    case OBJ_STRING:    result->string = strdup(obj->symbol); break;
    case OBJ_LIST:      result->list = kokos_list_dup(interp, obj->list); break;
    case OBJ_PROCEDURE: {
        result->procedure.body = kokos_list_dup(interp, obj->procedure.body);
        char** names = malloc(sizeof(char*) * obj->procedure.params.len);
        for (size_t i = 0; i < obj->procedure.params.len; i++)
            names[i] = strdup(obj->procedure.params.names[i]);
        result->procedure.params.names = names;
        result->procedure.params.len = obj->procedure.params.len;
        break;
    }
    default: assert(0 && "unreachable!");
    }

    return result;
}

inline bool kokos_obj_to_bool(const kokos_obj_t* obj)
{
    return obj != &kokos_obj_false && obj != &kokos_obj_nil;
}

inline kokos_obj_t* kokos_bool_to_obj(bool b)
{
    return b ? &kokos_obj_true : &kokos_obj_false;
}

bool kokos_obj_eq(const kokos_obj_t* left, const kokos_obj_t* right)
{
    if (left == right)
        return true;

    if (left->type != right->type)
        return false;

    switch (left->type) {
    case OBJ_FLOAT:
        if (isnan(left->floating))
            return isnan(right->floating);

        return left->floating == right->floating;
    case OBJ_INT:       return left->integer == right->integer;
    case OBJ_BOOL:      return false;
    case OBJ_MACRO:
    case OBJ_PROCEDURE: return left == right;
    case OBJ_LIST:      {
        if (left->list.len != right->list.len)
            return false;

        for (size_t i = 0; i < left->list.len; i++) {
            if (kokos_obj_eq(left->list.objs[i], right->list.objs[i]))
                return false;
        }

        return true;
    }
    case OBJ_VEC:
        if (left->vec.len != right->vec.len)
            return false;

        for (size_t i = 0; i < left->vec.len; i++) {
            if (kokos_obj_eq(left->vec.items[i], right->vec.items[i]))
                return false;
        }

        return true;
    case OBJ_MAP: {
        return false; // TODO: find a way to compare two maps
    }
    case OBJ_STRING:       return strcmp(left->string, right->string) == 0;
    case OBJ_SPECIAL_FORM:
    case OBJ_BUILTIN_PROC: return left->builtin == right->builtin;
    case OBJ_SYMBOL:
    case OBJ_NIL:          __builtin_unreachable();
    }
    assert(0 && "invalid object type");
}

kokos_obj_t kokos_obj_nil = { .type = OBJ_NIL };
kokos_obj_t kokos_obj_false = { .type = OBJ_BOOL };
kokos_obj_t kokos_obj_true = { .type = OBJ_BOOL };
