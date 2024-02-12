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

enum kokos_parser_err {
    ERR_NONE,
    ERR_UNMATCHED_DELIMITER,
    ERR_ILLEGAL_CHAR,
    ERR_UNEXPECTED_TOKEN,
};

typedef enum kokos_parser_err kokos_parser_err_e;

extern kokos_parser_err_e kokos_p_err;
extern kokos_token_t kokos_p_err_tok;

kokos_obj_t* kokos_parser_next(kokos_parser_t* parser, kokos_interp_t* interp);
kokos_parser_t kokos_parser_of_lexer(kokos_lexer_t lexer);

#endif // PARSER_H_
