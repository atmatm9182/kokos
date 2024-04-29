#include "ast.h"
#include "compile.h"
#include "instruction.h"
#include "lexer.h"
#include "macros.h"
#include "parser.h"
#include "vm.h"

int main()
{
    char code[] = "(proc add (x y) (add x y))\n(add 1 2)";

    kokos_lexer_t lexer = kokos_lex_buf(code, sizeof(code));
    kokos_parser_t parser = kokos_parser_init(&lexer);

    kokos_program_t prog = kokos_parser_program(&parser);

    kokos_compiler_context_t ctx = kokos_empty_compiler_context();
    code_t compiled = kokos_compile_program(prog, &ctx);
    printf("program code: \n");
    kokos_code_dump(compiled);
    printf("--------------------------------------------------\n");

    kokos_compiled_proc_t* proc = kokos_ctx_get_proc(&ctx, "add");
    KOKOS_VERIFY(proc);

    kokos_code_dump(proc->body);

    kokos_vm_t vm = { 0 };
    kokos_vm_run(&vm, compiled, &ctx);

    kokos_vm_dump(&vm);

    return 0;
}
