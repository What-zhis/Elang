#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"

typedef enum {
    AST_PROGRAM,
    AST_STATEMENT,
    AST_EXPRESSION,
    AST_VARIABLE_DECLARATION,
    AST_FUNCTION_DECLARATION,
    AST_FUNCTION_CALL,
    AST_RETURN,
    AST_FOR,
    AST_WHILE,
    AST_IF,
    AST_SWITCH,
    AST_BREAK,
    AST_CASE,
    AST_OUTPUT,
    AST_INPUT,
    AST_LITERAL,
    AST_IDENTIFIER,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_STRING_CONCAT,
    AST_ASSIGNMENT,
    AST_IMPORT,
    AST_STRUCT_DECLARATION,
    AST_ENUM_DECLARATION,
    AST_STRUCT_MEMBER,
    AST_ENUM_MEMBER,
    AST_MEMBER_ACCESS,
    AST_POINTER_DECLARATION,
    AST_MEMORY_DECLARATION,
    AST_DEREFERENCE,
    AST_ADDRESS_OF,
    AST_POINTER_OP,
    AST_ALLOC_CALL,
    AST_FREE_CALL,
    AST_RUN_CALL,
    AST_ASM,
    AST_MACRO_DEFINITION,
    AST_MACRO_INVOCATION,
    AST_MACRO_EXPANSION,
    AST_SECONDARY_MACRO_INVOCATION,
    AST_SECONDARY_MACRO_DEFINITION,
    AST_OBJECT_DECLARATION,
    AST_OBJECT_MEMBER,
    AST_OBJECT_ACCESS
} ASTNodeType;

typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_STRUCT,
    SYMBOL_ENUM,
    SYMBOL_POINTER,
    SYMBOL_MEMORY
} SymbolType;

typedef struct SymbolEntry {
    char name[100];
    SymbolType type;
    char varType[20];
    int isMemoryOwner;
    int isFreed;
    struct SymbolEntry *next;
} SymbolEntry;

typedef struct SymbolTable {
    SymbolEntry *entries;
    struct SymbolTable *parent;
} SymbolTable;

typedef struct ASTNode {
    ASTNodeType type;
    Token token;
    struct ASTNode *left;
    struct ASTNode *right;
    struct ASTNode *next;
    struct ASTNode *body;
    struct ASTNode *params;
    struct ASTNode *condition;
    struct ASTNode *init;
    struct ASTNode *update;
    struct ASTNode *elseBody;
    struct ASTNode *elseIfChain;
    struct ASTNode *namespaceExpr;
    char namespace[100];
    char filename[100];
} ASTNode;

typedef struct ImportNode {
    char filename[100];
    struct ImportNode *next;
} ImportNode;

// AST 内存池分配器
#define AST_POOL_BLOCK_SIZE 1024  // 每个块包含的节点数

typedef struct ASTPoolBlock {
    struct ASTPoolBlock *next;
    ASTNode nodes[AST_POOL_BLOCK_SIZE];
    int used;
} ASTPoolBlock;

typedef struct {
    ASTPoolBlock *first;
    ASTPoolBlock *current;
    int totalAllocated;
    int totalNodes;
} ASTPool;

extern ASTPool astPool;

// 内存池相关函数
void initASTPool();
void cleanupASTPool();
ASTNode *allocASTNode();

extern Token currentToken;
extern ImportNode *importList;
extern ImportNode *importTail;

ASTNode *parseProgram();
ASTNode *parseStatement();
ASTNode *parseExpression();
ASTNode *parseTerm();
ASTNode *parseFactor();
ASTNode *parsePrimary();
ASTNode *createNode(ASTNodeType type, Token token);
void expect(TokenType type, const char *message);
void addImport(char *filename);

// 符号表相关函数
SymbolTable *createSymbolTable(SymbolTable *parent);
void addSymbol(SymbolTable *table, const char *name, SymbolType type, const char *varType);
SymbolEntry *lookupSymbol(SymbolTable *table, const char *name);
void freeSymbolTable(SymbolTable *table);

// 内存管理相关函数
void freeAST(ASTNode *node);
void freeSingleASTNode(ASTNode *node);
void freeASTList(ASTNode *head);
void cleanupParser();
void cleanupUnreturnedAST(ASTNode *node);

void generateCode(ASTNode *node);
void generateProgram(ASTNode *node);
void generateStatement(ASTNode *node);
void generateExpression(ASTNode *node);
void generateBinaryOp(ASTNode *node);
void generateUnaryOp(ASTNode *node);
void generateLiteral(ASTNode *node);
void generateIdentifier(ASTNode *node);
void generateFunctionDeclaration(ASTNode *node);
void generateFunctionCall(ASTNode *node);
void generateReturn(ASTNode *node);
void generateForLoop(ASTNode *node);
void generateWhileLoop(ASTNode *node);
void generateIfStatement(ASTNode *node);
void generateOutput(ASTNode *node);
void generateInput(ASTNode *node);
void generateVariableDeclaration(ASTNode *node);
void generateImport(ASTNode *node);
void generatePointerDeclaration(ASTNode *node);
void generateMemoryDeclaration(ASTNode *node);
void generateDereference(ASTNode *node);
void generateAddressOf(ASTNode *node);
void generatePointerOp(ASTNode *node);
void generateAllocCall(ASTNode *node);
void generateFreeCall(ASTNode *node);
void generateRunCall(ASTNode *node);

extern char **importedFiles;
extern int importedFileCount;
void addImport(char *filename);

#endif