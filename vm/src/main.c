#include "ast.h"
#include "compile.h"
#include "instruction.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"

int main()
{
    // char code[] = "(if false (+ 1 2 3) (+ 4 5 6))";
    char code[] = "(proc fib (x) (if (< x 3) x (+ (fib (- x 1)) (fib (- x 2))))) (fib 25)";

    kokos_lexer_t lexer = kokos_lex_buf(code, sizeof(code));
    kokos_parser_t parser = kokos_parser_init(&lexer);

    kokos_program_t prog = kokos_parser_program(&parser);
    KOKOS_VERIFY(kokos_parser_ok(&parser));

    kokos_compiler_context_t ctx = kokos_empty_compiler_context();
    kokos_code_t compiled = kokos_compile_program(prog, &ctx);

    kokos_compiled_proc_t* fib = kokos_ctx_get_proc(&ctx, "fib");
    KOKOS_VERIFY(fib);
    printf("--------------------------------------------------\n");
    kokos_code_dump(fib->body);
    printf("--------------------------------------------------\n");

    printf("program code: \n");
    kokos_code_dump(compiled);
    printf("--------------------------------------------------\n");

    kokos_vm_t vm = { 0 };
    kokos_vm_run(&vm, compiled, &ctx);

    kokos_vm_dump(&vm);

    return 0;
}
