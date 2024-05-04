#include <errno.h>
#include <stdio.h>
#include <string.h>

#define BASE_IMPLEMENTATION
#include "base.h"
#include "interpreter.h"
#include "lexer.h"
#include "parser.h"
#include "src/obj.h"
#include "src/util.h"

static bool eof = false;

static kokos_obj_t* read(kokos_interp_t* interp)
{
    static char buf[1024];
    if (!fgets(buf, 1024, stdin)) {
        eof = true;
        return NULL;
    }

    kokos_lexer_t lex = kokos_lex_buf(buf, -1);
    lex.location.filename = "repl";
    kokos_parser_t parser = kokos_parser_of_lexer(lex);
    return kokos_parser_next(&parser, interp);
}

static void print_location(kokos_location_t location)
{
    printf("%s:%lu:%lu", location.filename, location.row, location.col);
}

static void print_parser_error(void)
{
    printf("Parser error: ");
    switch (kokos_p_err) {
    case ERR_NONE: return;
    case ERR_ILLEGAL_CHAR:
        printf("Illegal char '");
        sv_print(kokos_p_err_tok.value);
        printf("' at ");
        print_location(kokos_p_err_tok.location);
        break;
    case ERR_UNEXPECTED_TOKEN:
        printf("Unexpected token '%s' at ", kokos_token_type_str(kokos_p_err_tok.type));
        print_location(kokos_p_err_tok.location);
        break;
    case ERR_UNMATCHED_DELIMITER:
        printf("Unmatched delimiter at ");
        print_location(kokos_p_err_tok.location);
        break;
    }

    printf("\n");
}

static void print_interpreter_error(void)
{
    printf("Evaluation error: %s\n", kokos_interp_get_error());
}

int run_repl(size_t gc_threshold)
{
    kokos_interp_t* interp = kokos_interp_new(gc_threshold);
    while (1) {
        printf("> ");

        kokos_obj_t* obj = read(interp);
        if (!obj) {
            if (eof)
                break;

            print_parser_error();
            continue;
        }

        obj = kokos_interp_eval(interp, obj, 1);
        if (!obj) {
            print_interpreter_error();
            continue;
        }

        kokos_obj_print(obj);

        printf("\n");
    }

    kokos_interp_destroy(interp);

    return 0;
}

int main(int argc, char* argv[])
{
    if (argc == 1)
        return run_repl(KOKOS_DEFAULT_GC_THRESHOLD);

    const char* script_name = argv[1];
    char* script_data = read_whole_file(script_name);
    if (!script_data) {
        fprintf(stderr, "Could not open file %s because of: %s\n", script_name, strerror(errno));
        return 1;
    }

    kokos_lexer_t lex = kokos_lex_buf(script_data, strlen(script_data));
    lex.location.filename = script_name;
    kokos_parser_t parser = kokos_parser_of_lexer(lex);
    kokos_interp_t* interpreter = kokos_interp_new(KOKOS_DEFAULT_GC_THRESHOLD);

    kokos_obj_t* cur;
    while ((cur = kokos_parser_next(&parser, interpreter))) {
        kokos_obj_t* obj = kokos_interp_eval(interpreter, cur, 1);
        if (!obj) {
            print_interpreter_error();
            return 1;
        }
    }

    if (kokos_p_err != ERR_NONE) {
        print_parser_error();
        return 1;
    }

    kokos_interp_destroy(interpreter);
    free(script_data);

    return 0;
}
