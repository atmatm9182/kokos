#include "src/parser.h"
#include "src/map.h"
#include "src/obj.h"
#include "src/util.h"
#include "token.h"

#include <stdlib.h>
#include <string.h>

static kokos_obj_t* alloc_list(kokos_obj_t** objs, size_t len, kokos_interp_t* interp)
{
    kokos_obj_t* list = kokos_gc_alloc(&interp->gc);
    list->type = OBJ_LIST;
    list->list = (kokos_obj_list_t) { .objs = objs, .len = len };
    return list;
}

static kokos_obj_t* alloc_symbol(string_view value, kokos_interp_t* interp)
{
    kokos_obj_t* sym = kokos_gc_alloc(&interp->gc);
    sym->type = OBJ_SYMBOL;
    sym->symbol = malloc(sizeof(char) * (value.size + 1));
    memcpy(sym->symbol, value.ptr, value.size * sizeof(char));
    sym->symbol[value.size] = '\0';
    return sym;
}

static kokos_obj_t* alloc_integer(string_view value, kokos_interp_t* interp)
{
    kokos_obj_t* integer = kokos_gc_alloc(&interp->gc);
    integer->type = OBJ_INT;
    integer->integer = sv_atoi(value);
    return integer;
}

static kokos_obj_t* alloc_float(string_view value, kokos_interp_t* interp)
{
    kokos_obj_t* integer = kokos_gc_alloc(&interp->gc);
    integer->type = OBJ_FLOAT;
    integer->floating = sv_atof(value);
    return integer;
}

static kokos_obj_t* alloc_str(string_view value, kokos_interp_t* interp)
{
    kokos_obj_t* str = kokos_gc_alloc(&interp->gc);
    str->type = OBJ_STRING;
    str->string = malloc(sizeof(char) * (value.size + 1));
    memcpy(str->string, value.ptr, value.size * sizeof(char));
    str->string[value.size] = '\0';
    return str;
}

static void advance(kokos_parser_t* parser)
{
    parser->has_cur = kokos_lex_next(&parser->lex, &parser->cur);
}

kokos_parser_err_e kokos_p_err = 0;
kokos_token_t kokos_p_err_tok;

kokos_obj_t* kokos_parser_next(kokos_parser_t* parser, kokos_interp_t* interp)
{
    if (!parser->has_cur) {
        return NULL;
    }

    switch (parser->cur.type) {
    case TT_QUOTE: {
        advance(parser);
        kokos_obj_t* next = kokos_parser_next(parser, interp);
        if (!next)
            return NULL;

        next->quoted = true;
        return next;
    }

    case TT_LPAREN: {
        kokos_token_t list_token = parser->cur;

        struct {
            kokos_obj_t** items;
            size_t len;
            size_t cap;
        } objs;
        DA_INIT(&objs, 0, 1);

        advance(parser);
        while (parser->has_cur && parser->cur.type != TT_RPAREN) {
            kokos_obj_t* obj = kokos_parser_next(parser, interp);
            if (!obj) {
                DA_FREE(&objs);
                return NULL;
            }

            DA_ADD(&objs, obj);
        }

        if (!parser->has_cur) {
            kokos_p_err = ERR_UNMATCHED_DELIMITER;
            kokos_p_err_tok = list_token;
            DA_FREE(&objs);
            return NULL;
        }

        advance(parser);
        kokos_obj_t* list = alloc_list(objs.items, objs.len, interp);
        list->token = list_token;
        return list;
    }
    case TT_LBRACKET: {
        kokos_token_t start_token = parser->cur;

        kokos_obj_vec_t objs;
        DA_INIT(&objs, 0, 1);

        advance(parser);
        while (parser->has_cur && parser->cur.type != TT_RBRACKET) {
            kokos_obj_t* obj = kokos_parser_next(parser, interp);
            if (!obj) {
                DA_FREE(&objs);
                return NULL;
            }

            DA_ADD(&objs, obj);
        }

        if (!parser->has_cur) {
            kokos_p_err = ERR_UNMATCHED_DELIMITER;
            kokos_p_err_tok = start_token;
            DA_FREE(&objs);
            return NULL;
        }

        advance(parser);
        kokos_obj_t* vec = kokos_gc_alloc(&interp->gc);
        vec->token = start_token;
        vec->type = OBJ_VEC;
        vec->vec = objs;
        return vec;
    }
    case TT_LBRACE: {
        kokos_token_t start_token = parser->cur;

        kokos_obj_map_t map = kokos_obj_map_make(11);

        advance(parser);
        while (parser->has_cur && parser->cur.type != TT_RBRACE) {
            kokos_obj_t* key = kokos_parser_next(parser, interp);
            if (!key) {
                kokos_p_err = ERR_UNMATCHED_DELIMITER;
                kokos_p_err_tok = start_token;
                return NULL;
            }

            kokos_obj_t* value = kokos_parser_next(parser, interp);
            if (!value) {
                kokos_p_err = ERR_UNMATCHED_DELIMITER;
                kokos_p_err_tok = start_token;
                return NULL;
            }

            kokos_obj_map_add(&map, key, value);
        }

        if (!parser->has_cur) {
            kokos_p_err = ERR_UNMATCHED_DELIMITER;
            kokos_p_err_tok = start_token;
            kokos_obj_map_destroy(&map);
            return NULL;
        }

        advance(parser);
        kokos_obj_t* result = kokos_gc_alloc(&interp->gc);
        result->token = start_token;
        result->type = OBJ_MAP;
        result->map = map;
        return result;
    }
    case TT_IDENT: {
        kokos_obj_t* result = NULL;

        if (sv_eq_cstr(parser->cur.value, "true"))
            result = &kokos_obj_true;
        else if (sv_eq_cstr(parser->cur.value, "false"))
            result = &kokos_obj_false;
        else if (sv_eq_cstr(parser->cur.value, "nil"))
            result = &kokos_obj_nil;
        else
            result = alloc_symbol(parser->cur.value, interp);

        result->token = parser->cur;
        advance(parser);
        return result;
    }
    case TT_INT_LIT: {
        kokos_obj_t* integer = alloc_integer(parser->cur.value, interp);
        integer->token = parser->cur;
        advance(parser);
        return integer;
    }
    case TT_STR_LIT: {
        kokos_obj_t* str = alloc_str(parser->cur.value, interp);
        str->token = parser->cur;
        advance(parser);
        return str;
    }
    case TT_FLOAT_LIT: {
        kokos_obj_t* f = alloc_float(parser->cur.value, interp);
        f->token = parser->cur;
        advance(parser);
        return f;
    }
    case TT_ILLEGAL:
        kokos_p_err = ERR_ILLEGAL_CHAR;
        kokos_p_err_tok = parser->cur;
        advance(parser);
        return NULL;
    case TT_RBRACKET:
    case TT_RBRACE:
    case TT_RPAREN:
        kokos_p_err = ERR_UNEXPECTED_TOKEN;
        kokos_p_err_tok = parser->cur;
        advance(parser);
        return NULL;
    case TT_STR_LIT_UNCLOSED:
        kokos_p_err = ERR_UNMATCHED_DELIMITER;
        kokos_p_err_tok = parser->cur;
        advance(parser);
        return NULL;
    }

    KOKOS_FAIL_WITH("Unknown token type\n");
}

kokos_parser_t kokos_parser_of_lexer(kokos_lexer_t lexer)
{
    kokos_parser_t parser = { .lex = lexer, .has_cur = true };
    advance(&parser);
    return parser;
}
