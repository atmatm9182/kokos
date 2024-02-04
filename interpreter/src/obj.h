#ifndef OBJ_H_
#define OBJ_H_

#include <stddef.h>
#include <stdint.h>

enum kokos_obj_type {
    OBJ_INT,
    OBJ_STRING,
    OBJ_SYMBOL,
    OBJ_LIST,
    OBJ_BUILTIN_FUNC,
};

typedef enum kokos_obj_type kokos_obj_type_e;

struct kokos_obj;
typedef struct kokos_obj kokos_obj_t;

struct kokos_obj_list {
    kokos_obj_t** objs;
    size_t len;
};

typedef struct kokos_obj_list kokos_obj_list_t;

struct kokos_interp;

typedef kokos_obj_t* (*kokos_builtin_func_t)(struct kokos_interp* interp, kokos_obj_list_t args);

struct kokos_obj {
    struct kokos_obj* next;
    unsigned char marked;
    kokos_obj_type_e type;

    union {
        int64_t integer;
        char* string;
        char* symbol;
        kokos_obj_list_t list;
        kokos_builtin_func_t builtin_func;
    };
};

void kokos_obj_mark(kokos_obj_t* obj);

#endif // OBJ_H_
