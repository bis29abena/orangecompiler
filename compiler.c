#include "compiler.h"
#include "stdarg.h"
#include "stdlib.h"

struct lex_process_functions compiler_lex_functions =
{
    .next_char = compile_process_next_char,
    .peek_char = compile_process_peek_char,
    .push_char = compile_process_push_char
};

void compiler_error(struct compile_process* process, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);

    fprintf(stderr, "Error at %s:%d:%d\n", process->cfile.abs_path, process->pos.line, process->pos.col);

    exit(-1);
}

void compiler_warning(struct compile_process* process, const char* message, ...)
{
    va_list args;
    va_start(args, message);
    vfprintf(stderr, message, args);
    va_end(args);

    fprintf(stderr, "Warning at %s:%d:%d\n", process->cfile.abs_path, process->pos.line, process->pos.col);
}

int compile_file(const char *filename, const char* out_filename, int flags)
{
    struct compile_process* process = compile_process_create(filename, out_filename, flags);
    if (!process)
    {
        return COMPILER_FAILED_WITH_ERRORS;;
    }

    // Perform lexical analysis
    struct lex_process* lex_process = lex_process_create(process, &compiler_lex_functions, NULL);


    if(!lex_process)
    {
        return COMPILER_FAILED_WITH_ERRORS;
    }

    if(lex(lex_process) != LEXICAL_ANALYSIS_ALL_OK)
    {
        return COMPILER_FAILED_WITH_ERRORS;

    }

    process->token_vec = lex_process->token_vec;
    
    //Perform parsing
    if(parse(process) != PARSE_ALL_OK)
    {
        return COMPILER_FAILED_WITH_ERRORS;
    }

    //Perform code generation

    return COMPILER_FILE_COMPILED_SUCCESSFULLY;
}