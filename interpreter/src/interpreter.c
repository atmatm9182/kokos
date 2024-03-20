#include "interpreter.h"
#include "location.h"
#include "src/env.h"
#include "src/map.h"
#include "src/obj.h"
#include "src/util.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ERR_BUFFER_CAP 2048
#define DEFAULT_MAP_CAPACITY 11

static char err_buf[ERR_BUFFER_CAP];

const char* kokos_interp_get_error(void)
{
    return err_buf;
}

static void write_err(kokos_location_t location, const char* format, ...)
    __attribute__((format(printf, 2, 3)));

static void write_err(kokos_location_t location, const char* format, ...)
{
    int len = sprintf(err_buf, "%s:%lu:%lu ", location.filename, location.row, location.col);

    va_list sprintf_args;
    va_start(sprintf_args, format);
    vsprintf(err_buf + len, format, sprintf_args);
}

static const char* str_type(kokos_obj_type_e type)
{
    switch (type) {
    case OBJ_NIL:          return "nil";
    case OBJ_INT:          return "int";
    case OBJ_FLOAT:        return "float";
    case OBJ_STRING:       return "string";
    case OBJ_BOOL:         return "bool";
    case OBJ_SYMBOL:       return "symbol";
    case OBJ_BUILTIN_PROC: return "builtin procedure";
    case OBJ_PROCEDURE:    return "procedure";
    case OBJ_LIST:         return "list";
    case OBJ_VEC:          return "vector";
    case OBJ_MAP:          return "map";
    case OBJ_SPECIAL_FORM: return "special form";
    case OBJ_MACRO:        return "macro";
    }

    __builtin_unreachable();
}

static void type_mismatch_va(
    kokos_location_t where, kokos_obj_type_e got, int expected_count, va_list args)
{
    string_builder sb = sb_new(25);

    for (int i = 0; i < expected_count; i++) {
        kokos_obj_type_e t = va_arg(args, kokos_obj_type_e);
        sb_push_cstr(&sb, str_type(t));
        if (i != expected_count - 1)
            sb_push_cstr(&sb, ", ");
    }

    char tmp_buf[512];
    char* expected_str = sb_to_cstr(&sb);
    if (expected_count > 1)
        sprintf(tmp_buf, "one of %s", expected_str);
    else
        sprintf(tmp_buf, "%s", expected_str);

    write_err(where, "Type mismatch: got %s, expected %s", str_type(got), tmp_buf);
    sb_destroy(&sb);
}

static void type_mismatch(kokos_location_t where, kokos_obj_type_e got, int expected_count, ...)
{
    va_list expected_types;
    va_start(expected_types, expected_count);
    type_mismatch_va(where, got, expected_count, expected_types);
    va_end(expected_types);
}

static bool expect_type(const kokos_obj_t* obj, int expected_count, ...)
{
    va_list args;
    va_start(args, expected_count);
    va_list err_args;
    va_copy(err_args, args);

    for (int i = 0; i < expected_count; i++) {
        if (obj->type == va_arg(args, kokos_obj_type_e)) {
            va_end(args);
            return true;
        }
    }

    type_mismatch_va(obj->token.location, obj->type, expected_count, err_args);
    va_end(args);
    return false;
}

typedef enum {
    P_EQUAL,
    P_AT_LEAST,
} arity_predicate_e;

static bool expect_arity(kokos_location_t location, int expected, int got, arity_predicate_e pred)
{
    switch (pred) {
    case P_EQUAL:
        if (expected != got) {
            write_err(location, "Arity mismatch: expected %d arguments, got %d", expected, got);
            return false;
        }
        break;
    case P_AT_LEAST:
        if (got < expected) {
            write_err(
                location, "Arity mismatch: expected at least %d arguments, got %d", expected, got);
            return false;
        }
        break;
    }

    return true;
}

static kokos_obj_list_t list_to_args(kokos_obj_list_t list)
{
    return (kokos_obj_list_t) { .objs = list.objs + 1, .len = list.len - 1 };
}

static void push_env(kokos_interp_t* interp, kokos_env_t* env)
{
    env->parent = interp->current_env;
    interp->current_env = env;
}

static void pop_env(kokos_interp_t* interp)
{
    interp->current_env = interp->current_env->parent;
}

static kokos_obj_t* call_proc(
    kokos_interp_t* interp, kokos_obj_procedure_t proc, kokos_obj_list_t args)
{
    kokos_env_t call_env = kokos_env_empty(args.len);
    size_t i;
    for (i = 0; i < proc.params.len - 1; i++) {
        kokos_obj_t* obj = kokos_interp_eval(interp, args.objs[i], 0);
        if (!obj)
            return NULL;

        kokos_env_add(&call_env, proc.params.names[i], obj);
    }

    if (proc.params.var) {
        kokos_obj_t* rest_obj = kokos_gc_alloc(&interp->gc);
        rest_obj->type = OBJ_VEC;
        kokos_obj_vec_t* rest = &rest_obj->vec;

        size_t cap = args.len == 0 ? 0 : args.len - proc.params.len;
        DA_INIT(rest, 0, cap);
        for (; i < args.len; i++) {
            kokos_obj_t* obj = kokos_interp_eval(interp, args.objs[i], 0);
            if (!obj)
                return NULL;

            DA_ADD(rest, obj);
        }

        kokos_env_add(&call_env, proc.params.names[proc.params.len - 1], rest_obj);
    } else {
        kokos_obj_t* last = kokos_interp_eval(interp, args.objs[args.len - 1], false);
        if (!last)
            return NULL;

        kokos_env_add(&call_env, proc.params.names[proc.params.len - 1], last);
    }

    push_env(interp, &call_env);
    for (size_t i = 0; i < proc.body.len - 1; i++) {
        if (!kokos_interp_eval(interp, proc.body.objs[i], 0))
            return NULL;
    }

    kokos_obj_t* result = kokos_interp_eval(interp, proc.body.objs[proc.body.len - 1], 0);
    pop_env(interp);
    kokos_env_destroy(&call_env);

    return result;
}

