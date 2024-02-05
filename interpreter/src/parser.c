#include "parser.h"
#include "token.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static kokos_obj_t* alloc_list(kokos_obj_t** objs, size_t len, kokos_interp_t* interp)
{
    kokos_obj_t* list = kokos_interp_alloc(interp);
    list->type = OBJ_LIST;
    list->list = (kokos_obj_list_t) { .objs = objs, .len = len };
    return list;
}

static kokos_obj_t* alloc_symbol(string_view value, kokos_interp_t* interp)
{
    kokos_obj_t* sym = kokos_interp_alloc(interp);
    sym->type = OBJ_SYMBOL;
    sym->symbol = malloc(sizeof(char) * (value.size + 1));
    memcpy(sym->symbol, value.ptr, value.size);
    sym->symbol[value.size] = '\0';
    return sym;
}

static int64_t sv_atoi(string_view sv)
{
    int64_t num = 0;
    for (size_t i = 0; i < sv.size; i++) {
        num = num * 10 + (sv.ptr[i] - '0');
    }
    return num;
}

static kokos_obj_t* alloc_integer(string_view value, kokos_interp_t* interp)
{
    kokos_obj_t* integer = kokos_interp_alloc(interp);
    integer->type = OBJ_INT;
    integer->integer = sv_atoi(value);
    return integer;
}

static kokos_obj_t* alloc_str(string_view value, kokos_interp_t* interp)
{
    kokos_obj_t* str = kokos_interp_alloc(interp);
    str->type = OBJ_STRING;
    str->string = malloc(sizeof(char) * (value.size + 1));
    memcpy(str->string, value.ptr, value.size);
    return str;
}

static void advance(kokos_parser_t* parser)
{
    parser->has_cur = kokos_lex_next(&parser->lex, &parser->cur);
}

int parse_err = 0;

kokos_obj_t* kokos_parser_next(kokos_parser_t* parser, kokos_interp_t* interp)
{
    if (!parser->has_cur)
        return NULL;

    switch (parser->cur.type) {
    case TT_LPAREN: {
        struct {
            kokos_obj_t** items;
            size_t len;
            size_t cap;
        } gc_objs;
        DA_INIT(&gc_objs, 0, 1);

        advance(parser);
        while (parser->has_cur && parser->cur.type != TT_RPAREN) {
            DA_ADD(&gc_objs, kokos_parser_next(parser, interp));
        }

        if (!parser->has_cur) {
            parse_err = 1;
            for (size_t i = 0; i < gc_objs.len; i++) {
                free(gc_objs.items[i]);
            }
            DA_FREE(&gc_objs);
            return NULL;
        }

        advance(parser);
        return alloc_list(gc_objs.items, gc_objs.len, interp);
    }
    case TT_IDENT: {
        kokos_obj_t* ident = alloc_symbol(parser->cur.value, interp);
        advance(parser);
        return ident;
    }
    case TT_INT_LIT: {
        kokos_obj_t* integer = alloc_integer(parser->cur.value, interp);
        advance(parser);
        return integer;
    }
    case TT_STR_LIT: {
        kokos_obj_t* str = alloc_str(parser->cur.value, interp);
        advance(parser);
        return str;
    }
    default:
        assert(0 && "TODO: implement error handling");
    }
}

kokos_parser_t kokos_parser_of_lexer(kokos_lexer_t lexer)
{
    kokos_parser_t parser = { .lex = lexer, .has_cur = true };
    advance(&parser);
    return parser;
}
