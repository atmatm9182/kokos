#include <assert.h>
#include <stdio.h>

#include "interpreter.h"
#include "parser.h"

static kokos_interp_t* interp;

static kokos_obj_t* read(void)
{
    static char buf[1024];
    if (!fgets(buf, 1024, stdin))
        exit(0);

    kokos_lexer_t lex = kokos_lex_buf(buf, -1);
    kokos_parser_t parser = kokos_parser_of_lexer(lex);
    return kokos_parser_next(&parser, interp);
}

static kokos_obj_t* eval(kokos_obj_t* obj)
{
    return kokos_interp_eval(interp, obj);
}

static void print(kokos_obj_t* obj)
{
    if (obj == &kokos_obj_nil) {
        printf("nil");
        return;
    }

    switch (obj->type) {
    case OBJ_INT:
        printf("%ld", obj->integer);
        break;
    case OBJ_STRING:
        printf("\"%s\"", obj->string);
        break;
    case OBJ_SYMBOL:
        printf("%s", obj->symbol);
        break;
    case OBJ_LIST:
        printf("(");
        for (size_t i = 0; i < obj->list.len; i++) {
            print(obj->list.objs[i]);
            if (i != obj->list.len - 1) {
                printf(" ");
            }
        }
        printf(")");
        break;
    case OBJ_BUILTIN_PROC:
        printf("<builtin function> addr %p", (void*)obj->builtin);
        break;
    case OBJ_PROCEDURE:
        printf("<procedure> %p", (void*)obj);
        break;
    case OBJ_SPECIAL_FORM:
        assert(0 && "something went completely wrong");
    }
}

int main(int argc, char* argv[])
{
    interp = kokos_interp_new(50);
    while (1) {
        printf("> ");

        kokos_obj_t* obj = read();
        assert(obj);
        obj = eval(obj);
        print(obj);

        printf("\n");
    }
    return 0;
}
