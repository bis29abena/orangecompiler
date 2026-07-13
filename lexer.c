#include "compiler.h"
#include <string.h>
#include "helpers/vector.h"
#include "helpers/buffer.h"
#include <assert.h>
#include <ctype.h>

#define LEX_GETC_IF(buffer, c, exp)     \
    for (c = peekc(); exp; c = peekc()) \
    {                                   \
        buffer_write(buffer, c);        \
        nextc();                        \
    }

struct token *read_next_token();
static struct lex_process *lex_process;
static struct token temp_token;

static char peekc()
{
    return lex_process->function->peek_char(lex_process);
}

static char nextc()
{
    char c = lex_process->function->next_char(lex_process);
    lex_process->pos.col += 1;
    if (c == '\n')
    {
        lex_process->pos.line += 1;
        lex_process->pos.col = 1;
    }
    return c;
}

static struct pos lex_file_position()
{
    return lex_process->pos;
}

static void pushc(char c)
{
    lex_process->function->push_char(lex_process, c);
}

static char assert_next_char(char c)
{
    char next_c = nextc();
    assert(next_c == c);
    return next_c;
}

struct token *token_create(struct token *_token)
{
    memcpy(&temp_token, _token, sizeof(struct token));
    temp_token.pos = lex_file_position();
    return &temp_token;
}

static struct token *lexer_last_token()
{
    return vector_back_or_null(lex_process->token_vec);
}

static struct token *handle_whitespace()
{
    struct token *last_token = lexer_last_token();
    if (last_token)
    {
        last_token->whitespace = true;
    }
    nextc();
    return read_next_token();
}

const char *read_number_str()
{
    const char *num = NULL;
    struct buffer *buffer = buffer_create();
    char c = peekc();
    LEX_GETC_IF(buffer, c, (c >= '0' && c <= '9'));

    buffer_write(buffer, 0x00);
    return buffer_ptr(buffer);
}

struct token *token_make_number_for_value(unsigned long number)
{
    return token_create(&(struct token){
        .type = TOKEN_TYPE_NUMBER,
        .llnum = number});
}

unsigned long long read_number()
{
    const char *s = read_number_str();
    return atoll(s);
}

struct token *token_make_number()
{
    return token_make_number_for_value(read_number());
}

static struct token *token_make_string(char start_delimter, char end_delimiter)
{
    struct buffer *buf = buffer_create();

    assert(nextc() == start_delimter);

    char c = nextc();

    for (; c != end_delimiter && c != EOF; c = nextc())
    {
        if (c == '\\')
        {
            // we need to handle escape characters
            continue;
        }

        buffer_write(buf, c);
    }

    buffer_write(buf, 0x00);
    return token_create(&(struct token){
        .type = TOKEN_TYPE_STRING,
        .sval = buffer_ptr(buf)});
}

static bool op_treated_as_one(char op)
{
    return op == '(' || op == '[' || op == ',' || op == '.' || op == '?';
}

static bool is_single_operator(char op)
{
    return op == '+' || op == '-' || op == '/' || op == '*' || op == '=' || op == '<' || op == '>' || op == '!' || op == '&' || op == '|' || op == '^' || op == '~' || op == '%' || op == '(' || op == '[' || op == ',' || op == '.' || op == '?';
}

bool op_valid(const char *op)
{
    return S_EQ(op, "+") ||
           S_EQ(op, "-") ||
           S_EQ(op, "*") ||
           S_EQ(op, "/") ||
           S_EQ(op, "!") ||
           S_EQ(op, "^") ||
           S_EQ(op, "+=") ||
           S_EQ(op, "-=") ||
           S_EQ(op, "*=") ||
           S_EQ(op, "/=") ||
           S_EQ(op, ">>") ||
           S_EQ(op, "<<") ||
           S_EQ(op, ">=") ||
           S_EQ(op, "<=") ||
           S_EQ(op, ">") ||
           S_EQ(op, "<") ||
           S_EQ(op, "||") ||
           S_EQ(op, "&&") ||
           S_EQ(op, "|") ||
           S_EQ(op, "&") ||
           S_EQ(op, "++") ||
           S_EQ(op, "--") ||
           S_EQ(op, "=") ||
           S_EQ(op, "!=") ||
           S_EQ(op, "==") ||
           S_EQ(op, "->") ||
           S_EQ(op, "(") ||
           S_EQ(op, "[") ||
           S_EQ(op, ",") ||
           S_EQ(op, ".") ||
           S_EQ(op, "...") ||
           S_EQ(op, "~") ||
           S_EQ(op, "?") ||
           S_EQ(op, "%");
}

void read_op_flush_back_keep_first(struct buffer *buf)
{
    const char *data = buffer_ptr(buf);
    int len = buf->len;
    for (int i = len - 1; i >= 1; i--)
    {
        if (data[i] != 0x00)
        {
            continue;
        }

        pushc(data[i]);
    }
}

