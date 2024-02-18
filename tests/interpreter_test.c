#include "interpreter.h"
#include "lexer.h"
#include "obj.h"
#include "parser.h"
#include <string.h>

int test_arithmetic(void)
{
    const char* code = "(+ 1 2) (- 5 2) (* 2 8) (/ 10 4)";
    kokos_lexer_t lexer = kokos_lex_buf(code, strlen(code));
    kokos_parser_t parser = kokos_parser_of_lexer(lexer);

    kokos_interp_t* interp = kokos_interp_new(100);

    kokos_obj_t* plus = kokos_parser_next(&parser, interp);
    assert(plus);

    
    
    return 0;
}

int main()
{
    return 0;
}
