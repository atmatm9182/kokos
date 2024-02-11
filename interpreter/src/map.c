#include "src/map.h"
#include "base.h"
#include "obj.h"

#include <stdint.h>

// TODO: replace this by a better hash function
unsigned long djb2(unsigned char* str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

int64_t hash(const kokos_obj_t* obj)
{
    switch (obj->type) {
    case OBJ_INT:          return obj->integer;
    case OBJ_FLOAT:        return (int64_t)obj->floating;
    case OBJ_SYMBOL:       return djb2((unsigned char*)obj->symbol);
    case OBJ_STRING:       return djb2((unsigned char*)obj->string);
    case OBJ_PROCEDURE:
    case OBJ_BUILTIN_PROC:
    case OBJ_SPECIAL_FORM:
    case OBJ_MAP:
    case OBJ_BOOL:         return (int64_t)obj;
    case OBJ_LIST:         {
        int64_t sum = 0;
        for (size_t i = 0; i < obj->list.len; i++)
            sum += hash(obj->list.objs[i]);
        return sum;
    }
    case OBJ_VEC: {
        int64_t sum = 0;
        for (size_t i = 0; i < obj->vec.len; i++)
            sum += hash(obj->vec.items[i]);
        return sum;
    }
    }
}

kokos_obj_map_t kokos_obj_map_make(size_t cap)
{
    return ht_make((ht_hash_func)hash, (ht_eq_func)kokos_obj_eq, cap);
}

kokos_obj_t* kokos_obj_map_find(kokos_obj_map_t* map, const kokos_obj_t* key)
{
    return ht_find(map, key);
}

void kokos_obj_map_add(kokos_obj_map_t* map, kokos_obj_t* key, kokos_obj_t* value)
{
    ht_add(map, key, value);
}

void kokos_obj_map_destroy(kokos_obj_map_t *map)
{
    ht_destroy(map);
}
