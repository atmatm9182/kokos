#include "parser.h"
#include "ast.h"
#include "base.h"
#include "lexer.h"
#include "macros.h"
#include "token.h"

#include <stdio.h>
#include <string.h>

char err_buf[512];

static void set_unterminated_literal_err(const char* what, kokos_token_t token)
{
    sprintf(err_buf, "unterminated %s literal: '" SV_FMT "' at %s:%zu:%zu", what,
        (int)token.value.size, token.value.ptr, token.location.filename, token.location.row,
        token.location.col);
}

static kokos_expr_t* alloc_empty(const kokos_token_t* token, kokos_expr_type_e expr_type)
{
    kokos_expr_t* ptr = KOKOS_ALLOC(sizeof(kokos_expr_t));
    KOKOS_VERIFY(ptr);
    ptr->token = *token;
    ptr->type = expr_type;
    return ptr;
}

static void parser_advance(kokos_parser_t* parser)
{
    if (!parser->peek) {
        parser->cur = NULL;
        return;
    }

    *parser->cur = *parser->peek;
    if (!kokos_lex_next(parser->lexer, parser->peek)) {
        parser->peek = NULL;
    }
}

static kokos_expr_t* alloc_float(const kokos_token_t* token)
{
    kokos_expr_t* expr = alloc_empty(token, EXPR_FLOAT_LIT);
    return expr;
}

static kokos_expr_t* alloc_int(const kokos_token_t* token)
{
    kokos_expr_t* expr = alloc_empty(token, EXPR_INT_LIT);
    return expr;
}

static kokos_expr_t* alloc_vec(const kokos_token_t* token, kokos_vec_t vec)
{
    kokos_expr_t* expr = alloc_empty(token, EXPR_VECTOR);
    expr->vec = vec;
    return expr;
}

static kokos_expr_t* alloc_list(const kokos_token_t* token, kokos_expr_t const** items, size_t len)
{
    kokos_expr_t* expr = alloc_empty(token, EXPR_LIST);
    expr->list = (kokos_list_t) { .items = items, .len = len };
    return expr;
}

static kokos_expr_t* alloc_ident(const kokos_token_t* token)
{
    return alloc_empty(token, EXPR_IDENT);
}

static kokos_expr_t* alloc_string(const kokos_token_t* token)
{
    return alloc_empty(token, EXPR_STRING_LIG);
}

static kokos_expr_t* alloc_map(const kokos_token_t* token, kokos_map_t map)
{
    kokos_expr_t* expr = alloc_empty(token, EXPR_MAP);
    expr->map = map;
    return expr;
}

static kokos_expr_t* parse_vec(kokos_parser_t* parser, kokos_token_t start_token)
{
    parser_advance(parser);

    kokos_vec_t vec;
    DA_INIT(&vec, 0, 3);

    while (parser->cur && parser->cur->type != TT_RBRACKET) {
        kokos_expr_t* elem = kokos_parser_next(parser);
        if (!elem) {
            goto fail;
        }
        DA_ADD(&vec, elem);
    }

    if (!parser->cur) {
        set_unterminated_literal_err("vec", start_token);
        goto fail;
    }

    goto success;

fail:
    DA_FREE(&vec);
    return NULL;

success:
    parser_advance(parser);
    return alloc_vec(&start_token, vec);
}

static kokos_expr_t* parse_list(kokos_parser_t* parser, kokos_token_t start_token)
{
    parser_advance(parser);

    kokos_vec_t list;
    DA_INIT(&list, 0, 3);

    while (parser->cur && parser->cur->type != TT_RPAREN) {
        kokos_expr_t* elem = kokos_parser_next(parser);
        if (!elem) {
            goto fail;
        }
        DA_ADD(&list, elem);
    }

    if (!parser->cur) {
        set_unterminated_literal_err("list", start_token);
        goto fail;
    }

    goto success;

fail:
    DA_FREE(&list);
    return NULL;

success:
    parser_advance(parser);
    return alloc_list(&start_token, list.items, list.len);
}

static kokos_expr_t* parse_map(kokos_parser_t* parser, kokos_token_t start_token)
{
    parser_advance(parser);

    kokos_vec_t keys;
    kokos_vec_t values;
    DA_INIT(&keys, 0, 1);
    DA_INIT(&values, 0, 1);

    while (parser->cur && parser->cur->type != TT_RBRACE) {
        kokos_expr_t* key = kokos_parser_next(parser);
        if (!key) {
            goto fail;
        }
        kokos_expr_t* value = kokos_parser_next(parser);
        if (!value) {
            goto fail;
        }

        DA_ADD(&keys, key);
        DA_ADD(&values, value);
    }

    if (!parser->cur) {
        set_unterminated_literal_err("map", start_token);
        goto fail;
    }

    goto success;

fail:
    DA_FREE(&keys);
    DA_FREE(&values);
    return NULL;

success: {
    parser_advance(parser);

    kokos_map_t map = { .keys = keys.items, .values = values.items, .len = values.len };
    return alloc_map(&start_token, map);
}
}

kokos_expr_t* kokos_parser_next(kokos_parser_t* parser)
{
    kokos_expr_t* result = NULL;
    if (!parser->cur)
        return result;

    kokos_token_t* cur = parser->cur;

    switch (cur->type) {
    case TT_INT_LIT:
        result = alloc_int(cur);
        parser_advance(parser);
        break;
    case TT_FLOAT_LIT:
        result = alloc_float(cur);
        parser_advance(parser);
        break;
    case TT_IDENT:
        result = alloc_ident(cur);
        parser_advance(parser);
        break;
    case TT_STR_LIT:
        result = alloc_string(cur);
        parser_advance(parser);
        break;
    case TT_LPAREN: {
        result = parse_list(parser, *cur);
        break;
    }
    case TT_LBRACKET: {
        result = parse_vec(parser, *cur);
        break;
    }
    case TT_LBRACE: {
        result = parse_map(parser, *cur);
        break;
    }
    case TT_RPAREN:
    case TT_RBRACE:
    case TT_RBRACKET:
    case TT_STR_LIT_UNCLOSED:
    case TT_ILLEGAL:
        sprintf(err_buf, "unexpected token '" SV_FMT "' at %s:%lu:%lu", (int)cur->value.size,
            cur->value.ptr, cur->location.filename, cur->location.row, cur->location.col);
        break;
    case TT_QUOTE: {
        kokos_expr_t* expr = kokos_parser_next(parser);
        if (!expr) {
            return NULL;
        }
        expr->type |= 1 << 31;
        break;
    }
    }

    return result;
}

static kokos_token_t _cur_token;
static kokos_token_t _peek_token;

kokos_parser_t kokos_parser_init(kokos_lexer_t* lexer)
{
    kokos_parser_t parser = { .lexer = lexer, .cur = &_cur_token, .peek = &_peek_token };
    parser_advance(&parser);
    parser_advance(&parser);
    return parser;
}

kokos_program_t kokos_parser_program(kokos_parser_t* parser)
{
    kokos_program_t prog;
    DA_INIT(&prog, 0, 0);

    kokos_expr_t* expr;
    while ((expr = kokos_parser_next(parser))) {
        DA_ADD(&prog, expr);
    }

    return prog;
}

bool kokos_parser_ok(const kokos_parser_t* parser)
{
    (void)parser;
    return strlen(err_buf) == 0;
}

const char* kokos_parser_get_err(const kokos_parser_t* parser)
{
    (void)parser;
    return err_buf;
}
