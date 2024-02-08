#include <assert.h>
#include <stdio.h>

#include "interpreter.h"
#include "parser.h"

static kokos_interp_t* interp;

static kokos_obj_t* read(void)
{
    static char buf[1024];
    if (!fgets(buf, 1024, stdin))
        exit(0);

    kokos_lexer_t lex = kokos_lex_buf(buf, -1);
    kokos_parser_t parser = kokos_parser_of_lexer(lex);
    return kokos_parser_next(&parser, interp);
}

static kokos_obj_t* eval(kokos_obj_t* obj)
{
    return kokos_interp_eval(interp, obj, 1);
}

static void print_location(kokos_location_t location)
{
    printf("%s:%lu:%lu", location.filename, location.row, location.col);
}

static void print_parser_error(void)
{
    switch (kokos_p_err) {
    case ERR_NONE: assert(0 && "should never happen");
    case ERR_ILLEGAL_CHAR:
        printf("Illegal char '");
        sv_print(kokos_p_err_tok.value);
        printf("' at ");
        print_location(kokos_p_err_tok.location);
        break;
    case ERR_UNCLOSED_STR:
        printf("Unclosed string literal \"");
        sv_print(kokos_p_err_tok.value);
        printf("\" at ");
        print_location(kokos_p_err_tok.location);
        break;
    case ERR_UNEXPECTED_TOKEN:
        printf("Unexpected token '%s' at", kokos_token_type_str(kokos_p_err_tok.type));
        print_location(kokos_p_err_tok.location);
        break;
    case ERR_UNMATCHED_PAREN:
        printf("Unmatched parenthesis at ");
        kokos_p_err_tok.location.col += kokos_p_err_tok.value.size;
        print_location(kokos_p_err_tok.location);
        break;
    }

    printf("\n");
}

static void print_interpreter_error(void)
{
    printf("%s\n", kokos_interp_get_error());
}

int main(int argc, char* argv[])
{
    interp = kokos_interp_new(5);
    while (1) {
        printf("> ");

        kokos_obj_t* obj = read();
        if (!obj) {
            print_parser_error();
            continue;
        }

        obj = eval(obj);
        if (!obj) {
            print_interpreter_error();
            continue;
        }

        kokos_obj_print(obj);

        printf("\n");
    }
    return 0;
}
