#include "ast.h"
#include "compile.h"
#include "instruction.h"
#include "lexer.h"
#include "parser.h"
#include "src/runtime.h"
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

    char* data = KOKOS_CALLOC(sizeof(char), fsize + 1);
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
    kokos_module_t module = kokos_parser_parse_module(&parser);
    uint64_t parser_end = get_time_stamp();

    if (!kokos_parser_ok(&parser)) {
        const char* error_msg = kokos_parser_get_err(&parser);
        fprintf(stderr, "Error while parsing the module: %s\n", error_msg);
        return 1;
    }

    printf("module ast:\n");
    printf("--------------------------------------------------\n");
    kokos_module_dump(module);
    printf("--------------------------------------------------\n\n");

    kokos_scope_t* global_scope = kokos_scope_root();
    kokos_compiled_module_t compiled_module;

    uint64_t compile_start = get_time_stamp();
    bool ok = kokos_compile_module(module, global_scope, &compiled_module);
    uint64_t compile_end = get_time_stamp();

    if (!ok) {
        const char* error_msg = kokos_compile_get_err();
        fprintf(stderr, "Error while compiling the module: %s\n", error_msg);
        return 1;
    }

    printf("module code:\n");
    printf("--------------------------------------------------\n");
    kokos_code_dump(compiled_module.instructions);
    printf("--------------------------------------------------\n\n");

    printf("procedure code:\n");
    printf("--------------------------------------------------\n");
    HT_ITER(compiled_module.procs, {
        kokos_runtime_proc_t* proc = GET_PROC_PTR(kv.value);

        if (proc->type == PROC_NATIVE) {
            continue;
        }

        kokos_runtime_string_t* name = GET_STRING_PTR(kv.key);
        printf(RT_STRING_FMT ":\n", RT_STRING_ARG(*name));

        KOKOS_ASSERT(proc->type == PROC_KOKOS);

        kokos_code_dump(proc->kokos.code);
    });
    printf("--------------------------------------------------\n\n");

    kokos_vm_t* vm = kokos_vm_create(global_scope);

    uint64_t runtime_start = get_time_stamp();
    kokos_vm_load_module(vm, &compiled_module); // loading the module also runs it's code
    uint64_t runtime_end = get_time_stamp();

    printf("vm state:\n");
    printf("--------------------------------------------------\n");
    kokos_vm_dump(vm);
    printf("--------------------------------------------------\n\n");

    printf("parsing took %ld us\n", parser_end - parser_start);
    printf("compiling took %ld us\n", compile_end - compile_start);
    printf("runtime took %ld us\n", runtime_end - runtime_start);

    KOKOS_FREE(data);
    kokos_module_destroy(module);
    kokos_scope_destroy(global_scope);
    kokos_vm_destroy(vm);

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