static inline kokos_obj_t* call_macro(
    kokos_interp_t* interp, kokos_obj_macro_t macro, kokos_obj_list_t args)
{
    kokos_env_t call_env = kokos_env_empty(args.len);
    size_t i;
    for (i = 0; i < macro.params.len - 1; i++) {
        kokos_env_add(&call_env, macro.params.names[i], args.objs[i]);
    }

    if (macro.params.var) {
        kokos_obj_t* rest_obj = kokos_gc_alloc(&interp->gc);
        rest_obj->type = OBJ_VEC;
        kokos_obj_vec_t* rest = &rest_obj->vec;

        size_t cap = args.len == 0 ? 0 : args.len - macro.params.len;
        DA_INIT(rest, 0, cap);
        for (; i < args.len; i++) {
            DA_ADD(rest, args.objs[i]);
        }

        kokos_env_add(&call_env, macro.params.names[macro.params.len - 1], rest_obj);
    } else {
        kokos_env_add(&call_env, macro.params.names[macro.params.len - 1], args.objs[args.len - 1]);
    }

    push_env(interp, &call_env);
    for (size_t i = 0; i < macro.body.len - 1; i++) {
        if (!kokos_interp_eval(interp, macro.body.objs[i], 0))
            return NULL;
    }

    kokos_obj_t* result = kokos_interp_eval(interp, macro.body.objs[macro.body.len - 1], 0);
    pop_env(interp);
    kokos_env_destroy(&call_env);

    return result;
}

kokos_obj_t* kokos_interp_eval(kokos_interp_t* interp, kokos_obj_t* obj, bool top_level)
{
    if (obj->quoted)
        return obj;

    kokos_obj_t* result = NULL;
    switch (obj->type) {
    case OBJ_NIL:
    case OBJ_INT:
    case OBJ_STRING:
    case OBJ_FLOAT:
    case OBJ_BOOL:
    case OBJ_VEC:
    case OBJ_MAP:
    case OBJ_BUILTIN_PROC:
    case OBJ_MACRO:
    case OBJ_PROCEDURE:    result = obj; break;
    case OBJ_SYMBOL:       {
        kokos_env_pair_t* pair = kokos_env_find(interp->current_env, obj->symbol);
        if (!pair) {
            write_err(obj->token.location, "Undefined symbol %s", obj->symbol);
            return NULL;
        }
        result = pair->value;
        break;
    }
    case OBJ_LIST: {
        if (obj->list.len == 0)
            return obj;

        kokos_obj_t* head = kokos_interp_eval(interp, obj->list.objs[0], 0);
        if (!head)
            return NULL;
        switch (head->type) {
        case OBJ_BUILTIN_PROC: {
            struct {
                kokos_obj_t** items;
                size_t len;
                size_t cap;
            } args_arr;

            DA_INIT(&args_arr, 0, obj->list.len - 1);
            for (size_t i = 1; i < obj->list.len; i++) {
                kokos_obj_t* evaluated_arg = kokos_interp_eval(interp, obj->list.objs[i], 0);
                if (!evaluated_arg) {
                    DA_FREE(&args_arr);
                    return NULL;
                }
                DA_ADD(&args_arr, evaluated_arg);
            }

            kokos_obj_list_t args = { .objs = args_arr.items, .len = args_arr.len };

            kokos_builtin_procedure_t proc = head->builtin;
            result = proc(interp, args, head->token.location);
            DA_FREE(&args_arr);
            break;
        }

        case OBJ_SPECIAL_FORM: {
            kokos_obj_list_t args = list_to_args(obj->list);

            kokos_builtin_procedure_t proc = head->builtin;
            result = proc(interp, args, head->token.location);
            break;
        }

        case OBJ_PROCEDURE: {
            kokos_obj_procedure_t proc = head->procedure;

            kokos_obj_list_t args = list_to_args(obj->list);
            if (!proc.params.var) {
                if (!expect_arity(obj->token.location, proc.params.len, args.len, P_EQUAL))
                    return NULL;
            } else {
                if (!expect_arity(obj->token.location, proc.params.len - 1, args.len, P_AT_LEAST))
                    return NULL;
            }

            result = call_proc(interp, proc, args);
            break;
        }

        case OBJ_MACRO: {
            kokos_obj_macro_t macro = head->macro;

            kokos_obj_list_t args = list_to_args(obj->list);
            if (!macro.params.var) {
                if (!expect_arity(obj->token.location, macro.params.len, args.len, P_EQUAL))
                    return NULL;
            } else {
                if (!expect_arity(obj->token.location, macro.params.len - 1, args.len, P_AT_LEAST))
                    return NULL;
            }

            result = call_macro(interp, macro, args);
            if (!result)
                return NULL;
            result = kokos_interp_eval(interp, result, 0);
            break;
        }
        default:
            write_err(
                obj->token.location, "Object of type '%s' is not callable", str_type(head->type));
            return NULL;
        }
        break;
    }
    case OBJ_SPECIAL_FORM: KOKOS_FAIL_WITH("Unreachable code");
    }

    if (top_level && result && interp->gc.obj_count > interp->gc.obj_threshold) {
        kokos_obj_mark(result);
        kokos_gc_run(&interp->gc, interp->current_env);
    }

    return result;
}

