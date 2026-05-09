#ifndef LEXER_H
#define LEXER_H

#include <stdio.h>

typedef struct Preprocessor_ Preprocessor;

typedef enum {
    TOKEN_EOF,
    TOKEN_ID,
    TOKEN_INT,
    TOKEN_STRING,
    TOKEN_CHAR,
    TOKEN_NUMBER,
    TOKEN_FLOAT_NUMBER,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MUL,
    TOKEN_DIV,
    TOKEN_ASSIGN,
    TOKEN_EQ,
    TOKEN_NE,
    TOKEN_LT,
    TOKEN_LE,
    TOKEN_GT,
    TOKEN_GE,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_IMPORT,
    TOKEN_NAMESPACE,
    TOKEN_OUT,
    TOKEN_IN,
    TOKEN_FUNC,
    TOKEN_RETURN,
    TOKEN_FOR,
    TOKEN_WHILE,
    TOKEN_SWITCH,
    TOKEN_CASE,
    TOKEN_DEFAULT,
    TOKEN_BREAK,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_FLOAT,
    TOKEN_DOUBLE,
    TOKEN_STRING_TYPE,
    TOKEN_CHAR_TYPE,
    TOKEN_DIR,
    TOKEN_ENDL,
    TOKEN_STRING_CONCAT,
    TOKEN_ARROW,
    TOKEN_DOUBLE_ARROW,
    TOKEN_DOT,
    TOKEN_MEM,
    TOKEN_PO,
    TOKEN_ALLOC,
    TOKEN_FREE,
    TOKEN_AMPERSAND,
    TOKEN_ELLIPSIS,
    TOKEN_RUN,
    TOKEN_ASM,
    TOKEN_MACRO,
    TOKEN_HASH,
    TOKEN_INCLUDE,
    TOKEN_DEFINES,
    TOKEN_UNDEF,
    TOKEN_SECONDARY_MACRO,
    TOKEN_SECONDARY_KEYWORD,
    TOKEN_SECONDARY_OPERATOR,
    TOKEN_OBJECT
} TokenType;

typedef struct {
    TokenType type;
    char value[100];
    int line;
    int column;
} Token;

extern FILE *input;
extern int line;
extern int column;

Token getNextToken();
void setLexerPreprocessor(Preprocessor *pp);
void initLexer(FILE *newInput);
void skipWhitespace();
Token readIdentifier();
Token readNumber();
Token readString();
Token readChar();

#endif