#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include "parser.h"

// 优化级别
typedef enum {
    OPT_LEVEL_NONE = 0,
    OPT_LEVEL_BASIC = 1,
    OPT_LEVEL_FULL = 2
} OptimizationLevel;

extern OptimizationLevel optLevel;

// 优化函数
ASTNode *optimizeAST(ASTNode *node);
ASTNode *constantFolding(ASTNode *node);
ASTNode *deadCodeElimination(ASTNode *node);
ASTNode *commonSubexpressionElimination(ASTNode *node);

// 表达式求值
int evaluateConstantExpression(ASTNode *node, long long *result);
int isConstantExpression(ASTNode *node);

#endif