static kokos_obj_t* builtin_plus(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    bool use_float = false;
    int64_t int_res = 0;
    double float_res = 0;
    for (size_t i = 0; i < args.len; i++) {
        kokos_obj_t* obj = args.objs[i];

        switch (obj->type) {
        case OBJ_INT: int_res += obj->integer; break;
        case OBJ_FLOAT:
            use_float = true;
            float_res += obj->floating;
            break;
        default: type_mismatch(obj->token.location, obj->type, 2, OBJ_INT, OBJ_FLOAT); return NULL;
        }
    }

    kokos_obj_t* obj = kokos_gc_alloc(&interp->gc);
    if (use_float) {
        obj->type = OBJ_FLOAT;
        obj->floating = (double)int_res + float_res;
        return obj;
    }

    obj->type = OBJ_INT;
    obj->integer = int_res;
    return obj;
}

static kokos_obj_t* builtin_minus(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (args.len == 0) {
        kokos_obj_t* result = kokos_gc_alloc(&interp->gc);
        result->type = OBJ_INT;
        result->integer = 0;
        return result;
    }

    bool use_float = false;
    int64_t int_res = 0;
    double float_res = 0;

    switch (args.objs[0]->type) {
    case OBJ_INT: int_res = args.objs[0]->integer; break;
    case OBJ_FLOAT:
        float_res = args.objs[0]->floating;
        use_float = true;
        break;
    default:
        type_mismatch(args.objs[0]->token.location, args.objs[0]->type, 2, OBJ_INT, OBJ_FLOAT);
        return NULL;
    }

    for (size_t i = 1; i < args.len; i++) {
        kokos_obj_t* obj = args.objs[i];

        switch (obj->type) {
        case OBJ_INT: int_res -= obj->integer; break;
        case OBJ_FLOAT:
            use_float = true;
            float_res -= obj->floating;
            break;
        default: type_mismatch(obj->token.location, obj->type, 2, OBJ_INT, OBJ_FLOAT); return NULL;
        }
    }

    kokos_obj_t* obj = kokos_gc_alloc(&interp->gc);
    if (use_float) {
        obj->type = OBJ_FLOAT;
        obj->floating = (double)int_res + float_res;
        return obj;
    }

    obj->type = OBJ_INT;
    obj->integer = int_res;
    return obj;
}

static kokos_obj_t* builtin_star(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    bool use_float = false;
    int64_t int_res = 1;
    double float_res = 1;
    for (size_t i = 0; i < args.len; i++) {
        kokos_obj_t* obj = args.objs[i];

        switch (obj->type) {
        case OBJ_INT: int_res *= obj->integer; break;
        case OBJ_FLOAT:
            use_float = true;
            float_res *= obj->floating;
            break;
        default: type_mismatch(obj->token.location, obj->type, 2, OBJ_INT, OBJ_FLOAT); return NULL;
        }
    }

    kokos_obj_t* obj = kokos_gc_alloc(&interp->gc);
    if (use_float) {
        obj->type = OBJ_FLOAT;
        obj->floating = (double)int_res + float_res;
        return obj;
    }

    obj->type = OBJ_INT;
    obj->integer = int_res;
    return obj;
}

static kokos_obj_t* alloc_nan(kokos_interp_t* interp)
{
    kokos_obj_t* obj = kokos_gc_alloc(&interp->gc);
    obj->type = OBJ_FLOAT;
    obj->floating = NAN;
    return obj;
}

static kokos_obj_t* builtin_slash(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (args.len == 0)
        return alloc_nan(interp);

    kokos_obj_t* first = args.objs[0];
    double result;
    switch (args.objs[0]->type) {
    case OBJ_FLOAT: result = first->floating; break;
    case OBJ_INT:   result = (double)first->integer; break;
    default:        type_mismatch(first->token.location, first->type, 2, OBJ_INT, OBJ_FLOAT); return NULL;
    }

    for (size_t i = 1; i < args.len; i++) {
        kokos_obj_t* obj = args.objs[i];
        switch (obj->type) {
        case OBJ_FLOAT: result /= obj->floating; break;
        case OBJ_INT:   result /= (double)obj->integer; break;
        default:        type_mismatch(obj->token.location, obj->type, 2, OBJ_INT, OBJ_FLOAT); return NULL;
        }
    }

    kokos_obj_t* obj = kokos_gc_alloc(&interp->gc);
    obj->type = OBJ_FLOAT;
    obj->floating = result;

    return obj;
}

