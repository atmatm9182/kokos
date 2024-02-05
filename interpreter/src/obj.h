#ifndef OBJ_H_
#define OBJ_H_

#include <stddef.h>
#include <stdint.h>

enum kokos_obj_type {
    OBJ_INT,
    OBJ_STRING,
    OBJ_SYMBOL,
    OBJ_LIST,
    OBJ_PROCEDURE,
    OBJ_BUILTIN_PROC,
    OBJ_SPECIAL_FORM,
};

typedef enum kokos_obj_type kokos_obj_type_e;

struct kokos_obj;
typedef struct kokos_obj kokos_obj_t;

struct kokos_obj_list {
    kokos_obj_t** objs;
    size_t len;
};

typedef struct kokos_obj_list kokos_obj_list_t;

struct kokos_obj_procedure {
    kokos_obj_list_t params;
    kokos_obj_list_t body;
};

typedef struct kokos_obj_procedure kokos_obj_procedure_t;

struct kokos_interp;

typedef kokos_obj_t* (*kokos_builtin_procedure_t)(
    struct kokos_interp* interp, kokos_obj_list_t args);

struct kokos_obj {
    struct kokos_obj* next;
    unsigned char marked : 1;
    kokos_obj_type_e type : 7;

    union {
        int64_t integer;
        char* string;
        char* symbol;
        kokos_obj_list_t list;
        kokos_builtin_procedure_t builtin;
        kokos_obj_procedure_t procedure;
    };
};

extern kokos_obj_t kokos_obj_nil;

void kokos_obj_mark(kokos_obj_t* obj);

#endif // OBJ_H_
