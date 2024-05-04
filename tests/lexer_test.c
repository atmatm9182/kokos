#define BASE_STATIC
#define BASE_IMPLEMENTATION
#include "base.h"
#include "lexer.h"

#include <assert.h>

int test_empty()
{
    const char* code = "";
    kokos_lexer_t lexer = kokos_lex_buf(code, strlen(code));
    kokos_token_t token;
    return kokos_lex_next(&lexer, &token);
}

int test_simple()
{
    const char* code = "123 \"string\" 6.9 () hello \"i am unclosed";
    kokos_lexer_t lexer = kokos_lex_buf(code, strlen(code));
    kokos_token_t token;

    assert(kokos_lex_next(&lexer, &token));
    assert(token.type == TT_INT_LIT);
    assert(sv_eq_cstr(token.value, "123"));

    assert(kokos_lex_next(&lexer, &token));
    assert(token.type == TT_STR_LIT);
    assert(sv_eq_cstr(token.value, "string"));

    assert(kokos_lex_next(&lexer, &token));
    assert(token.type == TT_FLOAT_LIT);
    assert(sv_eq_cstr(token.value, "6.9"));

    assert(kokos_lex_next(&lexer, &token));
    assert(token.type == TT_LPAREN);
    assert(sv_eq_cstr(token.value, "("));

    assert(kokos_lex_next(&lexer, &token));
    assert(token.type == TT_RPAREN);
    assert(sv_eq_cstr(token.value, ")"));

    assert(kokos_lex_next(&lexer, &token));
    assert(token.type == TT_IDENT);
    assert(sv_eq_cstr(token.value, "hello"));

    assert(kokos_lex_next(&lexer, &token));
    assert(token.type == TT_STR_LIT_UNCLOSED);
    assert(sv_eq_cstr(token.value, "\"i am unclosed"));

    assert(!kokos_lex_next(&lexer, &token));

    return 0;
}

int main()
{
    int result = 0;
    if (test_empty())
        result = 1;
    if (test_simple())
        result = 2;
    return result;
}