static kokos_obj_t* builtin_print(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    for (size_t i = 0; i < args.len; i++) {
        kokos_obj_print(args.objs[i]);
        if (i != args.len - 1)
            printf(" ");
    }

    printf("\n");

    return &kokos_obj_nil;
}

static kokos_obj_t* builtin_type(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 1, args.len, P_EQUAL))
        return NULL;

    kokos_obj_t* res = kokos_gc_alloc(&interp->gc);
    res->type = OBJ_STRING;
    res->string = strdup(str_type(args.objs[0]->type));
    return res;
}

static kokos_obj_t* builtin_eq(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 2, args.len, P_AT_LEAST))
        return NULL;

    kokos_obj_t* left = args.objs[0];
    kokos_obj_t* right = args.objs[1];

    if (left->type == OBJ_SYMBOL) {
        left = kokos_interp_eval(interp, left, 0);
        if (!left)
            return NULL;
    }

    if (right->type == OBJ_SYMBOL) {
        right = kokos_interp_eval(interp, right, 0);
        if (!right)
            return NULL;
    }

    return kokos_bool_to_obj(kokos_obj_eq(left, right));
}

static kokos_obj_t* builtin_lt(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 2, args.len, P_EQUAL))
        return NULL;

    kokos_obj_t* left = args.objs[0];
    kokos_obj_t* right = args.objs[1];

    switch (left->type) {
    case OBJ_INT:
        switch (right->type) {
        case OBJ_INT:   return kokos_bool_to_obj(left->integer < right->integer);
        case OBJ_FLOAT: return kokos_bool_to_obj((double)left->integer < right->floating);
        default:
            type_mismatch(right->token.location, right->type, 2, OBJ_INT, OBJ_FLOAT);
            return NULL;
        }
    case OBJ_FLOAT:
        switch (right->type) {
        case OBJ_INT:   return kokos_bool_to_obj(left->floating < (double)right->integer);
        case OBJ_FLOAT: return kokos_bool_to_obj(left->floating < right->floating);
        default:
            type_mismatch(right->token.location, right->type, 2, OBJ_INT, OBJ_FLOAT);
            return NULL;
        }
    default: break;
    }

    type_mismatch(left->token.location, left->type, 2, OBJ_INT, OBJ_FLOAT);
    return NULL;
}

static kokos_obj_t* builtin_gt(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 2, args.len, P_EQUAL))
        return NULL;

    kokos_obj_t* left = args.objs[0];
    kokos_obj_t* right = args.objs[1];

    switch (left->type) {
    case OBJ_INT:
        switch (right->type) {
        case OBJ_INT:   return kokos_bool_to_obj(left->integer > right->integer);
        case OBJ_FLOAT: return kokos_bool_to_obj((double)left->integer > right->floating);
        default:
            type_mismatch(right->token.location, right->type, 2, OBJ_INT, OBJ_FLOAT);
            return NULL;
        }
    case OBJ_FLOAT:
        switch (right->type) {
        case OBJ_INT:   return kokos_bool_to_obj(left->floating > (double)right->integer);
        case OBJ_FLOAT: return kokos_bool_to_obj(left->floating > right->floating);
        default:
            type_mismatch(right->token.location, right->type, 2, OBJ_INT, OBJ_FLOAT);
            return NULL;
        }
    default: break;
    }

    type_mismatch(left->token.location, left->type, 2, OBJ_INT, OBJ_FLOAT);
    return NULL;
}

static kokos_obj_t* builtin_lte(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 2, args.len, P_EQUAL))
        return NULL;

    kokos_obj_t* left = args.objs[0];
    kokos_obj_t* right = args.objs[1];

    switch (left->type) {
    case OBJ_INT:
        switch (right->type) {
        case OBJ_INT:   return kokos_bool_to_obj(left->integer <= right->integer);
        case OBJ_FLOAT: return kokos_bool_to_obj((double)left->integer <= right->floating);
        default:
            type_mismatch(right->token.location, right->type, 2, OBJ_INT, OBJ_FLOAT);
            return NULL;
        }
    case OBJ_FLOAT:
        switch (right->type) {
        case OBJ_INT:   return kokos_bool_to_obj(left->floating <= (double)right->integer);
        case OBJ_FLOAT: return kokos_bool_to_obj(left->floating <= right->floating);
        default:
            type_mismatch(right->token.location, right->type, 2, OBJ_INT, OBJ_FLOAT);
            return NULL;
        }
    default: break;
    }

    type_mismatch(left->token.location, left->type, 2, OBJ_INT, OBJ_FLOAT);
    return NULL;
}

