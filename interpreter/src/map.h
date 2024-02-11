#ifndef MAP_H_
#define MAP_H_

#include "base.h"

typedef hash_table kokos_obj_map_t;
typedef struct kokos_obj kokos_obj_t;

kokos_obj_map_t kokos_obj_map_make(size_t cap);
void kokos_obj_map_add(kokos_obj_map_t* map, kokos_obj_t* key, kokos_obj_t* value);
kokos_obj_t* kokos_obj_map_find(kokos_obj_map_t* map, const kokos_obj_t* key);
void kokos_obj_map_destroy(kokos_obj_map_t* map);

#endif // MAP_H_
