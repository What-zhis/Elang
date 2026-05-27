#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "optimizer.h"
#include "parser.h"
#include "lexer.h"

OptimizationLevel optLevel = OPT_LEVEL_BASIC;

static ASTNode *optimizeNode(ASTNode *node);
static ASTNode *optimizeBinaryOp(ASTNode *node);
static ASTNode *optimizeStatementList(ASTNode *node);
static int isAlwaysFalse(ASTNode *node);
static int isAlwaysTrue(ASTNode *node);

int isConstantExpression(ASTNode *node) {
    if (!node) return 0;
    
    switch (node->type) {
        case AST_LITERAL:
            return 1;
        case AST_BINARY_OP:
            return isConstantExpression(node->left) && isConstantExpression(node->right);
        case AST_UNARY_OP:
            return isConstantExpression(node->left);
        default:
            return 0;
    }
}

int evaluateConstantExpression(ASTNode *node, long long *result) {
    if (!node || !result) return 0;
    
    if (node->type == AST_LITERAL) {
        *result = atoll(node->token.value);
        return 1;
    }
    
    if (node->type == AST_BINARY_OP) {
        long long leftVal, rightVal;
        if (!evaluateConstantExpression(node->left, &leftVal)) return 0;
        if (!evaluateConstantExpression(node->right, &rightVal)) return 0;
        
        switch (node->token.type) {
            case TOKEN_PLUS:
                *result = leftVal + rightVal;
                return 1;
            case TOKEN_MINUS:
                *result = leftVal - rightVal;
                return 1;
            case TOKEN_MUL:
                *result = leftVal * rightVal;
                return 1;
            case TOKEN_DIV:
                if (rightVal == 0) return 0;
                *result = leftVal / rightVal;
                return 1;
            case TOKEN_LE:
                *result = (leftVal <= rightVal) ? 1 : 0;
                return 1;
            case TOKEN_GE:
                *result = (leftVal >= rightVal) ? 1 : 0;
                return 1;
            case TOKEN_LT:
                *result = (leftVal < rightVal) ? 1 : 0;
                return 1;
            case TOKEN_GT:
                *result = (leftVal > rightVal) ? 1 : 0;
                return 1;
            case TOKEN_EQ:
                *result = (leftVal == rightVal) ? 1 : 0;
                return 1;
            case TOKEN_NE:
                *result = (leftVal != rightVal) ? 1 : 0;
                return 1;
            case TOKEN_AND:
                *result = (leftVal && rightVal) ? 1 : 0;
                return 1;
            case TOKEN_OR:
                *result = (leftVal || rightVal) ? 1 : 0;
                return 1;
            default:
                return 0;
        }
    }
    
    if (node->type == AST_UNARY_OP) {
        long long val;
        if (!evaluateConstantExpression(node->left, &val)) return 0;
        
        switch (node->token.type) {
            case TOKEN_MINUS:
                *result = -val;
                return 1;
            case TOKEN_NOT:
                *result = !val;
                return 1;
            default:
                return 0;
        }
    }
    
    return 0;
}

ASTNode *constantFolding(ASTNode *node) {
    if (!node) return NULL;
    
    if (node->type == AST_BINARY_OP || node->type == AST_UNARY_OP) {
        node->left = constantFolding(node->left);
        node->right = constantFolding(node->right);
        
        if (isConstantExpression(node)) {
            long long result;
            if (evaluateConstantExpression(node, &result)) {
                ASTNode *literal = allocASTNode();
                literal->type = AST_LITERAL;
                literal->token.type = TOKEN_NUMBER;
                snprintf(literal->token.value, sizeof(literal->token.value), "%lld", result);
                literal->token.line = node->token.line;
                literal->token.column = node->token.column;
                return literal;
            }
        }
    }
    
    node->left = constantFolding(node->left);
    node->right = constantFolding(node->right);
    node->body = constantFolding(node->body);
    node->next = constantFolding(node->next);
    node->params = constantFolding(node->params);
    node->condition = constantFolding(node->condition);
    node->init = constantFolding(node->init);
    node->update = constantFolding(node->update);
    node->elseBody = constantFolding(node->elseBody);
    
    return node;
}

