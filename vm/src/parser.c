#include "parser.h"
#include "ast.h"
#include "base.h"
#include "lexer.h"
#include "macros.h"
#include "token.h"

#include <stdio.h>
#include <string.h>

char err_buf[512];

static void set_unterminated_literal_err(char const* what, kokos_token_t token)
{
    sprintf(err_buf, "unterminated %s literal: '" SV_FMT "' at %s:%zu:%zu", what,
        SV_ARG(token.value), token.location.filename, token.location.row, token.location.col);
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

static bool parse_vec(kokos_parser_t* parser, kokos_token_t start_token, kokos_expr_t* expr)
{
    parser_advance(parser);

    kokos_vec_t* vec = &expr->vec;
    DA_INIT(vec, 0, 3);

    while (parser->cur && parser->cur->type != TT_RBRACKET) {
        kokos_expr_t expr;
        TRY(kokos_parser_next(parser, &expr));
        DA_ADD(vec, expr);
    }

    if (!parser->cur) {
        set_unterminated_literal_err("vec", start_token);
        return false;
    }

    parser_advance(parser);
    expr->token = start_token;
    expr->type = EXPR_VECTOR;
    return true;
}

static bool parse_list(kokos_parser_t* parser, kokos_token_t start_token, kokos_expr_t* expr)
{
    parser_advance(parser);

    kokos_vec_t list;
    DA_INIT(&list, 0, 3);

    while (parser->cur && parser->cur->type != TT_RPAREN) {
        kokos_expr_t expr;
        TRY(kokos_parser_next(parser, &expr));
        DA_ADD(&list, expr);
    }

    if (!parser->cur) {
        set_unterminated_literal_err("list", start_token);
        return false;
    }

    parser_advance(parser);
    expr->list = (kokos_list_t) { .items = list.items, .len = list.len };
    expr->type = EXPR_LIST;
    expr->token = start_token;
    return true;
}

static bool parse_map(kokos_parser_t* parser, kokos_token_t start_token, kokos_expr_t* expr)
{
    parser_advance(parser);

    kokos_vec_t keys;
    kokos_vec_t values;
    DA_INIT(&keys, 0, 1);
    DA_INIT(&values, 0, 1);

    while (parser->cur && parser->cur->type != TT_RBRACE) {
        kokos_expr_t key;
        TRY(kokos_parser_next(parser, &key));

        kokos_expr_t value;
        TRY(kokos_parser_next(parser, &value));

        DA_ADD(&keys, key);
        DA_ADD(&values, value);
    }

    if (!parser->cur) {
        set_unterminated_literal_err("map", start_token);
        return false;
    }

    parser_advance(parser);

    expr->map = (kokos_map_t) { .keys = keys.items, .values = values.items, .len = values.len };
    expr->type = EXPR_MAP;
    expr->token = start_token;
    return true;
}

bool kokos_parser_next(kokos_parser_t* parser, kokos_expr_t* expr)
{
    if (!parser->cur)
        return false;

    memset(expr, 0, sizeof(kokos_expr_t));

    kokos_token_t* cur = parser->cur;

    switch (cur->type) {
    case TT_INT_LIT:
        expr->type = EXPR_INT_LIT;
        expr->token = *cur;
        parser_advance(parser);
        break;
    case TT_FLOAT_LIT:
        expr->type = EXPR_FLOAT_LIT;
        expr->token = *cur;
        parser_advance(parser);
        break;
    case TT_IDENT:
        expr->type = EXPR_IDENT;
        expr->token = *cur;
        parser_advance(parser);
        break;
    case TT_STR_LIT:
        expr->type = EXPR_STRING_LIT;
        expr->token = *cur;
        parser_advance(parser);
        break;
    case TT_LPAREN: {
        parse_list(parser, *cur, expr);
        break;
    }
    case TT_LBRACKET: {
        parse_vec(parser, *cur, expr);
        break;
    }
    case TT_LBRACE: {
        parse_map(parser, *cur, expr);
        break;
    }
    case TT_RPAREN:
    case TT_RBRACE:
    case TT_RBRACKET:
    case TT_STR_LIT_UNCLOSED:
    case TT_ILLEGAL:
        sprintf(err_buf, "unexpected token '" SV_FMT "' at %s:%lu:%lu", SV_ARG(cur->value),
            cur->location.filename, cur->location.row, cur->location.col);
        break;
    case TT_QUOTE: {
        parser_advance(parser);
        TRY(kokos_parser_next(parser, expr));

        expr->flags |= EXPR_FLAG_QUOTE;
        break;
    }
    }

    return true;
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

kokos_module_t kokos_parser_parse_module(kokos_parser_t* parser)
{
    kokos_module_t prog;
    DA_INIT(&prog, 0, 0);

    kokos_expr_t expr;
    while (kokos_parser_next(parser, &expr)) {
        DA_ADD(&prog, expr);
    }

    return prog;
}

bool kokos_parser_ok(kokos_parser_t const* parser)
{
    (void)parser;
    return strlen(err_buf) == 0;
}

char const* kokos_parser_get_err(kokos_parser_t const* parser)
{
    (void)parser;
    return err_buf;
}
