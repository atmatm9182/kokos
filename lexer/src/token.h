#ifndef TOKEN_H_
#define TOKEN_H_

#include "location.h"
#include "base.h"

enum kokos_token_type {
    TT_LPAREN,
    TT_RPAREN,
    TT_IDENT,
    TT_INT_LIT,
    TT_STR_LIT,
    TT_STR_LIT_UNCLOSED,
    TT_SEMICOLON,
    TT_ILLEGAL,
};

typedef enum kokos_token_type kokos_token_type_e;

struct kokos_token {
    kokos_token_type_e type;
    string_view value;
    kokos_location_t location;
};

typedef struct kokos_token kokos_token_t;

#endif // TOKEN_H_