static kokos_obj_t* builtin_gte(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 2, args.len, P_EQUAL))
        return NULL;

    kokos_obj_t* left = args.objs[0];
    kokos_obj_t* right = args.objs[1];

    switch (left->type) {
    case OBJ_INT:
        switch (right->type) {
        case OBJ_INT:   return kokos_bool_to_obj(left->integer >= right->integer);
        case OBJ_FLOAT: return kokos_bool_to_obj((double)left->integer >= right->floating);
        default:
            type_mismatch(right->token.location, right->type, 2, OBJ_INT, OBJ_FLOAT);
            return NULL;
        }
    case OBJ_FLOAT:
        switch (right->type) {
        case OBJ_INT:   return kokos_bool_to_obj(left->floating >= (double)right->integer);
        case OBJ_FLOAT: return kokos_bool_to_obj(left->floating >= right->floating);
        default:
            type_mismatch(right->token.location, right->type, 2, OBJ_INT, OBJ_FLOAT);
            return NULL;
        }
    default: break;
    }

    type_mismatch(left->token.location, left->type, 2, OBJ_INT, OBJ_FLOAT);
    return NULL;
}

static kokos_obj_t* builtin_not(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 1, args.len, P_EQUAL))
        return NULL;

    return kokos_bool_to_obj(!kokos_obj_to_bool(args.objs[0]));
}

static kokos_obj_t* sform_or(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 2, args.len, P_AT_LEAST))
        return NULL;

    kokos_obj_t* result = NULL;
    for (size_t i = 0; i < args.len; i++) {
        kokos_obj_t* obj = args.objs[i];
        result = kokos_interp_eval(interp, obj, false);
        if (!result)
            break;

        if (kokos_obj_to_bool(result))
            break;
    }

    return result;
}

static kokos_obj_t* sform_and(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 2, args.len, P_AT_LEAST))
        return NULL;

    kokos_obj_t* result = NULL;
    for (size_t i = 0; i < args.len; i++) {
        kokos_obj_t* obj = args.objs[i];
        result = kokos_interp_eval(interp, obj, false);
        if (!result)
            break;

        if (!kokos_obj_to_bool(result))
            break;
    }

    return result;
}

static kokos_obj_t* builtin_list(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    kokos_obj_t* list = kokos_gc_alloc(&interp->gc);
    list->type = OBJ_LIST;
    list->list = kokos_list_dup(&interp->gc, args);
    return list;
}

static kokos_obj_t* builtin_make_vec(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    kokos_obj_t* result = kokos_gc_alloc(&interp->gc);
    result->type = OBJ_VEC;

    kokos_obj_vec_t vec;
    DA_INIT(&vec, 0, args.len);
    for (size_t i = 0; i < args.len; i++)
        DA_ADD(&vec, args.objs[i]);

    result->vec = vec;
    return result;
}

static kokos_obj_t* builtin_push(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 2, args.len, P_AT_LEAST))
        return NULL;

    if (!expect_type(args.objs[0], 1, OBJ_VEC))
        return NULL;

    kokos_obj_vec_t* vec = &args.objs[0]->vec;
    for (size_t i = 1; i < args.len; i++)
        DA_ADD(vec, args.objs[i]);

    return &kokos_obj_nil;
}

static kokos_obj_t* builtin_nth(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 2, args.len, P_EQUAL))
        return NULL;

    if (!expect_type(args.objs[0], 1, OBJ_VEC) || !expect_type(args.objs[1], 1, OBJ_INT))
        return NULL;

    kokos_obj_vec_t vec = args.objs[0]->vec;
    int64_t idx = args.objs[1]->integer;

    return idx >= vec.len ? &kokos_obj_nil : vec.items[idx];
}

static kokos_obj_t* builtin_make_map(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    kokos_obj_t* map = kokos_gc_alloc(&interp->gc);
    map->type = OBJ_MAP;
    map->map = kokos_obj_map_make(DEFAULT_MAP_CAPACITY);

    if (args.len % 2 != 0) {
        write_err(called_from, "expected an even number of arguments");
        return NULL;
    }

    for (size_t i = 0; i < args.len; i += 2) {
        kokos_obj_map_add(&map->map, args.objs[i], args.objs[i + 1]);
    }

    return map;
}

static kokos_obj_t* builtin_add_map(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 3, args.len, P_EQUAL))
        return NULL;

    if (!expect_type(args.objs[0], 1, OBJ_MAP))
        return NULL;

    kokos_obj_map_add(&args.objs[0]->map, args.objs[1], args.objs[2]);
    return &kokos_obj_nil;
}

static kokos_obj_t* builtin_find_map(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 2, args.len, P_EQUAL))
        return NULL;

    if (!expect_type(args.objs[0], 1, OBJ_MAP))
        return NULL;

    kokos_obj_t* obj = kokos_obj_map_find(&args.objs[0]->map, args.objs[1]);
    return obj ? obj : &kokos_obj_nil;
}