int isAlwaysFalse(ASTNode *node) {
    if (!node) return 0;
    
    if (node->type == AST_LITERAL) {
        long long val = atoll(node->token.value);
        return val == 0;
    }
    
    if (node->type == AST_BINARY_OP) {
        switch (node->token.type) {
            case TOKEN_EQ:
                if (isConstantExpression(node->left) && isConstantExpression(node->right)) {
                    long long leftVal, rightVal;
                    if (evaluateConstantExpression(node->left, &leftVal) &&
                        evaluateConstantExpression(node->right, &rightVal)) {
                        return leftVal == rightVal ? 0 : 1;
                    }
                }
                break;
            case TOKEN_NE:
                if (isConstantExpression(node->left) && isConstantExpression(node->right)) {
                    long long leftVal, rightVal;
                    if (evaluateConstantExpression(node->left, &leftVal) &&
                        evaluateConstantExpression(node->right, &rightVal)) {
                        return leftVal != rightVal ? 0 : 1;
                    }
                }
                break;
            case TOKEN_AND:
                return isAlwaysFalse(node->left) || isAlwaysFalse(node->right);
            case TOKEN_OR:
                return isAlwaysFalse(node->left) && isAlwaysFalse(node->right);
            case TOKEN_LT:
                if (isConstantExpression(node->left) && isConstantExpression(node->right)) {
                    long long leftVal, rightVal;
                    if (evaluateConstantExpression(node->left, &leftVal) &&
                        evaluateConstantExpression(node->right, &rightVal)) {
                        return leftVal < rightVal ? 0 : 1;
                    }
                }
                break;
            case TOKEN_GT:
                if (isConstantExpression(node->left) && isConstantExpression(node->right)) {
                    long long leftVal, rightVal;
                    if (evaluateConstantExpression(node->left, &leftVal) &&
                        evaluateConstantExpression(node->right, &rightVal)) {
                        return leftVal > rightVal ? 0 : 1;
                    }
                }
                break;
        }
    }
    
    return 0;
}

int isAlwaysTrue(ASTNode *node) {
    if (!node) return 0;
    
    if (node->type == AST_LITERAL) {
        long long val = atoll(node->token.value);
        return val != 0;
    }
    
    if (node->type == AST_BINARY_OP) {
        switch (node->token.type) {
            case TOKEN_EQ:
                if (isConstantExpression(node->left) && isConstantExpression(node->right)) {
                    long long leftVal, rightVal;
                    if (evaluateConstantExpression(node->left, &leftVal) &&
                        evaluateConstantExpression(node->right, &rightVal)) {
                        return leftVal == rightVal ? 1 : 0;
                    }
                }
                break;
            case TOKEN_NE:
                if (isConstantExpression(node->left) && isConstantExpression(node->right)) {
                    long long leftVal, rightVal;
                    if (evaluateConstantExpression(node->left, &leftVal) &&
                        evaluateConstantExpression(node->right, &rightVal)) {
                        return leftVal != rightVal ? 1 : 0;
                    }
                }
                break;
            case TOKEN_AND:
                return isAlwaysTrue(node->left) && isAlwaysTrue(node->right);
            case TOKEN_OR:
                return isAlwaysTrue(node->left) || isAlwaysTrue(node->right);
            case TOKEN_LT:
                if (isConstantExpression(node->left) && isConstantExpression(node->right)) {
                    long long leftVal, rightVal;
                    if (evaluateConstantExpression(node->left, &leftVal) &&
                        evaluateConstantExpression(node->right, &rightVal)) {
                        return leftVal < rightVal ? 1 : 0;
                    }
                }
                break;
            case TOKEN_GT:
                if (isConstantExpression(node->left) && isConstantExpression(node->right)) {
                    long long leftVal, rightVal;
                    if (evaluateConstantExpression(node->left, &leftVal) &&
                        evaluateConstantExpression(node->right, &rightVal)) {
                        return leftVal > rightVal ? 1 : 0;
                    }
                }
                break;
        }
    }
    
    return 0;
}

ASTNode *deadCodeElimination(ASTNode *node) {
    if (!node) return NULL;
    
    if (node->type == AST_IF) {
        node->condition = deadCodeElimination(node->condition);
        node->body = deadCodeElimination(node->body);
        node->elseBody = deadCodeElimination(node->elseBody);
        
        if (isAlwaysFalse(node->condition)) {
            return node->elseBody;
        } else if (isAlwaysTrue(node->condition)) {
            return node->body;
        }
    } else if (node->type == AST_WHILE) {
        node->condition = deadCodeElimination(node->condition);
        if (isAlwaysFalse(node->condition)) {
            return NULL;
        }
        node->body = deadCodeElimination(node->body);
    } else if (node->type == AST_FOR) {
        node->condition = deadCodeElimination(node->condition);
        if (isAlwaysFalse(node->condition)) {
            node->init = deadCodeElimination(node->init);
            return node->init;
        }
        node->init = deadCodeElimination(node->init);
        node->update = deadCodeElimination(node->update);
        node->body = deadCodeElimination(node->body);
    } else if (node->type == AST_FUNCTION_DECLARATION) {
        ASTNode *current = node->body->next;
        ASTNode *newNext = NULL;
        ASTNode *newTail = NULL;
        int foundReturn = 0;
        
        while (current && !foundReturn) {
            ASTNode *optimized = deadCodeElimination(current);
            
            if (optimized) {
                if (optimized->type == AST_IF && isAlwaysFalse(optimized->condition)) {
                    current = current->next;
                    continue;
                }
                if (optimized->type == AST_WHILE && isAlwaysFalse(optimized->condition)) {
                    current = current->next;
                    continue;
                }
                if (optimized->type == AST_FOR && isAlwaysFalse(optimized->condition)) {
                    if (!newNext) {
                        newNext = optimized->init;
                        newTail = optimized->init;
                    } else if (newTail && optimized->init) {
                        newTail->next = optimized->init;
                        while (newTail->next) newTail = newTail->next;
                    }
                    current = current->next;
                    continue;
                }
                
                if (!newNext) {
                    newNext = optimized;
                    newTail = optimized;
                } else if (newTail) {
                    newTail->next = optimized;
                }
                while (newTail && newTail->next) {
                    newTail = newTail->next;
                }
                
                if (optimized->type == AST_RETURN) {
                    foundReturn = 1;
                }
            }
            
            current = current->next;
        }
        
        node->body->next = newNext;
    }
    
    node->left = deadCodeElimination(node->left);
    node->right = deadCodeElimination(node->right);
    node->body = deadCodeElimination(node->body);
    node->params = deadCodeElimination(node->params);
    node->condition = deadCodeElimination(node->condition);
    node->init = deadCodeElimination(node->init);
    node->update = deadCodeElimination(node->update);
    node->elseBody = deadCodeElimination(node->elseBody);
    
    return node;
}

