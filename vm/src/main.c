#include "ast.h"
#include "compile.h"
#include "instruction.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"
#include <stdio.h>

char* read_file(const char* filename)
{
    FILE* f = fopen(filename, "r");
    if (!f) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    rewind(f);

    char* data = KOKOS_ALLOC(sizeof(char) * (fsize + 1));
    fread(data, sizeof(char), fsize, f);
    data[fsize] = '\0';

    return data;
}

static int run_file(const char* filename)
{
    char* data = read_file(filename);
    KOKOS_VERIFY(data);

    kokos_lexer_t lexer = kokos_lex_buf(data, strlen(data));
    kokos_parser_t parser = kokos_parser_init(&lexer);
    kokos_program_t program = kokos_parser_program(&parser);

    printf("--------------------------------------------------\n");
    kokos_program_dump(program);
    printf("--------------------------------------------------\n");

    if (!kokos_parser_ok(&parser)) {
        const char* error_msg = kokos_parser_get_err(&parser);
        fprintf(stderr, "Error while parsing the program: %s\n", error_msg);
        return 1;
    }

    kokos_compiler_context_t compiler_context = kokos_ctx_empty();
    kokos_code_t code = kokos_compile_program(program, &compiler_context);
    if (!kokos_compile_ok()) {
        const char* error_msg = kokos_compile_get_err();
        fprintf(stderr, "Error while compiling the program: %s\n", error_msg);
        return 1;
    }

    printf("--------------------------------------------------\n");
    kokos_code_dump(code);
    printf("--------------------------------------------------\n");
    kokos_code_dump(compiler_context.procedure_code);
    printf("--------------------------------------------------\n");

    kokos_vm_t vm = kokos_vm_create(&compiler_context);
    kokos_vm_run(&vm, code);

    kokos_vm_dump(&vm);

    return 0;
}

int main(int argc, char* argv[])
{
    if (argc > 1) {
        return run_file(argv[1]);
    }

    fprintf(stderr, "ERROR: not enough arguments\n");
    return 1;
}
