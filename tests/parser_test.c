#define BASE_IMPLEMENTATION
#include "base.h"

#include "interpreter.h"
#include "lexer.h"
#include "obj.h"
#include "parser.h"
#include "token.h"

#include <assert.h>
#include <string.h>

void test_basic(void)
{
    const char* code
        = "\"hello world!\" symbol (+ 3 7) 7.7 [1 2.1 \"string!\"] [] {\"hello\" \"world\"} {}";
    kokos_lexer_t lex = kokos_lex_buf(code, strlen(code));
    kokos_parser_t parser = kokos_parser_of_lexer(lex);
    kokos_interp_t* interp = kokos_interp_new(500);

    kokos_obj_t* string = kokos_parser_next(&parser, interp);
    assert(string);
    assert(string->type == OBJ_STRING);
    assert(strcmp(string->string, "hello world!") == 0);

    kokos_obj_t* symbol = kokos_parser_next(&parser, interp);
    assert(symbol);
    assert(symbol->type == OBJ_SYMBOL);
    assert(strcmp(symbol->symbol, "symbol") == 0);

    kokos_obj_t* list = kokos_parser_next(&parser, interp);
    assert(list);
    assert(list->type == OBJ_LIST);
    assert(list->list.len == 3);
    assert(list->list.objs[0]->type == OBJ_SYMBOL);
    assert(strcmp(list->list.objs[0]->symbol, "+") == 0);
    assert(list->list.objs[1]->type == OBJ_INT);
    assert(list->list.objs[1]->integer == 3);
    assert(list->list.objs[2]->type == OBJ_INT);
    assert(list->list.objs[2]->integer == 7);

    kokos_obj_t* floating = kokos_parser_next(&parser, interp);
    assert(floating);
    assert(floating->type == OBJ_FLOAT);
    assert(floating->floating == 7.7);

    kokos_obj_t* vec = kokos_parser_next(&parser, interp);
    assert(vec);
    assert(vec->type == OBJ_VEC);
    assert(vec->vec.len == 3);
    assert(vec->vec.items[0]->type == OBJ_INT);
    assert(vec->vec.items[0]->integer == 1);
    assert(vec->vec.items[1]->type == OBJ_FLOAT);
    assert(vec->vec.items[1]->floating == 2.1);
    assert(vec->vec.items[2]->type == OBJ_STRING);
    assert(strcmp(vec->vec.items[2]->string, "string!") == 0);

    kokos_obj_t* empty_vec = kokos_parser_next(&parser, interp);
    assert(empty_vec);
    assert(empty_vec->type == OBJ_VEC);
    assert(empty_vec->vec.len == 0);

    kokos_obj_t* map = kokos_parser_next(&parser, interp);
    assert(map);
    assert(map->type == OBJ_MAP);
    assert(map->map.len == 1);

    kokos_obj_t* map_entry_key = kokos_gc_alloc(&interp->gc);
    map_entry_key->type = OBJ_STRING;

    const char* key_raw = "hello";
    char* key_str = strdup(key_raw);
    map_entry_key->string = key_str;

    kokos_obj_t* map_entry_value = ht_find(&map->map, map_entry_key);
    assert(map_entry_value);
    assert(map_entry_value->type == OBJ_STRING);
    assert(strcmp(map_entry_value->string, "world") == 0);

    kokos_obj_t* empty_map = kokos_parser_next(&parser, interp);
    assert(empty_map);
    assert(empty_map->type == OBJ_MAP);
    assert(empty_map->map.len == 0);

    kokos_obj_t* null = kokos_parser_next(&parser, interp);
    assert(!null);
    assert(kokos_p_err == ERR_NONE);

    kokos_interp_destroy(interp);
}

void test_unmatched_delimiters(void)
{
    const char* paren_code = "(";
    kokos_lexer_t lexer = kokos_lex_buf(paren_code, strlen(paren_code));
    kokos_parser_t parser = kokos_parser_of_lexer(lexer);
    kokos_interp_t* interp = kokos_interp_new(500);

    kokos_obj_t* paren = kokos_parser_next(&parser, interp);
    assert(!paren);
    assert(kokos_p_err == ERR_UNMATCHED_DELIMITER);
    assert(kokos_p_err_tok.type == TT_LPAREN);
    assert(sv_eq_cstr(kokos_p_err_tok.value, "("));

    const char* bracket_code = "[123 12.3 symbol!!!";
    lexer = kokos_lex_buf(bracket_code, strlen(bracket_code));
    parser = kokos_parser_of_lexer(lexer);

    kokos_obj_t* bracket = kokos_parser_next(&parser, interp);
    assert(!bracket);
    assert(kokos_p_err == ERR_UNMATCHED_DELIMITER);
    assert(kokos_p_err_tok.type == TT_LBRACKET);
    assert(sv_eq_cstr(kokos_p_err_tok.value, "["));

    const char* brace_code = "{pair 123";
    lexer = kokos_lex_buf(brace_code, strlen(brace_code));
    parser = kokos_parser_of_lexer(lexer);

    kokos_obj_t* brace = kokos_parser_next(&parser, interp);
    assert(!brace);
    assert(kokos_p_err == ERR_UNMATCHED_DELIMITER);
    assert(kokos_p_err_tok.type == TT_LBRACE);
    assert(sv_eq_cstr(kokos_p_err_tok.value, "{"));

    const char* quote_code = "\"";
    lexer = kokos_lex_buf(quote_code, strlen(quote_code));
    parser = kokos_parser_of_lexer(lexer);

    kokos_obj_t* quote = kokos_parser_next(&parser, interp);
    assert(!quote);
    assert(kokos_p_err == ERR_UNMATCHED_DELIMITER);
    assert(kokos_p_err_tok.type == TT_STR_LIT_UNCLOSED);
    assert(sv_eq_cstr(kokos_p_err_tok.value, "\""));

    kokos_interp_destroy(interp);
}

int main()
{
    test_basic();
    test_unmatched_delimiters();
}
