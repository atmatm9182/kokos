#ifndef TOKEN_H_
#define TOKEN_H_

#include "base.h"
#include "location.h"

enum kokos_token_type {
    TT_LPAREN,
    TT_RPAREN,
    TT_IDENT,
    TT_INT_LIT,
    TT_STR_LIT,
    TT_STR_LIT_UNCLOSED,
    TT_ILLEGAL,
};

typedef enum kokos_token_type kokos_token_type_e;

struct kokos_token {
    kokos_token_type_e type;
    string_view value;
    kokos_location_t location;
};

typedef struct kokos_token kokos_token_t;

static inline const char* kokos_token_type_str(kokos_token_type_e tt)
{
    switch (tt) {
    case TT_STR_LIT:          return "TT_STR_LIT";
    case TT_RPAREN:           return "TT_RPAREN";
    case TT_STR_LIT_UNCLOSED: return "TT_STR_LIT_UNCLOSED";
    case TT_IDENT:            return "TT_IDENT";
    case TT_INT_LIT:          return "TT_INT_LIT";
    case TT_LPAREN:           return "TT_LPAREN";
    case TT_ILLEGAL:          return "TT_ILLEGAL";
    }
}

#endif // TOKEN_H_
