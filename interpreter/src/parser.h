#ifndef PARSER_H_
#define PARSER_H_

#include "interpreter.h"
#include "lexer.h"

struct kokos_parser {
    kokos_token_t cur;
    kokos_lexer_t lex;
    int has_cur;
};

typedef struct kokos_parser kokos_parser_t;

/**
 * should check the value of this variable after parsing
 * 1 - unmatched paren
 */
extern int parse_err;

kokos_obj_t* kokos_parser_next(kokos_parser_t* parser, kokos_interp_t* interp);
kokos_parser_t kokos_parser_of_lexer(kokos_lexer_t lexer);

#endif // PARSER_H_
