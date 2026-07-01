
#include <stdio.h>
#include "helpers/vector.h"
#include "compiler.h"

int main()
{

    int res = compile_file("test/test.c", "test/test.o", 0);

    if (res == COMPILER_FAILED_WITH_ERRORS)
    {
        printf("Compilation failed with errors\n");
    }
    else if (res == COMPILER_FILE_COMPILED_SUCCESSFULLY)
    {
        printf("Compilation succeeded\n");
    }
    else
    {
        printf("Unknown result from compile_file: %d\n", res);
    }
    return 0;
}