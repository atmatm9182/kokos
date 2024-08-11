#include "value.h"
#include "macros.h"
#include "runtime.h"
#include <stdio.h>

void kokos_value_print(kokos_value_t value)
{
    if (IS_TRUE(value)) {
        printf("true");
        return;
    }

    if (IS_FALSE(value)) {
        printf("false");
        return;
    }

    if (IS_NIL(value)) {
        printf("nil");
        return;
    }

    switch (VALUE_TAG(value)) {
    case STRING_TAG: {
        kokos_runtime_string_t* string = (kokos_runtime_string_t*)(value.as_int & ~STRING_BITS);
        printf("\"%.*s\"", (int)string->len, string->ptr);
        break;
    }
    case VECTOR_TAG: {
        kokos_runtime_vector_t* vector = (kokos_runtime_vector_t*)(value.as_int & ~VECTOR_BITS);
        printf("[");
        for (size_t i = 0; i < vector->len; i++) {
            kokos_value_print(vector->items[i]);
            if (i != vector->len - 1) {
                printf(" ");
            }
        }
        printf("]");
        break;
    }
    case MAP_TAG: {
        kokos_runtime_map_t* map = (kokos_runtime_map_t*)(value.as_int & ~MAP_BITS);
        hash_table table = map->table;

        size_t printed_count = 0;
        printf("{");
        for (size_t i = 0; i < table.cap; i++) {
            ht_bucket* bucket = table.buckets[i];
            if (!bucket) {
                continue;
            }

            for (size_t j = 0; j < bucket->len; j++) {
                ht_kv_pair kv = bucket->items[j];
                kokos_value_t key = TO_VALUE((uint64_t)kv.key);
                kokos_value_t value = TO_VALUE((uint64_t)kv.value);

                kokos_value_print(key);
                printf(" ");
                kokos_value_print(value);
                if (j != bucket->len - 1) {
                    printf(" ");
                }
            }

            if (++printed_count != table.len) {
                printf(" ");
            }
        }
        printf("}");
        break;
    }
    case LIST_TAG: {
        kokos_runtime_list_t* list = GET_LIST(value);

        printf("(");
        for (size_t i = 0; i < list->len; i++) {
            kokos_value_print(list->items[i]);
            if (i != list->len - 1) {
                printf(" ");
            }
        }
        printf(")");
        break;
    }
    case INT_TAG: {
        uint32_t i = GET_INT(value);
        printf("%d", i);
        break;
    }
    case PROC_TAG: {
        kokos_runtime_proc_t* proc = GET_PTR(value);
        switch (proc->type) {
        case PROC_KOKOS:  printf("<kokos proc at ip %lu>", proc->kokos.label); break;
        case PROC_NATIVE: printf("<native proc at address %p>", proc->native); break;
        }
        break;
    }
    default: {
        KOKOS_VERIFY(IS_DOUBLE(value));
        printf("%f", value.as_double);
        break;
    }
    }
}