static kokos_obj_t* builtin_map(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 2, args.len, P_EQUAL))
        return NULL;

    kokos_obj_t* proc = args.objs[0];
    if (!expect_type(proc, 1, OBJ_PROCEDURE))
        return NULL;

    if (!expect_arity(proc->token.location, 1, proc->procedure.params.len, P_EQUAL))
        return NULL;

    kokos_obj_t* collection = args.objs[1];
    kokos_obj_t* result;
    switch (collection->type) {
    case OBJ_VEC: {
        kokos_obj_vec_t vec = collection->vec;
        kokos_obj_vec_t result_vec;
        DA_INIT(&result_vec, 0, vec.len);

        for (size_t i = 0; i < vec.len; i++) {
            kokos_obj_list_t args = { .len = 1, .objs = &vec.items[i] };
            kokos_obj_t* obj = call_proc(interp, proc->procedure, args);
            if (!obj)
                return NULL;

            DA_ADD(&result_vec, obj);
        }

        result = kokos_gc_alloc(&interp->gc);
        result->type = OBJ_VEC;
        result->vec = result_vec;
        break;
    }
    case OBJ_STRING: {
        kokos_obj_vec_t result_vec;
        char* str = collection->string;
        size_t str_len = strlen(str);
        DA_INIT(&result_vec, 0, str_len);

        for (size_t i = 0; i < str_len; i++) {
            char buf[2] = { str[i], '\0' };
            kokos_obj_t c = { .type = OBJ_STRING, .string = buf };
            kokos_obj_t* ca = &c;
            kokos_obj_list_t args = { .len = 1, .objs = &ca };

            kokos_obj_t* obj = call_proc(interp, proc->procedure, args);
            if (!obj)
                return NULL;

            DA_ADD(&result_vec, obj);
        }

        result = kokos_gc_alloc(&interp->gc);
        result->type = OBJ_VEC;
        result->vec = result_vec;
        break;
    }
    default:
        type_mismatch(collection->token.location, collection->type, 2, OBJ_VEC, OBJ_STRING);
        result = NULL;
        break;
    }

    return result;
}

static kokos_obj_t* builtin_read_file(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 1, args.len, P_EQUAL))
        return NULL;

    if (!expect_type(args.objs[0], 1, OBJ_STRING))
        return NULL;

    const char* filename = args.objs[0]->string;
    if (access(filename, F_OK) != 0)
        return &kokos_obj_nil;

    char* contents = read_whole_file(filename);
    kokos_obj_t* result = kokos_gc_alloc(&interp->gc);
    result->type = OBJ_STRING;
    result->string = contents;
    return result;
}

static kokos_obj_t* builtin_write_file(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 2, args.len, P_EQUAL))
        return NULL;

    if (!expect_type(args.objs[0], 1, OBJ_STRING))
        return NULL;

    if (!expect_type(args.objs[1], 1, OBJ_STRING))
        return NULL;

    const char* filename = args.objs[0]->string;
    const char* contents = args.objs[1]->string;
    size_t contents_len = strlen(contents);

    FILE* f = fopen(filename, "wb");
    fwrite(contents, contents_len, sizeof(char), f);
    fclose(f);

    return kokos_bool_to_obj(true);
}

static kokos_obj_t* builtin_macroexpand_1(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 1, args.len, P_EQUAL))
        return NULL;

    kokos_obj_t* list_obj = args.objs[0];
    if (!(list_obj->quoted && list_obj->type == OBJ_LIST))
        return kokos_interp_eval(interp, list_obj, 0);

    kokos_obj_list_t list = list_obj->list;
    if (list.len == 0 || list.objs[0]->type != OBJ_SYMBOL)
        return kokos_interp_eval(interp, list_obj, 0);

    kokos_env_pair_t* symbol_pair = kokos_env_find(interp->current_env, list.objs[0]->symbol);
    if (!symbol_pair || symbol_pair->value->type != OBJ_MACRO)
        return kokos_interp_eval(interp, list_obj, 0);

    kokos_obj_macro_t macro = symbol_pair->value->macro;
    kokos_obj_list_t macro_args = list_to_args(list);
    return call_macro(interp, macro, macro_args);
}

static kokos_obj_t* sform_def(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 2, args.len, P_EQUAL))
        return NULL;

    if (!expect_type(args.objs[0], 1, OBJ_SYMBOL))
        return NULL;

    for (size_t i = 1; i < args.len - 1; i++) {
        if (!kokos_interp_eval(interp, args.objs[i], 1))
            return NULL;
    }

    kokos_obj_t* def = kokos_interp_eval(interp, args.objs[args.len - 1], 0);
    if (!def)
        return NULL;

    kokos_env_add(&interp->global_env, args.objs[0]->symbol, def);
    return &kokos_obj_nil;
}

static inline kokos_params_t make_params(kokos_obj_list_t params)
{
    kokos_params_t result = { .var = false, .len = params.len };
    char** names = malloc(sizeof(char*) * params.len);
    for (size_t i = 0; i < params.len; i++) {
        if (!expect_type(params.objs[i], 1, OBJ_SYMBOL)) {
            free(names);
            result.names = NULL;
            return result;
        }

        char* cur = params.objs[i]->symbol;
        if (strcmp(cur, "&") == 0) {
            if (i == params.len - 1) {
                write_err(
                    params.objs[i]->token.location, "expected a name for the rest parameter list");
                result.names = NULL;
                return result;
            }

            names[i] = strdup(params.objs[i + 1]->symbol);
            result.var = true;
            result.len -= 1;
            break;
        }

        names[i] = strdup(params.objs[i]->symbol);
    }

    result.names = names;
    return result;
}