typedef struct ExpressionCache {
    ASTNode *expr;
    char *key;
    struct ExpressionCache *next;
} ExpressionCache;

static ExpressionCache *cache = NULL;

static char *expressionToKey(ASTNode *node) {
    if (!node) return NULL;
    
    static char key[1024];
    key[0] = '\0';
    
    switch (node->type) {
        case AST_LITERAL:
            snprintf(key, sizeof(key), "LIT:%s", node->token.value);
            break;
        case AST_IDENTIFIER:
            snprintf(key, sizeof(key), "ID:%s", node->token.value);
            break;
        case AST_BINARY_OP: {
            char *leftKey = expressionToKey(node->left);
            char *rightKey = expressionToKey(node->right);
            if (leftKey && rightKey) {
                snprintf(key, sizeof(key), "BIN:%d:%s:%s", node->token.type, leftKey, rightKey);
            }
            break;
        }
        case AST_UNARY_OP: {
            char *childKey = expressionToKey(node->left);
            if (childKey) {
                snprintf(key, sizeof(key), "UN:%d:%s", node->token.type, childKey);
            }
            break;
        }
        default:
            return NULL;
    }
    
    return key;
}

static void freeExpressionCache() {
    ExpressionCache *current = cache;
    while (current) {
        ExpressionCache *next = current->next;
        free(current->key);
        free(current);
        current = next;
    }
    cache = NULL;
}

ASTNode *commonSubexpressionElimination(ASTNode *node) {
    if (!node || optLevel < OPT_LEVEL_FULL) return node;
    
    switch (node->type) {
        case AST_BINARY_OP: {
            node->left = commonSubexpressionElimination(node->left);
            node->right = commonSubexpressionElimination(node->right);
            
            if (node->token.type != TOKEN_ASSIGN) {
                char *key = expressionToKey(node);
                if (key) {
                    ExpressionCache *current = cache;
                    while (current) {
                        if (strcmp(current->key, key) == 0) {
                            return current->expr;
                        }
                        current = current->next;
                    }
                    
                    ExpressionCache *newEntry = malloc(sizeof(ExpressionCache));
                    newEntry->key = strdup(key);
                    newEntry->expr = node;
                    newEntry->next = cache;
                    cache = newEntry;
                }
            }
            break;
        }
        
        case AST_STATEMENT:
        case AST_PROGRAM: {
            freeExpressionCache();
            node->body = commonSubexpressionElimination(node->body);
            break;
        }
        
        case AST_FUNCTION_DECLARATION: {
            freeExpressionCache();
            node->body = commonSubexpressionElimination(node->body);
            break;
        }
    }
    
    node->left = commonSubexpressionElimination(node->left);
    node->right = commonSubexpressionElimination(node->right);
    node->next = commonSubexpressionElimination(node->next);
    node->body = commonSubexpressionElimination(node->body);
    node->params = commonSubexpressionElimination(node->params);
    node->condition = commonSubexpressionElimination(node->condition);
    node->init = commonSubexpressionElimination(node->init);
    node->update = commonSubexpressionElimination(node->update);
    node->elseBody = commonSubexpressionElimination(node->elseBody);
    
    return node;
}

ASTNode *optimizeAST(ASTNode *node) {
    if (!node || optLevel == OPT_LEVEL_NONE) return node;
    
    node = constantFolding(node);
    node = deadCodeElimination(node);
    if (optLevel >= OPT_LEVEL_FULL) {
        node = commonSubexpressionElimination(node);
    }
    
    return node;
}