const char *read_op()
{
    bool single_operator = true;
    char op = nextc();
    struct buffer *buf = buffer_create();
    buffer_write(buf, op);

    if (!op_treated_as_one(op))
    {
        op = peekc();
        if (is_single_operator(op))
        {
            buffer_write(buf, op);
            nextc();
            single_operator = false;
        }
    }
    // NULL TERMINATOR
    buffer_write(buf, 0x00);
    char *ptr = buffer_ptr(buf);

    if (!single_operator)
    {
        if (!op_valid(ptr))
        {
            read_op_flush_back_keep_first(buf);
            ptr[1] = 0x00;
        }
    }
    else if (!op_valid(ptr))
    {
        compiler_error(lex_process->compiler, "Invalid operator %s\n", ptr);
    }

    return ptr;
}

static void lex_new_expression()
{
    lex_process->current_expression_count++;
    if (lex_process->current_expression_count == 1)
    {
        lex_process->parenthesis_buffer = buffer_create();
    }
}

static void lex_finish_expression()
{
    lex_process->current_expression_count--;
    if (lex_process->current_expression_count < 0)
    {
        compiler_error(lex_process->compiler, "You closed an expression that was not opened\n");
    }
}

bool lex_in_expression()
{
    return lex_process->current_expression_count > 0;
}

bool is_keyword(const char *str)
{
    return S_EQ(str, "unsigned") ||
           S_EQ(str, "signed") ||
           S_EQ(str, "char") ||
           S_EQ(str, "int") ||
           S_EQ(str, "short") ||
           S_EQ(str, "float") ||
           S_EQ(str, "double") ||
           S_EQ(str, "long") ||
           S_EQ(str, "void") ||
           S_EQ(str, "struct") ||
           S_EQ(str, "union") ||
           S_EQ(str, "static") ||
           S_EQ(str, "__ignore_typecheck") ||
           S_EQ(str, "return") ||
           S_EQ(str, "include") ||
           S_EQ(str, "sizeof") ||
           S_EQ(str, "if") ||
           S_EQ(str, "else") ||
           S_EQ(str, "for") ||
           S_EQ(str, "while") ||
           S_EQ(str, "do") ||
           S_EQ(str, "break") ||
           S_EQ(str, "continue") ||
           S_EQ(str, "switch") ||
           S_EQ(str, "case") ||
           S_EQ(str, "default") ||
           S_EQ(str, "goto") ||
           S_EQ(str, "typedef") ||
           S_EQ(str, "const") ||
           S_EQ(str, "extern") ||
           S_EQ(str, "restrict");
}



static struct token *token_make_operator_or_string()
{
    char op = peekc();
    if (op == '<')
    {
        struct token *last_token = lexer_last_token();
        if (token_is_keyword(last_token, "include"))
        {
            return token_make_string('<', '>');
        }
    }

    struct token *token = token_create(&(struct token){
        .type = TOKEN_TYPE_OPERATOR,
        .sval = read_op()});

    if (op == '(')
    {
        lex_new_expression();
    }

    return token;
}

struct token* token_make_online_comment()
{
    struct buffer *buf = buffer_create();
    char c = 0;
    
    LEX_GETC_IF(buf, c, c != '\n' && c != EOF);

    return token_create(&(struct token){
        .type = TOKEN_TYPE_COMMENT,
        .sval = buffer_ptr(buf)});
}

struct token* token_make_multiline_comment()
{
    struct buffer *buf = buffer_create();
    char c = 0;

    while(1){
        LEX_GETC_IF(buf, c, c != '*' && c != EOF);

        if(c == EOF)
        {
            compiler_error(lex_process->compiler, "Unexpected end of file while reading multiline comment\n");
        }

        else if(c == '*')
        {
            nextc(); // consume '*'

            char next = peekc();
            if(next == '/')
            {
                nextc(); // consume '/'
                break;
            }
        }
    }

    return token_create(&(struct token){
        .type = TOKEN_TYPE_COMMENT,
        .sval = buffer_ptr(buf)});
}

struct token* handle_comment()
{
    char c = peekc();
    if(c == '/')
    {
        nextc(); // consume '/'
        char next = peekc();
        if(next == '/')
        {
            nextc(); // consume '/'
            return token_make_online_comment();
        }
        else if(next == '*')
        {
            nextc(); // consume '*'
            return token_make_multiline_comment();
        }

        pushc('/'); // push back the first '/' since it is not a comment
        return token_make_operator_or_string();
    }
    return NULL;
}

static struct token *token_make_symbol()
{
    char c = nextc();
    if (c == ')')
    {
        lex_finish_expression();
    }

    struct token *token = token_create(&(struct token){
        .type = TOKEN_TYPE_SYMBOL,
        .cval = c});

    return token;
}

