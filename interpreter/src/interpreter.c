#include "interpreter.h"
#include "location.h"
#include "src/env.h"
#include "src/obj.h"

#include <assert.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define ERR_BUFFER_CAP 2048

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
    case OBJ_INT:          return "int";
    case OBJ_FLOAT:        return "float";
    case OBJ_STRING:       return "string";
    case OBJ_BOOL:         return "bool";
    case OBJ_SYMBOL:       return "symbol";
    case OBJ_BUILTIN_PROC: return "builtin procedure";
    case OBJ_PROCEDURE:    return "procedure";
    case OBJ_LIST:         return "list";
    case OBJ_SPECIAL_FORM: return "special form";
    default:               assert(0 && "unreachable!");
    }
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

    for (int i = 0; i < expected_count; i++) {
        if (obj->type == va_arg(args, kokos_obj_type_e)) {
            va_end(args);
            return true;
        }
    }

    type_mismatch_va(obj->token.location, obj->type, expected_count, args);
    va_end(args);
    return false;
}

typedef enum {
    P_EQ,
    P_AT_LEAST,
} arity_predicate_e;

static bool expect_arity(kokos_location_t location, int expected, int got, arity_predicate_e pred)
{
    switch (pred) {
    case P_EQ:
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

kokos_obj_t* kokos_interp_alloc(kokos_interp_t* interp)
{
    kokos_obj_t* obj = malloc(sizeof(kokos_obj_t));
    obj->marked = 0;
    obj->next = interp->obj_head;
    interp->obj_head = obj;
    interp->obj_count++;
    return obj;
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

kokos_obj_t* kokos_interp_eval(kokos_interp_t* interp, kokos_obj_t* obj, bool top_level)
{
    kokos_obj_t* result = NULL;

    switch (obj->type) {
    case OBJ_INT:
    case OBJ_STRING:
    case OBJ_FLOAT:
    case OBJ_BOOL:
    case OBJ_BUILTIN_PROC:
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
        kokos_obj_t* head = kokos_interp_eval(interp, obj->list.objs[0], 0);
        if (!head)
            return NULL;
        if (head->type == OBJ_BUILTIN_PROC) {
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

        if (head->type == OBJ_SPECIAL_FORM) {
            kokos_obj_list_t args = list_to_args(obj->list);

            kokos_builtin_procedure_t proc = head->builtin;
            result = proc(interp, args, head->token.location);
            break;
        }

        if (head->type == OBJ_PROCEDURE) {
            kokos_obj_procedure_t proc = head->procedure;

            kokos_obj_list_t args = list_to_args(obj->list);
            if (!expect_arity(obj->token.location, proc.params.len, args.len, P_EQ))
                return NULL;

            kokos_env_t call_env = kokos_env_empty(args.len);
            for (size_t i = 0; i < proc.params.len; i++) {
                kokos_obj_t* obj = kokos_interp_eval(interp, args.objs[i], 0);
                if (!obj)
                    return NULL;

                kokos_env_add(&call_env, proc.params.objs[i]->symbol, obj);
            }

            push_env(interp, &call_env);
            for (size_t i = 0; i < proc.body.len - 1; i++) {
                if (!kokos_interp_eval(interp, proc.body.objs[i], 0))
                    return NULL;
            }

            result = kokos_interp_eval(interp, proc.body.objs[proc.body.len - 1], 0);

            pop_env(interp);
            kokos_env_destroy(&call_env);
            break;
        }

        write_err(obj->token.location, "Object of type '%s' is not callable", str_type(head->type));
        return NULL;
    }
    case OBJ_SPECIAL_FORM: assert(0 && "unreachable!"); break;
    }

    if (top_level && result && interp->obj_count > interp->gc_threshold) {
        kokos_obj_mark(result);
        kokos_gc_run(interp);
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

    kokos_obj_t* obj = kokos_interp_alloc(interp);
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
    if (args.len == 0)
        return 0;

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

    kokos_obj_t* obj = kokos_interp_alloc(interp);
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

    kokos_obj_t* obj = kokos_interp_alloc(interp);
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
    kokos_obj_t* obj = kokos_interp_alloc(interp);
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

    kokos_obj_t* obj = kokos_interp_alloc(interp);
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
    if (!expect_arity(called_from, 1, args.len, P_EQ))
        return NULL;

    kokos_obj_t* res = kokos_interp_alloc(interp);
    res->type = OBJ_STRING;
    res->string = strdup(str_type(args.objs[0]->type));
    return res;
}

static bool obj_to_bool(kokos_obj_t* obj)
{
    return obj != &kokos_obj_false && obj != &kokos_obj_nil;
}

static kokos_obj_t* bool_to_obj(bool b)
{
    return b ? &kokos_obj_true : &kokos_obj_false;
}

// TODO: allow comparing integers and floats
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

    if (left->type != right->type)
        return &kokos_obj_false;

    switch (left->type) {
    case OBJ_FLOAT:     return bool_to_obj(left->floating == right->floating);
    case OBJ_INT:       return bool_to_obj(left->integer == right->integer);
    case OBJ_BOOL:      return bool_to_obj(obj_to_bool(left) == obj_to_bool(right));
    case OBJ_PROCEDURE: return bool_to_obj(left == right);
    case OBJ_LIST:      {
        if (left->list.len != right->list.len)
            return bool_to_obj(false);

        kokos_obj_t* tmp_arr[2];
        kokos_obj_list_t args = { .objs = tmp_arr, .len = 2 };

        for (size_t i = 0; i < left->list.len; i++) {
            args.objs[0] = left->list.objs[i];
            args.objs[1] = right->list.objs[i];
            if (!builtin_eq(interp, args, called_from))
                return bool_to_obj(false);
        }

        return bool_to_obj(true);
    }
    case OBJ_STRING:       return bool_to_obj(strcmp(left->string, right->string) == 0);
    case OBJ_SYMBOL:       assert(0 && "unreachable!");
    case OBJ_SPECIAL_FORM:
    case OBJ_BUILTIN_PROC: return bool_to_obj(left->builtin == right->builtin);
    default:               assert(0 && "unreachable!");
    }
}

static kokos_obj_t* builtin_lt(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
    if (!expect_arity(called_from, 2, args.len, P_EQ))
        return NULL;

    kokos_obj_t* left = args.objs[0];
    kokos_obj_t* right = args.objs[1];

    switch (left->type) {
    case OBJ_INT:
        switch (right->type) {
        case OBJ_INT:   return bool_to_obj(left->integer < right->integer);
        case OBJ_FLOAT: return bool_to_obj((double)left->integer < right->floating);
        default:
            type_mismatch(right->token.location, right->type, 2, OBJ_INT, OBJ_FLOAT);
            return NULL;
        }
    case OBJ_FLOAT:
        switch (right->type) {
        case OBJ_INT:   return bool_to_obj(left->floating < (double)right->integer);
        case OBJ_FLOAT: return bool_to_obj(left->floating < right->floating);
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
    if (!expect_arity(called_from, 2, args.len, P_EQ))
        return NULL;

    kokos_obj_t* left = args.objs[0];
    kokos_obj_t* right = args.objs[1];

    switch (left->type) {
    case OBJ_INT:
        switch (right->type) {
        case OBJ_INT:   return bool_to_obj(left->integer > right->integer);
        case OBJ_FLOAT: return bool_to_obj((double)left->integer > right->floating);
        default:
            type_mismatch(right->token.location, right->type, 2, OBJ_INT, OBJ_FLOAT);
            return NULL;
        }
    case OBJ_FLOAT:
        switch (right->type) {
        case OBJ_INT:   return bool_to_obj(left->floating > (double)right->integer);
        case OBJ_FLOAT: return bool_to_obj(left->floating > right->floating);
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
    if (!expect_arity(called_from, 2, args.len, P_EQ))
        return NULL;

    kokos_obj_t* left = args.objs[0];
    kokos_obj_t* right = args.objs[1];

    switch (left->type) {
    case OBJ_INT:
        switch (right->type) {
        case OBJ_INT:   return bool_to_obj(left->integer <= right->integer);
        case OBJ_FLOAT: return bool_to_obj((double)left->integer <= right->floating);
        default:
            type_mismatch(right->token.location, right->type, 2, OBJ_INT, OBJ_FLOAT);
            return NULL;
        }
    case OBJ_FLOAT:
        switch (right->type) {
        case OBJ_INT:   return bool_to_obj(left->floating <= (double)right->integer);
        case OBJ_FLOAT: return bool_to_obj(left->floating <= right->floating);
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
    if (!expect_arity(called_from, 2, args.len, P_EQ))
        return NULL;

    kokos_obj_t* left = args.objs[0];
    kokos_obj_t* right = args.objs[1];

    switch (left->type) {
    case OBJ_INT:
        switch (right->type) {
        case OBJ_INT:   return bool_to_obj(left->integer >= right->integer);
        case OBJ_FLOAT: return bool_to_obj((double)left->integer >= right->floating);
        default:
            type_mismatch(right->token.location, right->type, 2, OBJ_INT, OBJ_FLOAT);
            return NULL;
        }
    case OBJ_FLOAT:
        switch (right->type) {
        case OBJ_INT:   return bool_to_obj(left->floating >= (double)right->integer);
        case OBJ_FLOAT: return bool_to_obj(left->floating >= right->floating);
        default:
            type_mismatch(right->token.location, right->type, 2, OBJ_INT, OBJ_FLOAT);
            return NULL;
        }
    default: break;
    }

    type_mismatch(left->token.location, left->type, 2, OBJ_INT, OBJ_FLOAT);
    return NULL;
}

static kokos_obj_t* sform_def(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
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

static kokos_obj_t* sform_proc(
    kokos_interp_t* interp, kokos_obj_list_t args, kokos_location_t called_from)
{
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
    kokos_obj_list_t params_list = kokos_list_dup(interp, params->list);
    body = kokos_list_dup(interp, body);

    kokos_obj_procedure_t proc = { .params = params_list, .body = body };

    kokos_obj_t* result = kokos_interp_alloc(interp);
    result->type = OBJ_PROCEDURE;
    result->procedure = proc;

    kokos_env_add(interp->current_env, args.objs[0]->symbol, result);
    return result;
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
    if (obj_to_bool(cond)) {
        return kokos_interp_eval(interp, args.objs[1], 0);
    }

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
    kokos_obj_t* obj = kokos_interp_alloc(interp);
    obj->type = OBJ_BUILTIN_PROC;
    obj->builtin = proc;
    return obj;
}

static kokos_obj_t* make_special_form(kokos_interp_t* interp, kokos_builtin_procedure_t sform)
{
    kokos_obj_t* obj = kokos_interp_alloc(interp);
    obj->type = OBJ_SPECIAL_FORM;
    obj->builtin = sform;
    return obj;
}

static kokos_env_t default_env(kokos_interp_t* interp)
{
    kokos_env_t env = kokos_env_empty(10);

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

    kokos_obj_t* print = make_builtin(interp, builtin_print);
    kokos_env_add(&env, "print", print);

    kokos_obj_t* type = make_builtin(interp, builtin_type);
    kokos_env_add(&env, "type", type);

    // special forms
    kokos_obj_t* def = make_special_form(interp, sform_def);
    kokos_env_add(&env, "def", def);

    kokos_obj_t* proc = make_special_form(interp, sform_proc);
    kokos_env_add(&env, "proc", proc);

    kokos_obj_t* if_ = make_special_form(interp, sform_if);
    kokos_env_add(&env, "if", if_);

    kokos_obj_t* let = make_special_form(interp, sform_let);
    kokos_env_add(&env, "let", let);

    return env;
}

kokos_interp_t* kokos_interp_new(size_t gc_threshold)
{
    kokos_interp_t* interp = malloc(sizeof(kokos_interp_t));
    *interp = (kokos_interp_t) { .gc_threshold = gc_threshold, .obj_count = 0, .obj_head = NULL };
    interp->global_env = default_env(interp);
    interp->current_env = &interp->global_env;
    return interp;
}

static inline void mark_all(kokos_env_t* env)
{
    if (env == NULL)
        return;

    for (size_t i = 0; i < env->len; i++) {
        kokos_obj_mark(env->items[i].value);
    }

    mark_all(env->parent);
}

static inline void obj_free(kokos_obj_t* obj)
{
    switch (obj->type) {
    case OBJ_INT:
    case OBJ_FLOAT:
    case OBJ_BOOL:
    case OBJ_BUILTIN_PROC:
    case OBJ_SPECIAL_FORM: break;
    case OBJ_LIST:         free(obj->list.objs); break;
    case OBJ_STRING:       free(obj->string); break;
    case OBJ_SYMBOL:       free(obj->symbol); break;
    case OBJ_PROCEDURE:
        free(obj->procedure.params.objs);
        free(obj->procedure.body.objs);
        break;
    }
    free(obj);
}

static inline void sweep(kokos_interp_t* interp)
{
    kokos_obj_t** cur = &interp->obj_head;
    while (*cur) {
        if (!(*cur)->marked) {
            kokos_obj_t* obj = *cur;
            *cur = (*cur)->next;
            obj_free(obj);
            interp->obj_count--;
        } else {
            (*cur)->marked = 0;
            cur = &(*cur)->next;
        }
    }
}

void kokos_gc_run(kokos_interp_t* interp)
{
    mark_all(interp->current_env);
    sweep(interp);
}

void kokos_interp_print_stat(const kokos_interp_t* interp)
{
    printf("ALLOCATED OBJECTS: %lu\n", interp->obj_count);
    printf("GC THRESHOLD: %lu\n", interp->gc_threshold);
}

void kokos_interp_destroy(kokos_interp_t* interp)
{
    kokos_obj_t* cur = interp->obj_head;
    while (cur) {
        kokos_obj_t* obj = cur;
        cur = cur->next;
        printf("freeing object at addr %p\n", (void*)obj);
        obj_free(obj);
    }

    kokos_env_destroy(&interp->global_env);
    free(interp);
}
