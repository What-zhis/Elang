#ifndef CODEGEN_H
#define CODEGEN_H

#include "lexer.h"
#include "parser.h"

extern FILE *output;
extern int indentLevel;

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
void indent();

#endif