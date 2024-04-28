#include "ast.h"
#include "lexer.h"
#include "parser.h"
#include "macros.h"
#include "compile.h"
#include "vm.h"

int main()
{
    char code[] = "(+ 69.420)";

    kokos_lexer_t lexer = kokos_lex_buf(code, sizeof(code));
    kokos_parser_t parser = kokos_parser_init(&lexer);

    kokos_expr_t* expr = kokos_parser_next(&parser);
    KOKOS_VERIFY(expr);

    code_t compiled = kokos_expr_compile(expr);
    kokos_code_dump(compiled);

    vm_t vm = { 0 };
    kokos_vm_run(&vm, compiled);

    kokos_vm_dump(&vm);

    return 0;
}
