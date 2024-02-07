#include "interpreter.h"

#include <assert.h>
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
    case OBJ_SYMBOL:       return "symbol";
    case OBJ_BUILTIN_PROC: return "builtin procedure";
    case OBJ_PROCEDURE:    return "procedure";
    case OBJ_LIST:         return "list";
    case OBJ_SPECIAL_FORM: return "special form";
    case OBJ_STRING:       return "string";
    }
}

static void type_mismatch(
    kokos_location_t where, kokos_obj_type_e got, int expected_count, va_list args)
{
    string_builder sb = sb_new(25);

    for (int i = 0; i < expected_count; i++) {
        kokos_obj_type_e t = va_arg(args, kokos_obj_type_e);
        sb_push_cstr(&sb, str_type(t));
        if (i != expected_count - 1)
            sb_push_cstr(&sb, " ");
    }

    char tmp_buf[512];
    char* expected_str = sb_to_cstr(&sb);
    if (expected_count > 1)
        sprintf(tmp_buf, "one of %s", expected_str);
    else
        sprintf(tmp_buf, "%s", expected_str);

    write_err(where, "Type mismatch: got %s, expected %s", str_type(got), tmp_buf);
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

    type_mismatch(obj->token.location, obj->type, expected_count, args);
    va_end(args);
    return false;
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

kokos_obj_t* kokos_interp_eval(kokos_interp_t* interp, kokos_obj_t* obj, bool top_level)
{
    kokos_obj_t* result = NULL;

    switch (obj->type) {
    case OBJ_INT:
    case OBJ_STRING:
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

            kokos_builtin_procedure_t func = head->builtin;
            result = func(interp, args);
            break;
        }

        if (head->type == OBJ_SPECIAL_FORM) {
            kokos_obj_list_t args = list_to_args(obj->list);
            kokos_builtin_procedure_t proc = head->builtin;
            result = proc(interp, args);
            break;
        }

        if (head->type == OBJ_PROCEDURE) {
            kokos_obj_procedure_t proc = head->procedure;

            kokos_obj_list_t args = list_to_args(obj->list);
            if (args.len != proc.params.len) {
                write_err(obj->token.location,
                    "Arity mismatch when calling: expected %lu arguments, got %lu", proc.params.len,
                    args.len);
                return NULL;
            }

            kokos_env_t call_env = kokos_env_empty(args.len);
            for (size_t i = 0; i < args.len; i++) {
                kokos_obj_t* obj = kokos_interp_eval(interp, args.objs[i], 0);
                if (!obj) {
                    return NULL;
                }

                kokos_env_add(&call_env, proc.params.objs[i]->symbol, obj);
            }

            kokos_env_t* call_parent = interp->current_env;
            call_env.parent = call_parent;
            interp->current_env = &call_env;

            for (size_t i = 0; i < proc.body.len - 1; i++) {
                if (!kokos_interp_eval(interp, proc.body.objs[i], 0))
                    return NULL;
            }

            result = kokos_interp_eval(interp, proc.body.objs[proc.body.len - 1], 0);
            interp->current_env = call_parent;
            break;
        }

        write_err(obj->token.location, "Object of type '%s' is not callable", str_type(obj->type));
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

static kokos_obj_t* builtin_plus(kokos_interp_t* interp, kokos_obj_list_t args)
{
    int64_t num = 0;
    for (size_t i = 0; i < args.len; i++) {
        if (!expect_type(args.objs[i], 1, OBJ_INT))
            return NULL;

        num += args.objs[i]->integer;
    }
    kokos_obj_t* obj = kokos_interp_alloc(interp);
    obj->type = OBJ_INT;
    obj->integer = num;
    return obj;
}

static kokos_obj_t* builtin_minus(kokos_interp_t* interp, kokos_obj_list_t args)
{
    int64_t num = args.objs[0]->integer;
    kokos_obj_t* obj = kokos_interp_alloc(interp);
    obj->type = OBJ_INT;

    if (args.len == 1) {
        obj->integer = -num;
        return obj;
    }

    for (size_t i = 1; i < args.len; i++) {
        if (!expect_type(args.objs[i], 1, OBJ_INT))
            return NULL;

        num -= args.objs[i]->integer;
    }
    obj->integer = num;
    return obj;
}

static kokos_obj_t* builtin_star(kokos_interp_t* interp, kokos_obj_list_t args)
{
    int64_t num = args.objs[0]->integer;
    for (size_t i = 1; i < args.len; i++) {
        if (!expect_type(args.objs[i], 1, OBJ_INT))
            return NULL;

        num *= args.objs[i]->integer;
    }
    kokos_obj_t* obj = kokos_interp_alloc(interp);
    obj->type = OBJ_INT;
    obj->integer = num;
    return obj;
}

static kokos_obj_t* builtin_slash(kokos_interp_t* interp, kokos_obj_list_t args)
{
    int64_t num = args.objs[0]->integer;
    for (size_t i = 1; i < args.len; i++) {
        if (!expect_type(args.objs[i], 1, OBJ_INT))
            return NULL;

        num /= args.objs[i]->integer;
    }
    kokos_obj_t* obj = kokos_interp_alloc(interp);
    obj->type = OBJ_INT;
    obj->integer = num;
    return obj;
}

static kokos_obj_t* sform_def(kokos_interp_t* interp, kokos_obj_list_t args)
{
    if (!expect_type(args.objs[0], 1, OBJ_SYMBOL))
        return NULL;

    for (size_t i = 1; i < args.len - 1; i++) {
        if (!kokos_interp_eval(interp, args.objs[i], 1))
            return NULL;
    }

    kokos_obj_t* def = kokos_interp_eval(interp, args.objs[args.len - 1], 1);
    if (!def)
        return NULL;

    kokos_env_add(&interp->global_env, args.objs[0]->symbol, def);
    return &kokos_obj_nil;
}

static kokos_obj_t* sform_proc(kokos_interp_t* interp, kokos_obj_list_t args)
{
    if (!expect_type(args.objs[0], 1, OBJ_SYMBOL))
        return NULL;

    kokos_obj_t* params = args.objs[1];
    if (!expect_type(params, 1, OBJ_LIST))
        return NULL;

    kokos_obj_list_t body = { .objs = args.objs + 2, .len = args.len - 2 };
    kokos_obj_procedure_t proc = { .params = params->list, .body = body };

    kokos_obj_t* result = kokos_interp_alloc(interp);
    result->type = OBJ_PROCEDURE;
    result->procedure = proc;

    kokos_env_add(interp->current_env, args.objs[0]->symbol, result);
    return result;
}

static kokos_obj_t* make_builtin(kokos_interp_t* interp, kokos_builtin_procedure_t func)
{
    kokos_obj_t* obj = kokos_interp_alloc(interp);
    obj->type = OBJ_BUILTIN_PROC;
    obj->builtin = func;
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

    // special forms
    kokos_obj_t* def = make_special_form(interp, sform_def);
    kokos_env_add(&env, "def", def);

    kokos_obj_t* proc = make_special_form(interp, sform_proc);
    kokos_env_add(&env, "proc", proc);

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

static inline void sweep(kokos_interp_t* interp)
{
    kokos_obj_t** cur = &interp->obj_head;
    while (*cur) {
        if (!(*cur)->marked) {
            kokos_obj_t* obj = *cur;
            *cur = (*cur)->next;
            free(obj);
            interp->obj_count--;
        } else {
            (*cur)->marked = 0;
            cur = &(*cur)->next;
        }
    }
}

// FIXME: there is a bug that occurs really rarely that causes a double-free
// to happen, howevew i do not even know what causes that bug yet
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
