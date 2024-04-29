#include "parser.h"
#include "ast.h"
#include "base.h"
#include "lexer.h"
#include "macros.h"
#include "token.h"

#include <stdio.h>

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

kokos_expr_t* kokos_parser_next(kokos_parser_t* parser)
{
    kokos_expr_t* result = NULL;
    if (!parser->cur)
        return result;

    switch (parser->cur->type) {
    case TT_INT_LIT:
        result = alloc_int(parser->cur);
        parser_advance(parser);
        break;
    case TT_FLOAT_LIT:
        result = alloc_float(parser->cur);
        parser_advance(parser);
        break;
    case TT_IDENT:
        result = alloc_ident(parser->cur);
        parser_advance(parser);
        break;
    case TT_STR_LIT:
        result = alloc_string(parser->cur);
        parser_advance(parser);
        break;
    case TT_LPAREN: {
        kokos_token_t start_token = *parser->cur;

        parser_advance(parser);
        struct {
            kokos_expr_t const** items;
            size_t len;
            size_t cap;
        } list;
        DA_INIT(&list, 0, 3);

        while (parser->cur && parser->cur->type != TT_RPAREN) {
            kokos_expr_t* elem = kokos_parser_next(parser);
            if (!elem)
                return NULL;
            DA_ADD(&list, elem);
        }

        if (!parser->cur) {
            DA_FREE(&list);
            return NULL;
        }

        parser_advance(parser);

        result = alloc_list(&start_token, list.items, list.len);
        break;
    }
    default: {
        char buf[64];
        sprintf(buf, "%s", kokos_token_type_str(parser->cur->type));
        KOKOS_TODO(buf);
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
