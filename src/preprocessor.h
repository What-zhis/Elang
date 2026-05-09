#ifndef PREPROCESSOR_H
#define PREPROCESSOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MACRO_PARAMS 16
#define MAX_MACRO_STACK 64
#define MAX_MACRO_BODY 4096
#define MAX_SECONDARY_MACROS 64

typedef enum {
    MACRO_CONSTANT,
    MACRO_FUNCTION,
    MACRO_TYPE
} MacroType;

typedef enum {
    SECONDARY_KEYWORD,
    SECONDARY_FUNCTION,
    SECONDARY_OPERATOR,
    SECONDARY_SYNTAX,
    SECONDARY_TYPEDEF
} SecondaryMacroType;

typedef struct MacroParam {
    char name[64];
    char defaultValue[256];
    int hasDefault;
} MacroParam;

typedef struct {
    char name[64];
    char type[32];
} SecondaryField;

typedef struct SecondaryMacro {
    char name[128];
    SecondaryMacroType type;
    char pattern[512];
    int precedence;
    int associativity;
    char execBody[4096];
    int paramCount;
    MacroParam params[MAX_MACRO_PARAMS];
    int fieldCount;
    SecondaryField fields[MAX_MACRO_PARAMS];
    int isActive;
} SecondaryMacro;

typedef struct Macro {
    char name[128];
    MacroType type;
    int paramCount;
    MacroParam params[MAX_MACRO_PARAMS];
    char body[MAX_MACRO_BODY];
    int isActive;
} Macro;

typedef struct MacroScope {
    Macro macros[256];
    int macroCount;
    struct MacroScope *parent;
} MacroScope;

typedef struct Preprocessor_ {
    MacroScope *globalScope;
    MacroScope *currentScope;
    int ifdefStack[64];
    int ifdefDepth;
    int skipBlock;
    int elseDepth;
    
    SecondaryMacro secondaryMacros[MAX_SECONDARY_MACROS];
    int secondaryMacroCount;
} Preprocessor;

void initPreprocessor(Preprocessor *pp);
void freePreprocessor(Preprocessor *pp);
char *preprocess(Preprocessor *pp, const char *filename, const char *content);

int addMacro(Preprocessor *pp, const char *name, MacroType type, const char *body);
int addMacroWithParams(Preprocessor *pp, const char *name, MacroType type,
                       int paramCount, MacroParam *params, const char *body);
Macro *findMacro(Preprocessor *pp, const char *name);
void undefMacro(Preprocessor *pp, const char *name);

void pushScope(Preprocessor *pp);
void popScope(Preprocessor *pp);

char *expandMacros(Preprocessor *pp, const char *input);

char *stringify(Preprocessor *pp, const char *input);
char *concatenate(Preprocessor *pp, const char *left, const char *right);

int addSecondaryMacro(Preprocessor *pp, SecondaryMacro *sm);
SecondaryMacro *findSecondaryMacro(Preprocessor *pp, const char *name, SecondaryMacroType type);

#endif
