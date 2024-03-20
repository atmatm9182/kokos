#ifndef OBJ_H_
#define OBJ_H_

#include "token.h"

#include <stddef.h>
#include <stdint.h>

enum kokos_obj_type {
    OBJ_NIL,
    OBJ_INT,
    OBJ_STRING,
    OBJ_FLOAT,
    OBJ_BOOL,
    OBJ_SYMBOL,
    OBJ_LIST,
    OBJ_VEC,
    OBJ_MAP,
    OBJ_PROCEDURE,
    OBJ_BUILTIN_PROC,
    OBJ_SPECIAL_FORM,
    OBJ_MACRO,
};

typedef enum kokos_obj_type kokos_obj_type_e;

struct kokos_obj;
typedef struct kokos_obj kokos_obj_t;

struct kokos_obj_list {
    kokos_obj_t** objs;
    size_t len;
};

typedef struct kokos_obj_list kokos_obj_list_t;

struct kokos_obj_vec {
    kokos_obj_t** items;
    size_t len;
    size_t cap;
};

typedef struct kokos_obj_vec kokos_obj_vec_t;

struct kokos_params {
    bool var;
    size_t len;
    char** names;
};

typedef struct kokos_params kokos_params_t;

struct kokos_obj_procedure {
    kokos_params_t params;
    kokos_obj_list_t body;
};

typedef struct kokos_obj_procedure kokos_obj_procedure_t;
typedef struct kokos_obj_procedure kokos_obj_macro_t;

struct kokos_interp;
struct kokos_gc;

typedef kokos_obj_t* (*kokos_builtin_procedure_t)(
    struct kokos_interp* interp, kokos_obj_list_t args, kokos_location_t called_from);

typedef hash_table kokos_obj_map_t;

typedef struct kokos_obj kokos_obj_t;

struct kokos_obj {
    kokos_token_t token;
    struct kokos_obj* next;
    unsigned char marked : 1;
    unsigned char quoted : 1;
    kokos_obj_type_e type : 6;

    union {
        int64_t integer;
        double floating;
        char* string;
        char* symbol;
        kokos_obj_list_t list;
        kokos_obj_vec_t vec;
        kokos_obj_map_t map;
        kokos_builtin_procedure_t builtin;
        kokos_obj_procedure_t procedure;
        kokos_obj_macro_t macro;
    };
};

extern kokos_obj_t kokos_obj_nil;
extern kokos_obj_t kokos_obj_false;
extern kokos_obj_t kokos_obj_true;

void kokos_obj_mark(kokos_obj_t* obj);
void kokos_obj_free(kokos_obj_t* obj);
void kokos_obj_print(kokos_obj_t* obj);

kokos_obj_list_t kokos_list_dup(struct kokos_gc* gc, kokos_obj_list_t list);
kokos_obj_t* kokos_obj_dup(struct kokos_gc* gc, kokos_obj_t* obj);

bool kokos_obj_eq(const kokos_obj_t* left, const kokos_obj_t* right);

bool kokos_obj_to_bool(const kokos_obj_t* obj);
kokos_obj_t* kokos_bool_to_obj(bool b);

#endif // OBJ_H_
