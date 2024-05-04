#ifndef LEXER_H_
#define LEXER_H_

#include "location.h"
#include "token.h"

#include <stddef.h>

struct kokos_lexer {
    string_view contents;
    size_t pos;
    kokos_location_t location;
};

typedef struct kokos_lexer kokos_lexer_t;

int kokos_lex_next(kokos_lexer_t* lexer, kokos_token_t* token);

kokos_lexer_t kokos_lex_buf(const char* buf, size_t buf_size);
kokos_lexer_t kokos_lex_named_buf(const char* buf, size_t buf_size, const char* name);

#endif // LEXER_H_
