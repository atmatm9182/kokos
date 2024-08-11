#ifndef VM_PARSER_H_
#define VM_PARSER_H_

#include "ast.h"
#include "lexer.h"

typedef struct kokos_parser {
    kokos_lexer_t* lexer;
    kokos_token_t* cur;
    kokos_token_t* peek;
} kokos_parser_t;

kokos_module_t kokos_parser_parse_module(kokos_parser_t* parser);
bool kokos_parser_next(kokos_parser_t* parser, kokos_expr_t* expr);
kokos_parser_t kokos_parser_init(kokos_lexer_t* lexer);

bool kokos_parser_ok(kokos_parser_t const* parser);
char const* kokos_parser_get_err(kokos_parser_t const* parser);

#endif // VM_PARSER_H_