static inline kokos_obj_t* make_lambda(
    kokos_interp_t* interp, kokos_obj_list_t params, kokos_obj_list_t body)
{
    kokos_obj_t* result = kokos_gc_alloc(&interp->gc);
    result->type = OBJ_PROCEDURE;

    kokos_params_t proc_params = make_params(params);
    if (!proc_params.names)
        return NULL;

    result->procedure.params = proc_params;
    result->procedure.body = body;

    return result;
}

static kokos_obj_t* sform_proc(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 3, args.len, P_AT_LEAST))
        return NULL;

    if (!expect_type(args.objs[0], 1, OBJ_SYMBOL))
        return NULL;

    kokos_obj_t* params = args.objs[1];
    if (!expect_type(params, 1, OBJ_LIST))
        return NULL;

    kokos_obj_list_t body = { .objs = args.objs + 2, .len = args.len - 2 };

    // to prevent the garbage collector from collecting params and
    // the body of a procedure we actually need to duplicate everything
    // since if we tried to slice those lists the object from which we would've sliced
    // would become unreachable and thus would get collected leaving us with pointers to freed
    // memory
    body = kokos_list_dup(&interp->gc, body);

    kokos_obj_t* result = make_lambda(interp, params->list, body);
    if (result)
        kokos_env_add(interp->current_env, args.objs[0]->symbol, result);
    return result;
}

static kokos_obj_t* sform_fn(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 2, args.len, P_AT_LEAST))
        return NULL;

    kokos_obj_t* params = args.objs[0];
    if (!expect_type(params, 1, OBJ_LIST))
        return NULL;

    kokos_obj_list_t body = { .objs = args.objs + 1, .len = args.len - 1 };
    body = kokos_list_dup(&interp->gc, body);
    return make_lambda(interp, params->list, body);
}

static kokos_obj_t* sform_macro(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 3, args.len, P_AT_LEAST))
        return NULL;

    kokos_obj_t* name = args.objs[0];
    if (!expect_type(name, 1, OBJ_SYMBOL))
        return NULL;

    kokos_obj_t* params = args.objs[1];
    if (!expect_type(params, 1, OBJ_LIST))
        return NULL;

    kokos_params_t macro_params = make_params(params->list);
    if (!macro_params.names)
        return NULL;

    kokos_obj_list_t body = { .len = args.len - 2, .objs = args.objs + 2 };
    body = kokos_list_dup(
        &interp->gc, body); // see 'builtin_proc' for explanation why this is necessary

    kokos_obj_t* macro = kokos_gc_alloc(&interp->gc);
    macro->type = OBJ_MACRO;
    macro->macro.params = macro_params;
    macro->macro.body = body;

    kokos_env_add(interp->current_env, name->symbol, macro);
    return macro;
}

static kokos_obj_t* sform_if(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 2, args.len, P_AT_LEAST))
        return NULL;

    if (args.len > 3) {
        write_err(
            called_from, "Too many arguments: expected 2 or 3, but got %lu instead", args.len);
        return NULL;
    }

    kokos_obj_t* cond = kokos_interp_eval(interp, args.objs[0], 0);
    if (kokos_obj_to_bool(cond))
        return kokos_interp_eval(interp, args.objs[1], 0);

    if (args.len > 2)
        return kokos_interp_eval(interp, args.objs[2], 0);

    return &kokos_obj_nil;
}

static kokos_obj_t* sform_let(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 1, args.len, P_AT_LEAST))
        return NULL;

    if (!expect_type(args.objs[0], 1, OBJ_LIST))
        return NULL;

    kokos_obj_list_t binding_pairs = args.objs[0]->list;
    if (binding_pairs.len % 2 != 0) {
        write_err(args.objs[0]->token.location, "expected an even number of arguments");
        return NULL;
    }

    kokos_env_t env = kokos_env_empty(binding_pairs.len / 2);
    for (size_t i = 0; i < binding_pairs.len; i += 2) {
        kokos_obj_t* name = binding_pairs.objs[i];
        if (!expect_type(name, 1, OBJ_SYMBOL)) {
            kokos_env_destroy(&env);
            return NULL;
        }

        kokos_obj_t* value = kokos_interp_eval(interp, binding_pairs.objs[i + 1], 0);
        if (!value) {
            kokos_env_destroy(&env);
            return NULL;
        }

        kokos_env_add(&env, name->symbol, value);
    }

    push_env(interp, &env);
    kokos_obj_t* result = &kokos_obj_nil;
    for (size_t i = 1; i < args.len; i++) {
        result = kokos_interp_eval(interp, args.objs[i], 0);
        if (!result)
            return NULL;
    }

    pop_env(interp);
    kokos_env_destroy(&env);
    return result;
}

static kokos_obj_t* make_builtin(kokos_interp_t* interp, kokos_builtin_procedure_t proc)
{
    kokos_obj_t* obj = kokos_gc_alloc(&interp->gc);
    obj->type = OBJ_BUILTIN_PROC;
    obj->builtin = proc;
    return obj;
}

static kokos_obj_t* make_special_form(kokos_interp_t* interp, kokos_builtin_procedure_t sform)
{
    kokos_obj_t* obj = kokos_gc_alloc(&interp->gc);
    obj->type = OBJ_SPECIAL_FORM;
    obj->builtin = sform;
    return obj;
}

