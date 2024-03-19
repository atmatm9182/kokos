#include "env.h"
#include "interpreter.h"
#include "lexer.h"
#include "obj.h"
#include "parser.h"

#include <assert.h>
#include <math.h>
#include <string.h>

void test_def(void)
{
    const char* code = "(def x 1) (def x 2)";
    kokos_lexer_t lexer = kokos_lex_buf(code, strlen(code));
    kokos_parser_t parser = kokos_parser_of_lexer(lexer);
    kokos_interp_t* interp = kokos_interp_new(100);

    kokos_obj_t* first = kokos_parser_next(&parser, interp);
    assert(first);

    size_t prev_env_size = interp->current_env->len;
    first = kokos_interp_eval(interp, first, 1);
    assert(first);
    assert(first->type == OBJ_NIL);
    assert(first == &kokos_obj_nil);
    assert(interp->current_env->len == prev_env_size + 1);

    kokos_env_pair_t* x = kokos_env_find(interp->current_env, "x");
    assert(x);
    assert(x->value->type == OBJ_INT);
    assert(x->value->integer == 1);

    kokos_obj_t* second = kokos_parser_next(&parser, interp);
    assert(second);

    prev_env_size = interp->current_env->len;
    second = kokos_interp_eval(interp, second, 1);
    assert(second);
    assert(second->type == OBJ_NIL);
    assert(second == &kokos_obj_nil);
    assert(interp->current_env->len == prev_env_size);

    x = kokos_env_find(interp->current_env, "x");
    assert(x);
    assert(x->value->type == OBJ_INT);
    assert(x->value->integer == 2);

    kokos_interp_destroy(interp);
}

void test_arithmetic(void)
{
    const char* code = "(+ 1 2 3 4 5) (- 5 2.1) (* 2 8) (/ 10 4 1)";
    kokos_lexer_t lexer = kokos_lex_buf(code, strlen(code));
    kokos_parser_t parser = kokos_parser_of_lexer(lexer);
    kokos_interp_t* interp = kokos_interp_new(100);

    kokos_obj_t* plus = kokos_parser_next(&parser, interp);
    assert(plus);
    plus = kokos_interp_eval(interp, plus, true);
    assert(plus);
    assert(plus->type == OBJ_INT);
    assert(plus->integer == 15);

    kokos_obj_t* minus = kokos_parser_next(&parser, interp);
    assert(minus);
    minus = kokos_interp_eval(interp, minus, true);
    assert(minus);
    assert(minus->type == OBJ_FLOAT);
    assert(minus->floating == 2.9);

    kokos_obj_t* star = kokos_parser_next(&parser, interp);
    assert(star);
    star = kokos_interp_eval(interp, star, true);
    assert(star);
    assert(star->type == OBJ_INT);
    assert(star->integer == 16);

    kokos_obj_t* slash = kokos_parser_next(&parser, interp);
    assert(slash);
    slash = kokos_interp_eval(interp, slash, true);
    assert(slash);
    assert(slash->type == OBJ_FLOAT);
    assert(slash->floating == 2.5);

    kokos_interp_destroy(interp);
}

void test_arithmetic_special_cases(void)
{
    const char* code = "(+) (-) (*) (/)";
    kokos_lexer_t lexer = kokos_lex_buf(code, strlen(code));
    kokos_parser_t parser = kokos_parser_of_lexer(lexer);
    kokos_interp_t* interp = kokos_interp_new(100);

    kokos_obj_t* plus = kokos_parser_next(&parser, interp);
    assert(plus);
    plus = kokos_interp_eval(interp, plus, true);
    assert(plus);
    assert(plus->type == OBJ_INT);
    assert(plus->integer == 0);

    kokos_obj_t* minus = kokos_parser_next(&parser, interp);
    assert(minus);
    minus = kokos_interp_eval(interp, minus, true);
    assert(minus);
    assert(minus->type == OBJ_INT);
    assert(minus->integer == 0);

    kokos_obj_t* star = kokos_parser_next(&parser, interp);
    assert(star);
    star = kokos_interp_eval(interp, star, true);
    assert(star);
    assert(star->type == OBJ_INT);
    assert(star->integer == 1);

    kokos_obj_t* slash = kokos_parser_next(&parser, interp);
    assert(slash);
    slash = kokos_interp_eval(interp, slash, true);
    assert(slash);
    assert(slash->type == OBJ_FLOAT);
    assert(isnan(slash->floating));

    kokos_interp_destroy(interp);
}

int main()
{
    // special forms
    test_def();
    // builtins
    test_arithmetic();
    test_arithmetic_special_cases();
}
