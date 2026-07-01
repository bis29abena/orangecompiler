#include "compiler.h"
#include "helpers/vector.h"
#include <stdlib.h>

struct lex_process* lex_process_create(struct compile_process* compiler, struct lex_process_functions* functions, void* private)
{
    struct lex_process* process = calloc(1, sizeof(struct lex_process));
    process->compiler = compiler;
    process->token_vec = vector_create(sizeof(struct token));
    process->functions = functions;
    process->user_data = user_data;
    return process;
}