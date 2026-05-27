#ifndef CODEGEN_NASM_H
#define CODEGEN_NASM_H

#include "lexer.h"

typedef enum {
    OUTPUT_FORMAT_NASM,
    OUTPUT_FORMAT_IR,
    OUTPUT_FORMAT_MACHINE
} OutputFormat;

typedef enum {
    TARGET_WIN,
    TARGET_LINUX,
    TARGET_ARM_LINUX,
    TARGET_ARM_ANDROID
} TargetPlatform;

extern FILE *output;
extern int nasmLabelCounter;
extern int nasmStringCounter;
extern int nasmFunctionCounter;
extern TargetPlatform targetPlatform;

void generateNASMCode(ASTNode *node);
void generateNASMProgram(ASTNode *node);
void generateNASMStatement(ASTNode *node);
void generateNASMExpression(ASTNode *node);
void generateNASMBinaryOp(ASTNode *node);
void generateNASMLiteral(ASTNode *node);
void generateNASMIdentifier(ASTNode *node);
void generateNASMFunctionDeclaration(ASTNode *node);
void generateNASMFunctionCall(ASTNode *node);
void generateNASMReturn(ASTNode *node);
void generateNASMForLoop(ASTNode *node);
void generateNASMWhileLoop(ASTNode *node);
void generateNASMIfStatement(ASTNode *node);
void generateNASMOutput(ASTNode *node);
void generateNASMVariableDeclaration(ASTNode *node);
void generateNASMInput(ASTNode *node);
void generateNASMUnaryOp(ASTNode *node);
void generateNASMStructDeclaration(ASTNode *node);
void generateNASMEnumDeclaration(ASTNode *node);
void generateNASMMemberAccess(ASTNode *node);
void generateNASMDereference(ASTNode *node);
void generateNASMAddressOf(ASTNode *node);
void generateNASMAllocCall(ASTNode *node);
void generateNASMFreeCall(ASTNode *node);

// ARM backend functions
void generateARMInput(ASTNode *node);
void generateARMUnaryOp(ASTNode *node);
void generateARMStructDeclaration(ASTNode *node);
void generateARMEnumDeclaration(ASTNode *node);
void generateARMMemberAccess(ASTNode *node);
void generateARMDereference(ASTNode *node);
void generateARMAddressOf(ASTNode *node);
void generateARMAllocCall(ASTNode *node);
void generateARMFreeCall(ASTNode *node);
void generateARMObjectDeclaration(ASTNode *node);
void generateARMObjectMember(ASTNode *node);
void generateARMObjectAccess(ASTNode *node);
void generateNASMObjectDeclaration(ASTNode *node);
void generateNASMObjectMember(ASTNode *node);
void generateNASMObjectAccess(ASTNode *node);

char *nasmNewLabel();
char *nasmNewString();
char *nasmNewFunction();

#endif