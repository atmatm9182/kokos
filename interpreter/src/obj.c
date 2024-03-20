#include "obj.h"
#include "src/gc.h"
#include "src/map.h"
#include "src/util.h"

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
    case OBJ_SPECIAL_FORM: KOKOS_FAIL_WITH("Something went completely wrong");
    }
}

kokos_obj_t* kokos_gc_alloc(struct kokos_gc*);

kokos_obj_list_t kokos_list_dup(struct kokos_gc* gc, kokos_obj_list_t list)
{
    kokos_obj_list_t result;
    struct {
        kokos_obj_t** items;
        size_t len;
        size_t cap;
    } objs_arr;
    DA_INIT(&objs_arr, 0, list.len);

    for (size_t i = 0; i < list.len; i++)
        DA_ADD(&objs_arr, kokos_obj_dup(gc, list.objs[i]));

    result.len = objs_arr.len;
    result.objs = objs_arr.items;

    return result;
}

kokos_obj_t* kokos_obj_dup(struct kokos_gc* gc, kokos_obj_t* obj)
{
    if (obj->type == OBJ_BOOL) // we don't clone anything because all boolean values are variables
                               // with static lifetime
        return obj;

    kokos_obj_t* result = kokos_gc_alloc(gc);
    result->type = obj->type;

    switch (obj->type) {
    case OBJ_INT:       result->integer = obj->integer; break;
    case OBJ_FLOAT:     result->floating = obj->floating; break;
    case OBJ_SYMBOL:    result->symbol = strdup(obj->symbol); break;
    case OBJ_STRING:    result->string = strdup(obj->symbol); break;
    case OBJ_LIST:      result->list = kokos_list_dup(gc, obj->list); break;
    case OBJ_PROCEDURE: {
        result->procedure.body = kokos_list_dup(gc, obj->procedure.body);
        char** names = malloc(sizeof(char*) * obj->procedure.params.len);
        for (size_t i = 0; i < obj->procedure.params.len; i++)
            names[i] = strdup(obj->procedure.params.names[i]);
        result->procedure.params.names = names;
        result->procedure.params.len = obj->procedure.params.len;
        break;
    }
    default: KOKOS_FAIL_WITH("Unreachable");
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

static inline bool is_num(const kokos_obj_t* obj)
{
    return obj->type == OBJ_INT || obj->type == OBJ_FLOAT;
}

bool kokos_obj_eq(const kokos_obj_t* lhs, const kokos_obj_t* rhs)
{
    if (lhs == rhs)
        return true;

    if (lhs->type != rhs->type && !(is_num(lhs) && is_num(rhs)))
        return false;

    switch (lhs->type) {
    case OBJ_FLOAT:
        if (isnan(lhs->floating))
            return isnan(rhs->floating);

        if (rhs->type == OBJ_INT)
            return lhs->floating == (double)rhs->integer;

        return lhs->floating == rhs->floating;
    case OBJ_INT: {
        if (rhs->type == OBJ_FLOAT)
            return (double)lhs->integer == rhs->floating;

        return lhs->integer == rhs->integer;
    }
    case OBJ_BOOL:      return false;
    case OBJ_MACRO:
    case OBJ_PROCEDURE: return lhs == rhs;
    case OBJ_LIST:      {
        if (lhs->list.len != rhs->list.len)
            return false;

        for (size_t i = 0; i < lhs->list.len; i++) {
            if (kokos_obj_eq(lhs->list.objs[i], rhs->list.objs[i]))
                return false;
        }

        return true;
    }
    case OBJ_VEC:
        if (lhs->vec.len != rhs->vec.len)
            return false;

        for (size_t i = 0; i < lhs->vec.len; i++) {
            if (kokos_obj_eq(lhs->vec.items[i], rhs->vec.items[i]))
                return false;
        }

        return true;
    case OBJ_MAP: {
        return false; // NOTE: for now two maps are equal only if they literally point to the same
                      // object. Maybe i should fix that? dunno
    }
    case OBJ_STRING:       return strcmp(lhs->string, rhs->string) == 0;
    case OBJ_SPECIAL_FORM:
    case OBJ_BUILTIN_PROC: return lhs->builtin == rhs->builtin;
    case OBJ_SYMBOL:
    case OBJ_NIL:          __builtin_unreachable();
    }

    KOKOS_FAIL_WITH("Invalid object type");
}

void kokos_obj_free(kokos_obj_t* obj)
{
    switch (obj->type) {
    case OBJ_INT:
    case OBJ_FLOAT:
    case OBJ_BOOL:
    case OBJ_BUILTIN_PROC:
    case OBJ_SPECIAL_FORM: break;
    case OBJ_VEC:          DA_FREE(&obj->vec); break;
    case OBJ_MAP:          kokos_obj_map_destroy(&obj->map); break;
    case OBJ_LIST:         free(obj->list.objs); break;
    case OBJ_STRING:       free(obj->string); break;
    case OBJ_SYMBOL:       free(obj->symbol); break;
    case OBJ_PROCEDURE:
        for (size_t i = 0; i < obj->procedure.params.len; i++)
            free(obj->procedure.params.names[i]);
        free(obj->procedure.params.names);
        free(obj->procedure.body.objs);
        break;
    case OBJ_MACRO:
        for (size_t i = 0; i < obj->macro.params.len; i++)
            free(obj->macro.params.names[i]);
        free(obj->macro.params.names);
        free(obj->macro.body.objs);
        break;

    case OBJ_NIL: return;
    }
    free(obj);
}

kokos_obj_t kokos_obj_nil = { .type = OBJ_NIL };
kokos_obj_t kokos_obj_false = { .type = OBJ_BOOL };
kokos_obj_t kokos_obj_true = { .type = OBJ_BOOL };