static struct token* token_make_identifier_or_keyword()
{
    struct buffer *buf = buffer_create();
    char c = 0;
    LEX_GETC_IF(buf, c, (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_');

    // null terminator
    buffer_write(buf, 0x00);

    // we have a valid identifier, now we need to check if it is a keyword
    if(is_keyword(buffer_ptr(buf)))
    {
        struct token *token = token_create(&(struct token){
            .type = TOKEN_TYPE_KEYWORD,
            .sval = buffer_ptr(buf)});

        return token;
    }

    struct token *token = token_create(&(struct token){
        .type = TOKEN_TYPE_IDENTIFIER,
        .sval = buffer_ptr(buf)});

    return token;
}

struct token* read_special_token()
{
    char c = peekc();

    if(isalpha(c) || c == '_')
    {
        return token_make_identifier_or_keyword();
    }

    return NULL;
}

struct token* token_make_newline(){
    nextc();
    return token_create(&(struct token){
        .type = TOKEN_TYPE_NEWLINE});
}

char lex_get_escaped_char(char c)
{
    char co = 0;
    switch (c)
    {
    case 'n':
        co = '\n'; break;
    case 't':
        co = '\t'; break;
    case '\\':
        co = '\\'; break;
    case '\'':
        co = '\''; break;
    default:
        compiler_error(lex_process->compiler, "Invalid escape character: \\%c\n", c);
        return 0;
    }

    return co;
}

void lexer_pop_token()
{
    vector_pop(lex_process->token_vec);
}

bool is_hex_char(char c)
{
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

const char* read_hex_number_str()
{
    struct buffer *buf = buffer_create();
    char c = peekc();
    LEX_GETC_IF(buf, c, is_hex_char(c));
    //write our null terminator
    buffer_write(buf, 0x00);
    return buffer_ptr(buf);
}

void lexer_validate_binary_string(const char* str)
{
    size_t len = strlen(str);
    for(size_t i = 0; i < len; i++)
    {
        if(str[i] != '0' && str[i] != '1')
        {
            compiler_error(lex_process->compiler, "Invalid binary number: %s\n", str);
        }
    }

}

struct token* token_make_special_number_hexadecimal()
{
    // Skip the 'x' prefix
    nextc();

    unsigned long number = 0;
    const char *num_str = read_number_str();
    number = strtol(num_str, 0, 16);
    return token_make_number_for_value(number);
}

struct token* token_make_special_number_binary()
{
    // Skip the 'b' prefix
    nextc();

    unsigned long number = 0;
    const char *num_str = read_number_str();
    lexer_validate_binary_string(num_str);
    number = strtol(num_str, 0, 2);
    return token_make_number_for_value(number);
}

struct token* token_make_special_number()
{
    struct token* token = NULL;

    struct token* last_token = lexer_last_token();

    if(!last_token || !(last_token->type != TOKEN_TYPE_NUMBER && last_token->llnum == 0))
    {
        return token_make_identifier_or_keyword();
    }

    lexer_pop_token();
    char c = peekc();

    if(c == 'x')
    {
        token = token_make_special_number_hexadecimal();
    }
    else if(c == 'b')
    {
        token = token_make_special_number_binary();
    }

    return token;
}

struct token* token_make_quote()
{
    assert_next_char('\'');

    char c = nextc();
    if(c == '\\')
    {
        c = nextc();

        c = lex_get_escaped_char(c);
    }

    if(nextc() != '\'')
    {
        compiler_error(lex_process->compiler, "Invalid character literal\n");
    }

    return token_create(&(struct token){
        .type = TOKEN_TYPE_NUMBER,
        .cval = c});
}

struct token *read_next_token()
{
    struct token *token = NULL;
    char c = peekc();

    token = handle_comment();
    if(token)
    {
        return token;
    }

    switch (c)
    {
    NUMERIC_CASE:

        token = token_make_number();

        break;

    OPERATOR_CASE_EXCLUDING_DIVISION:
        token = token_make_operator_or_string();
        break;

    SYMBOL_CASE:
        token = token_make_symbol();
        break;

    case 'b':
    case 'x':
        token = token_make_special_number();
        break;

    case '"':
        token = token_make_string('"', '"');
        break;

    case '\'':
        token = token_make_quote();
        break;

    // Handle whitespace
    // i
    case ' ':
    case '\t':
        token = handle_whitespace();
        break;

    case '\n':
        token = token_make_newline();
        break;
        
    case EOF:
        // we have finished lexical analysis on the file
        break;

    default:
        token = read_special_token();
        if(!token)
        {
            compiler_error(lex_process->compiler, "Unexpected token \n");
        }
    }

    return token;
}

int lex(struct lex_process *process)
{
    process->current_expression_count = 0;
    process->parenthesis_buffer = NULL;
    lex_process = process;
    process->pos.filename = process->compiler->cfile.abs_path;

    struct token *token = read_next_token();

    while (token)
    {
        vector_push(process->token_vec, token);
        token = read_next_token();
    }

    return LEXICAL_ANALYSIS_ALL_OK;
}