static kokos_env_t default_env(kokos_interp_t* interp)
{
    kokos_env_t env = kokos_env_empty(0);

    // builtins
    kokos_obj_t* plus = make_builtin(interp, builtin_plus);
    kokos_env_add(&env, "+", plus);

    kokos_obj_t* minus = make_builtin(interp, builtin_minus);
    kokos_env_add(&env, "-", minus);

    kokos_obj_t* star = make_builtin(interp, builtin_star);
    kokos_env_add(&env, "*", star);

    kokos_obj_t* slash = make_builtin(interp, builtin_slash);
    kokos_env_add(&env, "/", slash);

    kokos_obj_t* eq = make_builtin(interp, builtin_eq);
    kokos_env_add(&env, "=", eq);

    kokos_obj_t* lt = make_builtin(interp, builtin_lt);
    kokos_env_add(&env, "<", lt);

    kokos_obj_t* gt = make_builtin(interp, builtin_gt);
    kokos_env_add(&env, ">", gt);

    kokos_obj_t* lte = make_builtin(interp, builtin_lte);
    kokos_env_add(&env, "<=", lte);

    kokos_obj_t* gte = make_builtin(interp, builtin_gte);
    kokos_env_add(&env, ">=", gte);

    kokos_obj_t* not = make_builtin(interp, builtin_not);
    kokos_env_add(&env, "not", not );

    kokos_obj_t* print = make_builtin(interp, builtin_print);
    kokos_env_add(&env, "print", print);

    kokos_obj_t* type = make_builtin(interp, builtin_type);
    kokos_env_add(&env, "type", type);

    kokos_obj_t* list = make_builtin(interp, builtin_list);
    kokos_env_add(&env, "list", list);

    kokos_obj_t* make_vec = make_builtin(interp, builtin_make_vec);
    kokos_env_add(&env, "make-vec", make_vec);

    kokos_obj_t* push = make_builtin(interp, builtin_push);
    kokos_env_add(&env, "push", push);

    kokos_obj_t* nth = make_builtin(interp, builtin_nth);
    kokos_env_add(&env, "nth", nth);

    kokos_obj_t* make_map = make_builtin(interp, builtin_make_map);
    kokos_env_add(&env, "make-map", make_map);

    kokos_obj_t* add_map = make_builtin(interp, builtin_add_map);
    kokos_env_add(&env, "add-map", add_map);

    kokos_obj_t* find_map = make_builtin(interp, builtin_find_map);
    kokos_env_add(&env, "find-map", find_map);

    kokos_obj_t* map = make_builtin(interp, builtin_map);
    kokos_env_add(&env, "map", map);

    kokos_obj_t* read_file = make_builtin(interp, builtin_read_file);
    kokos_env_add(&env, "read-file", read_file);

    kokos_obj_t* write_file = make_builtin(interp, builtin_write_file);
    kokos_env_add(&env, "write-file", write_file);

    kokos_obj_t* macroexpand_1 = make_builtin(interp, builtin_macroexpand_1);
    kokos_env_add(&env, "macroexpand-1", macroexpand_1);

    // special forms
    kokos_obj_t* def = make_special_form(interp, sform_def);
    kokos_env_add(&env, "def", def);

    kokos_obj_t* proc = make_special_form(interp, sform_proc);
    kokos_env_add(&env, "proc", proc);

    kokos_obj_t* macro = make_special_form(interp, sform_macro);
    kokos_env_add(&env, "macro", macro);

    kokos_obj_t* fn = make_special_form(interp, sform_fn);
    kokos_env_add(&env, "fn", fn);

    kokos_obj_t* if_ = make_special_form(interp, sform_if);
    kokos_env_add(&env, "if", if_);

    kokos_obj_t* let = make_special_form(interp, sform_let);
    kokos_env_add(&env, "let", let);

    kokos_obj_t* or = make_special_form(interp, sform_or);
    kokos_env_add(&env, "or", or);

    kokos_obj_t*and = make_special_form(interp, sform_and);
    kokos_env_add(&env, "and", and);

    return env;
}

kokos_interp_t* kokos_interp_new(size_t gc_threshold)
{
    kokos_interp_t* interp = malloc(sizeof(kokos_interp_t));
    *interp = (kokos_interp_t) { .gc
        = { .obj_threshold = gc_threshold, .obj_count = 0, .root = NULL } };
    interp->global_env = default_env(interp);
    interp->current_env = &interp->global_env;
    return interp;
}

void kokos_interp_print_stat(const kokos_interp_t* interp)
{
    printf("ALLOCATED OBJECTS: %lu\n", interp->gc.obj_count);
    printf("GC THRESHOLD: %lu\n", interp->gc.obj_threshold);
}

void kokos_interp_destroy(kokos_interp_t* interp)
{
    kokos_obj_t* cur = interp->gc.root;
    while (cur) {
        kokos_obj_t* obj = cur;
        cur = cur->next;
        kokos_obj_free(obj);
    }

    kokos_env_destroy(&interp->global_env);
    free(interp);
}
