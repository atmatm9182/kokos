#include "ast.h"
#include "compile.h"
#include "instruction.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"

int main()
{
    char code[] = "(if false (+ 1 2 3) (+ 4 5 6))";

    kokos_lexer_t lexer = kokos_lex_buf(code, sizeof(code));
    kokos_parser_t parser = kokos_parser_init(&lexer);

    kokos_program_t prog = kokos_parser_program(&parser);

    kokos_compiler_context_t ctx = kokos_empty_compiler_context();
    kokos_code_t compiled = kokos_compile_program(prog, &ctx);
    printf("program code: \n");
    kokos_code_dump(compiled);
    printf("--------------------------------------------------\n");

    kokos_vm_t vm = { 0 };
    kokos_vm_run(&vm, compiled, &ctx);

    kokos_vm_dump(&vm);

    return 0;
}
