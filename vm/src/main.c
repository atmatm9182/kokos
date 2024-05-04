#include "ast.h"
#include "compile.h"
#include "instruction.h"
#include "lexer.h"
#include "parser.h"
#include "vm.h"

#include <stdio.h>
#include <sys/time.h>
#include <time.h>

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

uint64_t get_time_stamp(void)
{
    struct timeval val;
    gettimeofday(&val, NULL);
    return val.tv_usec + val.tv_sec * 1000000;
}

static int run_file(const char* filename)
{
    char* data = read_file(filename);
    KOKOS_VERIFY(data);

    kokos_lexer_t lexer = kokos_lex_named_buf(data, strlen(data), filename);
    kokos_parser_t parser = kokos_parser_init(&lexer);

    uint64_t parser_start = get_time_stamp();
    kokos_program_t program = kokos_parser_program(&parser);
    uint64_t parser_end = get_time_stamp();

    if (!kokos_parser_ok(&parser)) {
        const char* error_msg = kokos_parser_get_err(&parser);
        fprintf(stderr, "Error while parsing the program: %s\n", error_msg);
        return 1;
    }

    printf("program ast:\n");
    printf("--------------------------------------------------\n");
    kokos_program_dump(program);
    printf("--------------------------------------------------\n\n");

    kokos_compiler_context_t compiler_context = kokos_ctx_empty();

    uint64_t compile_start = get_time_stamp();
    kokos_code_t code = kokos_compile_program(program, &compiler_context);
    uint64_t compile_end = get_time_stamp();

    if (!kokos_compile_ok()) {
        const char* error_msg = kokos_compile_get_err();
        fprintf(stderr, "Error while compiling the program: %s\n", error_msg);
        return 1;
    }

    printf("program code:\n");
    printf("--------------------------------------------------\n");
    kokos_code_dump(code);
    printf("--------------------------------------------------\n\n");

    printf("procedure code:\n");
    printf("--------------------------------------------------\n");
    kokos_code_dump(compiler_context.procedure_code);
    printf("--------------------------------------------------\n\n");

    kokos_vm_t vm = kokos_vm_create(&compiler_context);

    uint64_t runtime_start = get_time_stamp();
    kokos_vm_run(&vm, code);
    uint64_t runtime_end = get_time_stamp();

    printf("vm state:\n");
    printf("--------------------------------------------------\n");
    kokos_vm_dump(&vm);
    printf("--------------------------------------------------\n\n");

    printf("parsing took %ld us\n", parser_end - parser_start);
    printf("compiling took %ld us\n", compile_end - compile_start);
    printf("runtime took %ld us\n", runtime_end - runtime_start);

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
