#include "util.h"

#include <stdio.h>
#include <stdlib.h>

char* read_whole_file(const char* filepath)
{
    FILE* f = fopen(filepath, "rb");
    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = malloc(sizeof(char) * (fsize + 1));
    fread(buf, sizeof(char), fsize, f);
    buf[fsize] = '\0';

    fclose(f);
    return buf;
}
