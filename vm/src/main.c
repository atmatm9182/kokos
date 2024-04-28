#include "lexer.h"
#include "src/ast.h"
#include "src/parser.h"

int main()
{
    char code[] = "(* (+ 1 2 3) 2 2.5 \"hello\")";

    kokos_lexer_t lexer = kokos_lex_buf(code, sizeof(code));
    kokos_parser_t parser = kokos_parser_init(&lexer);

    kokos_expr_t* expr;
    while ((expr = kokos_parser_next(&parser))) {
        kokos_expr_dump(expr);
    }

    return 0;
}
