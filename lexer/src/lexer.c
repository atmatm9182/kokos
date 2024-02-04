#define BASE_IMPL
#include "lexer.h"
#include "token.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static inline char cur_char(kokos_lexer_t* lex)
{
    return lex->contents.ptr[lex->pos];
}

static bool lex_advance(kokos_lexer_t* lex)
{
    if (lex->pos >= lex->contents.size) {
        return false;
    }

    lex->pos++;
    lex->location.col++;
    return true;
}

static bool lex_skip_whitespace(kokos_lexer_t* lex)
{
    while (lex->pos < lex->contents.size) {
        switch (cur_char(lex)) {
        case ' ':
        case '\t':
            if (!lex_advance(lex)) {
                return false;
            }
            break;
        case '\n':
            lex->location.row++;
            if (!lex_advance(lex)) {
                return false;
            }
            break;
        default:
            return true;
        }
    }

    return true;
}

int kokos_lex_next(kokos_lexer_t* lex, kokos_token_t* token)
{
    lex_skip_whitespace(lex);
    if (lex->pos == lex->contents.size) {
        return 0;
    }

    char c = cur_char(lex);
    token->location = lex->location;
    switch (c) {
    case '(':
        token->type = TT_LPAREN;
        token->value = sv_slice(lex->contents, lex->pos, 1);
        break;
    case ')':
        token->type = TT_RPAREN;
        token->value = sv_slice(lex->contents, lex->pos, 1);
        break;
    case ';':
        token->type = TT_SEMICOLON;
        token->value = sv_slice(lex->contents, lex->pos, 1);
        break;
    case '"': {
        token->type = TT_STR_LIT;
        if (!lex_advance(lex)) {
            token->type = TT_STR_LIT_UNCLOSED;
            token->value = sv_slice(lex->contents, lex->pos, 1);
            return 0;
        }

        size_t start = lex->pos;
        while (lex->pos < lex->contents.size && lex->contents.ptr[lex->pos] != '"') {
            lex_advance(lex);
        }

        if (lex->pos == lex->contents.size) {
            token->type = TT_STR_LIT_UNCLOSED;
            token->value = sv_slice(lex->contents, start - 1, 1);
            return 0;
        }

        token->type = TT_STR_LIT;
        token->value = sv_slice(lex->contents, start, lex->pos - start);
        break;
    }
    case '\0':
        return 0;
    default:
        if (isdigit(c)) {
            token->type = TT_INT_LIT;
            size_t start = lex->pos;
            lex_advance(lex);
            while (lex->pos < lex->contents.size && isdigit(cur_char(lex))) {
                lex_advance(lex);
            }
            token->value = sv_slice(lex->contents, start, lex->pos - start);
            lex->pos--;
            lex->location.col--;
            break;
        }
        token->type = TT_IDENT;
        size_t start = lex->pos;
        lex_advance(lex);
        while (lex->pos < lex->contents.size && !isspace(cur_char(lex))) {
            lex_advance(lex);
        }
        token->value = sv_slice(lex->contents, start, lex->pos - start);
        lex->pos--;
        lex->location.col--;

        break;
    }

    lex_advance(lex);

    return 1;
}

kokos_lexer_t kokos_lex_buf(const char* buf, size_t buf_size)
{
    buf_size = buf_size == (size_t)-1 ? strlen(buf) : buf_size;
    return (kokos_lexer_t) { .contents = sv_make(buf, buf_size),
        .pos = 0,
        .location = { .filename = NULL, .row = 1, .col = 1 } };
}
