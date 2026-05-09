#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"

Token currentToken;
ImportNode *importList = NULL;
ImportNode *importTail = NULL;
SymbolTable *currentSymbolTable = NULL;
int errorCount = 0;

// AST 内存池
ASTPool astPool;

ASTNode *parseProgram();
ASTNode *parseStatement();
ASTNode *parseExpression();
ASTNode *parseTerm();
ASTNode *parseFactor();
ASTNode *parsePrimary();
ASTNode *parseObjectMember();
ASTNode *createNode(ASTNodeType type, Token token);
void expect(TokenType type, const char *message);

ASTNode *parseProgram() {
    // 创建全局符号表
    currentSymbolTable = createSymbolTable(NULL);
    
    ASTNode *program = createNode(AST_PROGRAM, currentToken);
    ASTNode *current = program;

    while (currentToken.type != TOKEN_EOF) {
        ASTNode *statement = parseStatement();
        if (statement) {
            current->next = statement;
            current = statement;
        } else if (currentToken.type != TOKEN_EOF) {
            // 错误恢复：跳过当前token
            currentToken = getNextToken();
        }
    }

    return program;
}

// 清理全局资源
void cleanupParser() {
    if (currentSymbolTable) {
        freeSymbolTable(currentSymbolTable);
        currentSymbolTable = NULL;
    }
}

ASTNode *parseMacroDefinition();

ASTNode *parseStatement() {
    switch (currentToken.type) {
        case TOKEN_HASH:
            {
                expect(TOKEN_HASH, "Expected #");
                
                if (currentToken.type == TOKEN_DEFINES) {
                    expect(TOKEN_DEFINES, "Expected defines");
                    return parseMacroDefinition();
                } else if (currentToken.type == TOKEN_UNDEF) {
                    expect(TOKEN_UNDEF, "Expected undef");
                    Token nameTok = currentToken;
                    expect(TOKEN_ID, "Expected macro name");
                    expect(TOKEN_SEMICOLON, "Expected ;");
                    
                    // 创建宏取消定义节点
                    ASTNode *undefNode = createNode(AST_MACRO_DEFINITION, nameTok);
                    strcpy(undefNode->filename, "undef"); // 标记为取消定义
                    return undefNode;
                } else if (currentToken.type == TOKEN_INCLUDE) {
                    expect(TOKEN_INCLUDE, "Expected include");
                    Token filenameTok = currentToken;
                    if (currentToken.type == TOKEN_STRING) {
                        expect(TOKEN_STRING, "Expected filename");
                    } else if (currentToken.type == TOKEN_DIR) {
                        expect(TOKEN_DIR, "Expected dir");
                    }
                    expect(TOKEN_SEMICOLON, "Expected ;");
                    
                    // 创建包含节点（实际处理在预处理器中）
                    ASTNode *includeNode = createNode(AST_IMPORT, filenameTok);
                    strcpy(includeNode->filename, filenameTok.value);
                    return includeNode;
                }
                break;
            }
        case TOKEN_MACRO:
            {
                expect(TOKEN_MACRO, "Expected macro");
                return parseMacroDefinition();
            }
        case TOKEN_INT:
            {
                Token typeTok = currentToken;
                expect(TOKEN_INT, "Expected int");

                if (currentToken.type == TOKEN_ID) {
                    Token varTok = currentToken;
                    expect(TOKEN_ID, "Expected variable name");

                    addSymbol(currentSymbolTable, varTok.value, SYMBOL_VARIABLE, "int");

                    ASTNode *varDecl = createNode(AST_VARIABLE_DECLARATION, typeTok);
                    ASTNode *varNode = createNode(AST_IDENTIFIER, varTok);
                    varDecl->left = varNode;

                    if (currentToken.type == TOKEN_ASSIGN) {
                        expect(TOKEN_ASSIGN, "Expected =");
                        ASTNode *expr = parseExpression();
                        varDecl->right = expr;
                    }

                    expect(TOKEN_SEMICOLON, "Expected ;");
                    return varDecl;
                } else {
                    fprintf(stderr, "Error: Expected variable name at line %d, column %d\n", currentToken.line, currentToken.column);
                    errorCount++;
                    // 错误恢复：跳过直到分号
                    while (currentToken.type != 0 && currentToken.type != TOKEN_EOF && 
                           currentToken.type != TOKEN_SEMICOLON && currentToken.type != TOKEN_RBRACE) {
                        currentToken = getNextToken();
                    }
                    if (currentToken.type == TOKEN_SEMICOLON) {
                        currentToken = getNextToken();
                    }
                    return NULL;
                }
            }
        case TOKEN_IMPORT:
            {
                Token importTok = currentToken;
                expect(TOKEN_IMPORT, "Expected import");

                if (currentToken.type != TOKEN_STRING) {
                    fprintf(stderr, "Error: Expected filename in quotes at line %d, column %d\n", currentToken.line, currentToken.column);
                    errorCount++;
                    return NULL;
                }

                Token filenameTok = currentToken;
                expect(TOKEN_STRING, "Expected filename in quotes");
                expect(TOKEN_SEMICOLON, "Expected ;");

                // 创建导入节点（实际导入在main.c中通过processImports处理）
                ASTNode *importNode = createNode(AST_IMPORT, importTok);
                strcpy(importNode->filename, filenameTok.value);

                return importNode;
            }
        case TOKEN_ID:
            {
                // 检查是否是struct或enum关键字
                if (strcmp(currentToken.value, "struct") == 0) {
                    // 处理结构体声明
                    Token structTok = currentToken;
                    expect(TOKEN_ID, "Expected struct");
                    Token structNameTok = currentToken;
                    expect(TOKEN_ID, "Expected struct name");
                    expect(TOKEN_LBRACE, "Expected {");
                    
                    ASTNode *structNode = createNode(AST_STRUCT_DECLARATION, structTok);
                    strcpy(structNode->namespace, structNameTok.value);
                    
                    ASTNode *members = NULL;
                    ASTNode *currentMember = NULL;
                    
                    while (currentToken.type != TOKEN_RBRACE && currentToken.type != TOKEN_EOF) {
                        // 解析结构体成员
                        Token memberTypeTok = currentToken;
                        if (currentToken.type != TOKEN_ID) {
                            fprintf(stderr, "Error: Expected member type at line %d, column %d\n", currentToken.line, currentToken.column);
                            errorCount++;
                            // 错误恢复：跳过直到分号或右大括号
                            while (currentToken.type != 0 && currentToken.type != TOKEN_EOF && 
                                   currentToken.type != TOKEN_SEMICOLON && currentToken.type != TOKEN_RBRACE) {
                                currentToken = getNextToken();
                            }
                            if (currentToken.type == TOKEN_SEMICOLON) {
                                currentToken = getNextToken();
                            }
                            continue;
                        }
                        expect(TOKEN_ID, "Expected member type");
                        
                        Token memberNameTok = currentToken;
                        if (currentToken.type != TOKEN_ID) {
                            fprintf(stderr, "Error: Expected member name at line %d, column %d\n", currentToken.line, currentToken.column);
                            errorCount++;
                            // 错误恢复：跳过直到分号或右大括号
                            while (currentToken.type != 0 && currentToken.type != TOKEN_EOF && 
                                   currentToken.type != TOKEN_SEMICOLON && currentToken.type != TOKEN_RBRACE) {
                                currentToken = getNextToken();
                            }
                            if (currentToken.type == TOKEN_SEMICOLON) {
                                currentToken = getNextToken();
                            }
                            continue;
                        }
                        expect(TOKEN_ID, "Expected member name");
                        
                        expect(TOKEN_SEMICOLON, "Expected ;");
                        
                        ASTNode *memberNode = createNode(AST_STRUCT_MEMBER, memberTypeTok);
                        memberNode->left = createNode(AST_IDENTIFIER, memberNameTok);
                        
                        if (!members) {
                            members = memberNode;
                            currentMember = memberNode;
                        } else {
                            currentMember->next = memberNode;
                            currentMember = memberNode;
                        }
                    }
                    
                    expect(TOKEN_RBRACE, "Expected }");
                    expect(TOKEN_SEMICOLON, "Expected ;");
                    
                    structNode->body = members;
                    
                    // 将结构体添加到符号表
                    addSymbol(currentSymbolTable, structNameTok.value, SYMBOL_STRUCT, "struct");
                    
                    return structNode;
                } else if (strcmp(currentToken.value, "enum") == 0) {
                    // 处理枚举声明
                    Token enumTok = currentToken;
                    expect(TOKEN_ID, "Expected enum");
                    Token enumNameTok = currentToken;
                    expect(TOKEN_ID, "Expected enum name");
                    expect(TOKEN_LBRACE, "Expected {");
                    
                    ASTNode *enumNode = createNode(AST_ENUM_DECLARATION, enumTok);
                    strcpy(enumNode->namespace, enumNameTok.value);
                    
                    ASTNode *members = NULL;
                    ASTNode *currentMember = NULL;
                    
                    while (currentToken.type != TOKEN_RBRACE) {
                        // 解析枚举成员
                        Token memberNameTok = currentToken;
                        expect(TOKEN_ID, "Expected member name");
                        
                        ASTNode *memberNode = createNode(AST_ENUM_MEMBER, memberNameTok);
                        
                        if (currentToken.type == TOKEN_ASSIGN) {
                            expect(TOKEN_ASSIGN, "Expected =");
                            ASTNode *valueExpr = parseExpression();
                            memberNode->right = valueExpr;
                        }
                        
                        if (currentToken.type == TOKEN_COMMA) {
                            expect(TOKEN_COMMA, "Expected ,");
                        } else if (currentToken.type != TOKEN_RBRACE) {
                            expect(TOKEN_SEMICOLON, "Expected ;");
                        }
                        
                        if (!members) {
                            members = memberNode;
                            currentMember = memberNode;
                        } else {
                            currentMember->next = memberNode;
                            currentMember = memberNode;
                        }
                        
                        // 将枚举值添加到符号表
                        addSymbol(currentSymbolTable, memberNameTok.value, SYMBOL_VARIABLE, "int");
                    }
                    
                    expect(TOKEN_RBRACE, "Expected }");
                    expect(TOKEN_SEMICOLON, "Expected ;");
                    
                    enumNode->body = members;
                    
                    // 将枚举添加到符号表
                    addSymbol(currentSymbolTable, enumNameTok.value, SYMBOL_ENUM, "enum");
                    
                    return enumNode;
                }
                // 检查是否是类型关键字或结构体类型
                Token typeTok = currentToken;
                SymbolEntry *typeSymbol = lookupSymbol(currentSymbolTable, typeTok.value);
                
                // 检查是否是类型转换函数调用（如 string(a)）
                if ((currentToken.type == TOKEN_STRING_TYPE || 
                     currentToken.type == TOKEN_CHAR_TYPE ||
                     (currentToken.type == TOKEN_ID && 
                      (strcmp(typeTok.value, "int") == 0 || 
                       strcmp(typeTok.value, "dir") == 0)))) {
                    // 消耗当前 token
                    expect(currentToken.type, "Expected type");
                    
                    // 检查是否是函数调用
                    if (currentToken.type == TOKEN_LPAREN) {
                        // 处理类型转换函数调用
                        ASTNode *call = createNode(AST_FUNCTION_CALL, typeTok);
                        expect(TOKEN_LPAREN, "Expected (");
                        ASTNode *arg = parseExpression();
                        call->params = arg;
                        expect(TOKEN_RPAREN, "Expected )");
                        if (currentToken.type == TOKEN_SEMICOLON) {
                            expect(TOKEN_SEMICOLON, "Expected ;");
                            ASTNode *stmt = createNode(AST_STATEMENT, typeTok);
                            stmt->left = call;
                            return stmt;
                        }
                        return call;
                    } else {
                        // 处理变量声明
                        Token idTok = currentToken;
                        expect(TOKEN_ID, "Expected variable name");
                        ASTNode *declaration = createNode(AST_VARIABLE_DECLARATION, typeTok);
                        ASTNode *varNode = createNode(AST_IDENTIFIER, idTok);
                        declaration->left = varNode;
                        // 将变量添加到符号表
                        addSymbol(currentSymbolTable, idTok.value, SYMBOL_VARIABLE, typeTok.value);
                        if (currentToken.type == TOKEN_ASSIGN) {
                            expect(TOKEN_ASSIGN, "Expected =");
                            ASTNode *expr = parseExpression();
                            declaration->right = expr;
                        }
                        expect(TOKEN_SEMICOLON, "Expected ;");
                        return declaration;
                    }
                } else if (currentToken.type == TOKEN_ID && 
                           (strcmp(typeTok.value, "int") == 0 || 
                            strcmp(typeTok.value, "float") == 0 || 
                            strcmp(typeTok.value, "double") == 0) ||
                           (typeSymbol && (typeSymbol->type == SYMBOL_STRUCT || typeSymbol->type == SYMBOL_ENUM))) {
                    // 处理变量声明
                    expect(TOKEN_ID, "Expected type");
                    Token idTok = currentToken;
                    expect(TOKEN_ID, "Expected variable name");
                    ASTNode *declaration = createNode(AST_VARIABLE_DECLARATION, typeTok);
                    ASTNode *varNode = createNode(AST_IDENTIFIER, idTok);
                    declaration->left = varNode;
                    // 将变量添加到符号表
                    addSymbol(currentSymbolTable, idTok.value, SYMBOL_VARIABLE, typeTok.value);
                    if (currentToken.type == TOKEN_ASSIGN) {
                        expect(TOKEN_ASSIGN, "Expected =");
                        ASTNode *expr = parseExpression();
                        declaration->right = expr;
                    }
                    expect(TOKEN_SEMICOLON, "Expected ;");
                    return declaration;
                } else {
                    // 处理函数调用或标识符或结构体成员访问
                    Token funcToken = currentToken;
                    expect(TOKEN_ID, "Expected identifier");
                    
                    // 检查是否是结构体成员访问或指针解引用
                    ASTNode *expr = createNode(AST_IDENTIFIER, funcToken);
                    while (currentToken.type == TOKEN_DOT) {
                        expect(TOKEN_DOT, "Expected .");
                        Token memberToken = currentToken;
                        expect(TOKEN_ID, "Expected member name");
                        
                        // 检查是否是指针解引用 (ptr.value)
                        if (strcmp(memberToken.value, "value") == 0) {
                            // 创建解引用节点
                            ASTNode *dereference = createNode(AST_DEREFERENCE, memberToken);
                            dereference->left = expr;
                            expr = dereference;
                        } else {
                            // 创建成员访问节点
                            ASTNode *memberAccess = createNode(AST_MEMBER_ACCESS, memberToken);
                            memberAccess->left = expr; // 左侧是结构体对象
                            memberAccess->right = createNode(AST_IDENTIFIER, memberToken); // 右侧是成员名
                            expr = memberAccess;
                        }
                    }
                    
                    if (currentToken.type == TOKEN_LPAREN) {
                        // 处理函数调用
                        ASTNode *call = createNode(AST_FUNCTION_CALL, funcToken);
                        expect(TOKEN_LPAREN, "Expected (");
                        ASTNode *args = NULL;
                        ASTNode *currentArg = NULL;
                        while (currentToken.type != TOKEN_RPAREN) {
                            ASTNode *arg = parseExpression();
                            if (!args) {
                                args = arg;
                                currentArg = arg;
                            } else {
                                currentArg->next = arg;
                                currentArg = arg;
                            }
                            if (currentToken.type == TOKEN_COMMA) {
                                expect(TOKEN_COMMA, "Expected ,");
                            }
                        }
                        expect(TOKEN_RPAREN, "Expected )");
                        if (currentToken.type == TOKEN_DOUBLE_ARROW) {
                            expect(TOKEN_DOUBLE_ARROW, "Expected =>");
                            Token varToken = currentToken;
                            expect(TOKEN_ID, "Expected variable name");
                            expect(TOKEN_SEMICOLON, "Expected ;");
                            call->params = args;
                            ASTNode *assign = createNode(AST_VARIABLE_DECLARATION, varToken);
                            assign->left = createNode(AST_IDENTIFIER, varToken);
                            assign->right = call;
                            // 将变量添加到符号表
                            addSymbol(currentSymbolTable, varToken.value, SYMBOL_VARIABLE, "int");
                            return assign;
                        }
                        expect(TOKEN_SEMICOLON, "Expected ;");
                        call->params = args;
                        ASTNode *stmt = createNode(AST_STATEMENT, funcToken);
                        stmt->left = call;
                        return stmt;
                    } else if (currentToken.type == TOKEN_ASSIGN) {
                        // 处理赋值语句
                        expect(TOKEN_ASSIGN, "Expected =");
                        ASTNode *value = parseExpression();
                        expect(TOKEN_SEMICOLON, "Expected ;");
                        
                        // 检查是否是内存所有权转移或分配
                        if (expr->type == AST_IDENTIFIER) {
                            SymbolEntry *leftSymbol = lookupSymbol(currentSymbolTable, expr->token.value);
                            if (leftSymbol && leftSymbol->type == SYMBOL_MEMORY) {
                                // 如果右侧是 alloc 调用，设置内存所有权
                                if (value->type == AST_ALLOC_CALL) {
                                    leftSymbol->isMemoryOwner = 1;
                                } else if (value->type == AST_IDENTIFIER) {
                                    SymbolEntry *rightSymbol = lookupSymbol(currentSymbolTable, value->token.value);
                                    if (rightSymbol && rightSymbol->type == SYMBOL_MEMORY && rightSymbol->isMemoryOwner) {
                                        // 内存所有权转移
                                        leftSymbol->isMemoryOwner = 1;
                                        rightSymbol->isMemoryOwner = 0;
                                    }
                                }
                            }
                        }
                        
                        // 创建赋值节点
                        Token assignToken;
                        assignToken.type = TOKEN_ASSIGN;
                        strcpy(assignToken.value, "=");
                        assignToken.line = funcToken.line;
                        assignToken.column = funcToken.column;
                        ASTNode *assign = createNode(AST_VARIABLE_DECLARATION, assignToken);
                        assign->left = expr;
                        assign->right = value;
                        return assign;
                    } else {
                        // 处理普通标识符
                        expect(TOKEN_SEMICOLON, "Expected ;");
                        return expr;
                    }
                }
            }
            break;
        case TOKEN_OUT:
            {
                ASTNode *output = createNode(AST_OUTPUT, currentToken);
                expect(TOKEN_OUT, "Expected eout");
                expect(TOKEN_GT, "Expected >");
                // namespace可以是任意表达式（标识符、dir路径等）
                // 但如果namespace是标识符或dir，需要避免符号检查
                ASTNode *nsExpr = NULL;
                if (currentToken.type == TOKEN_DIR) {
                    // dir类型路径，直接创建字面量节点
                    nsExpr = createNode(AST_LITERAL, currentToken);
                    currentToken = getNextToken();
                } else if (currentToken.type == TOKEN_ID) {
                    // 检查是否是std标识符
                    if (strcmp(currentToken.value, "std") == 0) {
                        // 标准输出标识符
                        nsExpr = createNode(AST_IDENTIFIER, currentToken);
                        strcpy(nsExpr->namespace, "std"); // 标记为std命名空间
                    } else {
                        // 其他标识符作为文件路径变量
                        nsExpr = createNode(AST_IDENTIFIER, currentToken);
                    }
                    currentToken = getNextToken();
                } else {
                    nsExpr = parseExpression();
                }
                output->namespaceExpr = nsExpr;
                ASTNode *expr = parseExpression();
                output->left = expr;
                expect(TOKEN_SEMICOLON, "Expected ;");
                return output;
            }
        case TOKEN_IN:
            {
                ASTNode *input = createNode(AST_INPUT, currentToken);
                expect(TOKEN_IN, "Expected ein");
                expect(TOKEN_GT, "Expected >");
                expect(TOKEN_NAMESPACE, "Expected namespace");
                // namespace可以是任意表达式（标识符、dir路径等）
                // 但如果namespace是标识符或dir，需要避免符号检查
                ASTNode *nsExpr = NULL;
                if (currentToken.type == TOKEN_DIR) {
                    // dir类型路径，直接创建字面量节点
                    nsExpr = createNode(AST_LITERAL, currentToken);
                    currentToken = getNextToken();
                } else if (currentToken.type == TOKEN_ID) {
                    // 检查是否是std标识符
                    if (strcmp(currentToken.value, "std") == 0) {
                        // 标准输入标识符
                        nsExpr = createNode(AST_IDENTIFIER, currentToken);
                        strcpy(nsExpr->namespace, "std"); // 标记为std命名空间
                    } else {
                        // 其他标识符作为文件路径变量
                        nsExpr = createNode(AST_IDENTIFIER, currentToken);
                    }
                    currentToken = getNextToken();
                } else {
                    nsExpr = parseExpression();
                }
                input->namespaceExpr = nsExpr;
                // 对于ein，提示词是可选的
                if (currentToken.type == TOKEN_STRING) {
                    Token promptTok = currentToken;
                    input->left = createNode(AST_LITERAL, promptTok);
                    expect(TOKEN_STRING, "Expected prompt string");
                } else {
                    // 如果没有提示词，创建一个空字符串
                    input->left = NULL;
                }
                if (currentToken.type == TOKEN_DOUBLE_ARROW) {
                    expect(TOKEN_DOUBLE_ARROW, "Expected =>");
                    Token varTok = currentToken;
                    expect(TOKEN_ID, "Expected variable name");
                    input->right = createNode(AST_IDENTIFIER, varTok);
                }
                expect(TOKEN_SEMICOLON, "Expected ;");
                return input;
            }

        case TOKEN_FLOAT:
        case TOKEN_DOUBLE:
        case TOKEN_DIR:
        case TOKEN_STRING_TYPE:
        case TOKEN_CHAR_TYPE:
            {
                Token typeTok = currentToken;
                expect(currentToken.type, "Expected type");
                
                // 检查是否是函数调用（如 string(a)）
                if (currentToken.type == TOKEN_LPAREN) {
                    // 处理类型转换函数调用
                    ASTNode *call = createNode(AST_FUNCTION_CALL, typeTok);
                    expect(TOKEN_LPAREN, "Expected (");
                    ASTNode *arg = parseExpression();
                    call->params = arg;
                    expect(TOKEN_RPAREN, "Expected )");
                    expect(TOKEN_SEMICOLON, "Expected ;");
                    return call;
                } else {
                    // 处理变量声明
                    Token idTok = currentToken;
                    expect(TOKEN_ID, "Expected variable name");
                    ASTNode *declaration = createNode(AST_VARIABLE_DECLARATION, typeTok);
                    ASTNode *varNode = createNode(AST_IDENTIFIER, idTok);
                    declaration->left = varNode;
                    // 将变量添加到符号表
                    addSymbol(currentSymbolTable, idTok.value, SYMBOL_VARIABLE, typeTok.value);
                    if (currentToken.type == TOKEN_ASSIGN) {
                        expect(TOKEN_ASSIGN, "Expected =");
                        ASTNode *expr = parseExpression();
                        declaration->right = expr;
                    }
                    expect(TOKEN_SEMICOLON, "Expected ;");
                    return declaration;
                }
            }
        case TOKEN_PO:
            {
                // 处理指针声明
                Token poTok = currentToken;
                expect(TOKEN_PO, "Expected po");
                
                // 解析指针指向的类型
                Token typeTok = currentToken;
                if (currentToken.type == TOKEN_ID || currentToken.type == TOKEN_INT || 
                    currentToken.type == TOKEN_FLOAT || currentToken.type == TOKEN_DOUBLE || 
                    currentToken.type == TOKEN_STRING || currentToken.type == TOKEN_CHAR) {
                    if (currentToken.type == TOKEN_ID) {
                        expect(TOKEN_ID, "Expected type");
                    } else {
                        expect(currentToken.type, "Expected type");
                    }
                } else {
                    fprintf(stderr, "Error: Expected type after po at line %d, column %d\n", currentToken.line, currentToken.column);
                    errorCount++;
                    return NULL;
                }
                
                Token idTok = currentToken;
                expect(TOKEN_ID, "Expected variable name");
                
                ASTNode *declaration = createNode(AST_POINTER_DECLARATION, poTok);
                ASTNode *varNode = createNode(AST_IDENTIFIER, idTok);
                declaration->left = varNode;
                strcpy(declaration->namespace, typeTok.value);
                
                // 将指针添加到符号表
                addSymbol(currentSymbolTable, idTok.value, SYMBOL_POINTER, typeTok.value);
                
                if (currentToken.type == TOKEN_ASSIGN) {
                    expect(TOKEN_ASSIGN, "Expected =");
                    ASTNode *expr = parseExpression();
                    declaration->right = expr;
                }
                expect(TOKEN_SEMICOLON, "Expected ;");
                return declaration;
            }
        case TOKEN_MEM:
            {
                // 处理内存块声明
                Token memTok = currentToken;
                expect(TOKEN_MEM, "Expected mem");
                
                Token idTok = currentToken;
                expect(TOKEN_ID, "Expected variable name");
                
                ASTNode *declaration = createNode(AST_MEMORY_DECLARATION, memTok);
                ASTNode *varNode = createNode(AST_IDENTIFIER, idTok);
                declaration->left = varNode;
                
                // 将内存块添加到符号表
                addSymbol(currentSymbolTable, idTok.value, SYMBOL_MEMORY, "mem");
                
                if (currentToken.type == TOKEN_ASSIGN) {
                    expect(TOKEN_ASSIGN, "Expected =");
                    ASTNode *expr = parseExpression();
                    declaration->right = expr;
                }
                expect(TOKEN_SEMICOLON, "Expected ;");
                return declaration;
            }
        case TOKEN_ALLOC:
            {
                // 处理 alloc 函数调用作为表达式
                Token allocToken = currentToken;
                expect(TOKEN_ALLOC, "Expected alloc");
                if (currentToken.type == TOKEN_LPAREN) {
                    ASTNode *call = createNode(AST_ALLOC_CALL, allocToken);
                    expect(TOKEN_LPAREN, "Expected (");
                    ASTNode *args = NULL;
                    ASTNode *currentArg = NULL;
                    while (currentToken.type != TOKEN_RPAREN) {
                        ASTNode *arg = parseExpression();
                        if (!args) {
                            args = arg;
                            currentArg = arg;
                        } else {
                            currentArg->next = arg;
                            currentArg = arg;
                        }
                        if (currentToken.type == TOKEN_COMMA) {
                            expect(TOKEN_COMMA, "Expected ,");
                        }
                    }
                    expect(TOKEN_RPAREN, "Expected )");
                    call->params = args;
                    expect(TOKEN_SEMICOLON, "Expected ;");
                    return call;
                }
                return NULL;
            }
        case TOKEN_FREE:
            {
                // 处理 free 函数调用作为语句
                Token freeToken = currentToken;
                expect(TOKEN_FREE, "Expected free");
                if (currentToken.type == TOKEN_LPAREN) {
                    ASTNode *call = createNode(AST_FREE_CALL, freeToken);
                    expect(TOKEN_LPAREN, "Expected (");
                    ASTNode *args = NULL;
                    ASTNode *currentArg = NULL;
                    while (currentToken.type != TOKEN_RPAREN) {
                        ASTNode *arg = parseExpression();
                        if (!args) {
                            args = arg;
                            currentArg = arg;
                        } else {
                            currentArg->next = arg;
                            currentArg = arg;
                        }
                        if (currentToken.type == TOKEN_COMMA) {
                            expect(TOKEN_COMMA, "Expected ,");
                        }
                    }
                    expect(TOKEN_RPAREN, "Expected )");
                    call->params = args;
                    
                    // 检查 free 的参数是否是内存块或指针
                    if (args && args->type == AST_IDENTIFIER) {
                        SymbolEntry *symbol = lookupSymbol(currentSymbolTable, args->token.value);
                        if (symbol) {
                            if (symbol->type == SYMBOL_MEMORY || symbol->type == SYMBOL_POINTER) {
                                if (symbol->isFreed) {
                                    fprintf(stderr, "Error: Double free of memory '%s' at line %d, column %d\n", args->token.value, currentToken.line, currentToken.column);
                                    errorCount++;
                                } else {
                                    symbol->isFreed = 1;
                                }
                            }
                        }
                    }
                    
                    expect(TOKEN_SEMICOLON, "Expected ;");
                    return call;
                }
                return NULL;
            }
        case TOKEN_OBJECT:
            {
                expect(TOKEN_OBJECT, "Expected object");
                
                Token nameToken = currentToken;
                expect(TOKEN_ID, "Expected object name");
                
                ASTNode *objDecl = createNode(AST_OBJECT_DECLARATION, nameToken);
                strcpy(objDecl->namespace, nameToken.value);
                
                expect(TOKEN_LBRACE, "Expected {");
                
                ASTNode *members = NULL;
                ASTNode *lastMember = NULL;
                
                while (currentToken.type != TOKEN_RBRACE && currentToken.type != TOKEN_EOF) {
                    if (currentToken.type == TOKEN_INT || 
                        currentToken.type == TOKEN_ID || 
                        currentToken.type == TOKEN_STRING_TYPE ||
                        currentToken.type == TOKEN_FLOAT ||
                        currentToken.type == TOKEN_OBJECT) {
                        
                        ASTNode *member = parseObjectMember();
                        if (!members) {
                            members = member;
                            lastMember = member;
                        } else {
                            lastMember->next = member;
                            lastMember = member;
                        }
                        
                        if (currentToken.type == TOKEN_COMMA) {
                            expect(TOKEN_COMMA, "Expected ,");
                        } else {
                            // 允许省略逗号
                        }
                    } else {
                        break;
                    }
                }
                
                expect(TOKEN_RBRACE, "Expected }");
                
                objDecl->body = members;
                
                addSymbol(currentSymbolTable, nameToken.value, SYMBOL_STRUCT, "object");
                
                return objDecl;
            }
        case TOKEN_FUNC:
            {
                expect(TOKEN_FUNC, "Expected func");
                
                // 检查是否有返回类型
                char returnType[20] = "";
                if (currentToken.type == TOKEN_ID) {
                    // 检查是否是类型关键字
                    if (strcmp(currentToken.value, "int") == 0 || 
                        strcmp(currentToken.value, "float") == 0 || 
                        strcmp(currentToken.value, "double") == 0 || 
                        strcmp(currentToken.value, "string") == 0 || 
                        strcmp(currentToken.value, "char") == 0) {
                        strcpy(returnType, currentToken.value);
                        expect(TOKEN_ID, "Expected return type");
                    }
                } else if (currentToken.type == TOKEN_INT || currentToken.type == TOKEN_FLOAT || 
                    currentToken.type == TOKEN_DOUBLE || currentToken.type == TOKEN_STRING_TYPE ||
                    currentToken.type == TOKEN_CHAR_TYPE) {
                    strcpy(returnType, currentToken.value);
                    expect(currentToken.type, "Expected return type");
                }
                
                Token funcToken = currentToken;
                expect(TOKEN_ID, "Expected function name");
                ASTNode *function = createNode(AST_FUNCTION_DECLARATION, funcToken);
                
                // 设置函数的返回类型
                if (returnType[0] != '\0') {
                    strcpy(function->namespace, returnType);
                }
                
                // 将函数添加到符号表
                addSymbol(currentSymbolTable, funcToken.value, SYMBOL_FUNCTION, returnType[0] != '\0' ? returnType : "int");
                
                expect(TOKEN_LPAREN, "Expected (");
                
                // 创建函数的作用域
                SymbolTable *functionScope = createSymbolTable(currentSymbolTable);
                SymbolTable *oldScope = currentSymbolTable;
                currentSymbolTable = functionScope;
                
                ASTNode *params = NULL;
                ASTNode *currentParam = NULL;
                while (currentToken.type != TOKEN_RPAREN && currentToken.type != TOKEN_EOF) {
                    if (currentToken.type == TOKEN_ID || currentToken.type == TOKEN_INT ||
                        currentToken.type == TOKEN_FLOAT || currentToken.type == TOKEN_DOUBLE ||
                        currentToken.type == TOKEN_STRING_TYPE || currentToken.type == TOKEN_CHAR_TYPE) {
                        Token paramTypeTok = currentToken;
                        char paramType[20] = "";
                        if (currentToken.type == TOKEN_INT || currentToken.type == TOKEN_FLOAT ||
                            currentToken.type == TOKEN_DOUBLE || currentToken.type == TOKEN_STRING_TYPE ||
                            currentToken.type == TOKEN_CHAR_TYPE) {
                            strcpy(paramType, currentToken.value);
                            expect(currentToken.type, "Expected type");
                        } else if (currentToken.type == TOKEN_ID) {
                            // 检查是否是类型关键字
                            if (strcmp(currentToken.value, "int") == 0 ||
                                strcmp(currentToken.value, "string") == 0 ||
                                strcmp(currentToken.value, "char") == 0) {
                                strcpy(paramType, currentToken.value);
                                expect(TOKEN_ID, "Expected type");
                            }
                        }
                        
                        // 检查参数名
                        if (currentToken.type != TOKEN_ID) {
                            fprintf(stderr, "Error: Expected parameter name at line %d, column %d\n", currentToken.line, currentToken.column);
                            errorCount++;
                            // 错误恢复：跳过直到右括号或逗号
                            while (currentToken.type != 0 && currentToken.type != TOKEN_EOF && 
                                   currentToken.type != TOKEN_RPAREN && currentToken.type != TOKEN_COMMA) {
                                currentToken = getNextToken();
                            }
                            if (currentToken.type == TOKEN_COMMA) {
                                currentToken = getNextToken();
                            }
                            continue;
                        }
                        
                        Token paramNameTok = currentToken;
                        expect(TOKEN_ID, "Expected parameter name");
                        ASTNode *param = createNode(AST_IDENTIFIER, paramNameTok);
                        if (paramType[0] != '\0') {
                            strcpy(param->namespace, paramType);
                            // 将参数添加到函数作用域的符号表中
                            addSymbol(currentSymbolTable, paramNameTok.value, SYMBOL_VARIABLE, paramType);
                        }
                        if (!params) {
                            params = param;
                            currentParam = param;
                        } else {
                            currentParam->next = param;
                            currentParam = param;
                        }
                        if (currentToken.type == TOKEN_COMMA) {
                            expect(TOKEN_COMMA, "Expected ,");
                        }
                    } else {
                        break;
                    }
                }
                expect(TOKEN_RPAREN, "Expected )");
                
                // 检查是否有左大括号
                if (currentToken.type != TOKEN_LBRACE) {
                    fprintf(stderr, "Error: Expected { at line %d, column %d\n", currentToken.line, currentToken.column);
                    errorCount++;
                    // 恢复作用域并释放函数作用域
                    currentSymbolTable = oldScope;
                    freeSymbolTable(functionScope);
                    // 错误恢复：跳过直到右大括号
                    while (currentToken.type != 0 && currentToken.type != TOKEN_EOF && 
                           currentToken.type != TOKEN_RBRACE) {
                        currentToken = getNextToken();
                    }
                    if (currentToken.type == TOKEN_RBRACE) {
                        currentToken = getNextToken();
                    }
                    // 释放已创建的函数节点
                    freeAST(function);
                    return NULL;
                }
                
                expect(TOKEN_LBRACE, "Expected {");
                ASTNode *body = createNode(AST_STATEMENT, currentToken);
                ASTNode *currentStmt = body;
                while (currentToken.type != TOKEN_RBRACE && currentToken.type != TOKEN_EOF) {
                    ASTNode *stmt = parseStatement();
                    if (stmt) {
                        currentStmt->next = stmt;
                        currentStmt = stmt;
                    } else if (currentToken.type != TOKEN_RBRACE) {
                        currentToken = getNextToken();
                    }
                }
                expect(TOKEN_RBRACE, "Expected }");
                
                // 恢复到原来的作用域
                currentSymbolTable = oldScope;
                freeSymbolTable(functionScope);
                
                if (currentToken.type == TOKEN_DOUBLE_ARROW) {
                    expect(TOKEN_DOUBLE_ARROW, "Expected =>");
                    Token aliasToken = currentToken;
                    expect(TOKEN_ID, "Expected variable name");
                    expect(TOKEN_SEMICOLON, "Expected ;");
                    function->left = createNode(AST_IDENTIFIER, aliasToken);
                } else {
                    // 如果没有函数别名，不需要分号
                }
                function->params = params;
                function->body = body;
                return function;
            }

        case TOKEN_RETURN:
            {
                ASTNode *returnStmt = createNode(AST_RETURN, currentToken);
                expect(TOKEN_RETURN, "Expected return");
                ASTNode *expr = parseExpression();
                returnStmt->left = expr;
                expect(TOKEN_SEMICOLON, "Expected ;");
                return returnStmt;
            }
        case TOKEN_FOR:
            {
                ASTNode *forLoop = createNode(AST_FOR, currentToken);
                expect(TOKEN_FOR, "Expected for");
                expect(TOKEN_LPAREN, "Expected (");
                if (currentToken.type == TOKEN_ID) {
                    ASTNode *init = createNode(AST_STATEMENT, currentToken);
                    expect(TOKEN_ID, "Expected variable name");
                    if (currentToken.type == TOKEN_ASSIGN) {
                        expect(TOKEN_ASSIGN, "Expected =");
                        expect(TOKEN_NUMBER, "Expected number");
                    }
                    forLoop->init = init;
                }
                if (currentToken.type == TOKEN_SEMICOLON) {
                    expect(TOKEN_SEMICOLON, "Expected ;");
                } else {
                    currentToken = getNextToken();
                }
                ASTNode *condition = parseExpression();
                forLoop->condition = condition;
                expect(TOKEN_SEMICOLON, "Expected ;");
                if (currentToken.type == TOKEN_ID) {
                    Token varToken = currentToken;
                    expect(TOKEN_ID, "Expected variable name");
                    expect(TOKEN_PLUS, "Expected +");
                    expect(TOKEN_PLUS, "Expected +");
                    
                    ASTNode *update = createNode(AST_BINARY_OP, (Token){TOKEN_PLUS, "+", 0, 0});
                    ASTNode *left = createNode(AST_IDENTIFIER, varToken);
                    ASTNode *right = createNode(AST_LITERAL, (Token){TOKEN_NUMBER, "1", 0, 0});
                    update->left = left;
                    update->right = right;
                    
                    ASTNode *assign = createNode(AST_ASSIGNMENT, (Token){TOKEN_ASSIGN, "=", 0, 0});
                    assign->left = createNode(AST_IDENTIFIER, varToken);
                    assign->right = update;
                    
                    ASTNode *updateStmt = createNode(AST_STATEMENT, currentToken);
                    updateStmt->left = assign;
                    
                    forLoop->update = updateStmt;
                }
                expect(TOKEN_RPAREN, "Expected )");
                expect(TOKEN_LBRACE, "Expected {");
                ASTNode *body = createNode(AST_STATEMENT, currentToken);
                ASTNode *currentStmt = body;
                while (currentToken.type != TOKEN_RBRACE) {
                    ASTNode *stmt = parseStatement();
                    if (stmt) {
                        currentStmt->next = stmt;
                        currentStmt = stmt;
                    } else if (currentToken.type != TOKEN_RBRACE) {
                        currentToken = getNextToken();
                    }
                }
                expect(TOKEN_RBRACE, "Expected }");
                forLoop->body = body;
                return forLoop;
            }
        case TOKEN_IF:
            {
                ASTNode *ifStmt = createNode(AST_IF, currentToken);
                expect(TOKEN_IF, "Expected if");
                expect(TOKEN_LPAREN, "Expected (");
                ASTNode *condition = parseExpression();
                ifStmt->condition = condition;
                expect(TOKEN_RPAREN, "Expected )");
                expect(TOKEN_LBRACE, "Expected {");
                ASTNode *ifBody = createNode(AST_STATEMENT, currentToken);
                ASTNode *currentStmt = ifBody;
                while (currentToken.type != TOKEN_RBRACE) {
                    ASTNode *stmt = parseStatement();
                    if (stmt) {
                        currentStmt->next = stmt;
                        currentStmt = stmt;
                    } else if (currentToken.type != TOKEN_RBRACE) {
                        currentToken = getNextToken();
                    }
                }
                expect(TOKEN_RBRACE, "Expected }");
                ifStmt->body = ifBody;
                if (currentToken.type == TOKEN_ELSE) {
                    expect(TOKEN_ELSE, "Expected else");
                    if (currentToken.type == TOKEN_IF) {
                        ASTNode *elseIf = parseStatement();
                        ifStmt->elseIfChain = elseIf;
                    } else {
                        expect(TOKEN_LBRACE, "Expected {");
                        ASTNode *elseBody = createNode(AST_STATEMENT, currentToken);
                        currentStmt = elseBody;
                        while (currentToken.type != TOKEN_RBRACE) {
                            ASTNode *stmt = parseStatement();
                            if (stmt) {
                                currentStmt->next = stmt;
                                currentStmt = stmt;
                            } else if (currentToken.type != TOKEN_RBRACE) {
                                currentToken = getNextToken();
                            }
                        }
                        expect(TOKEN_RBRACE, "Expected }");
                        ifStmt->elseBody = elseBody;
                    }
                }
                return ifStmt;
            }
        case TOKEN_WHILE:
            {
                ASTNode *whileLoop = createNode(AST_WHILE, currentToken);
                expect(TOKEN_WHILE, "Expected while");
                expect(TOKEN_LPAREN, "Expected (");
                ASTNode *condition = parseExpression();
                whileLoop->condition = condition;
                expect(TOKEN_RPAREN, "Expected )");
                expect(TOKEN_LBRACE, "Expected {");
                ASTNode *body = createNode(AST_STATEMENT, currentToken);
                ASTNode *currentStmt = body;
                while (currentToken.type != TOKEN_RBRACE) {
                    ASTNode *stmt = parseStatement();
                    if (stmt) {
                        currentStmt->next = stmt;
                        currentStmt = stmt;
                    } else if (currentToken.type != TOKEN_RBRACE) {
                        currentToken = getNextToken();
                    }
                }
                expect(TOKEN_RBRACE, "Expected }");
                whileLoop->body = body;
                return whileLoop;
            }
        case TOKEN_BREAK:
            {
                expect(TOKEN_BREAK, "Expected break");
                expect(TOKEN_SEMICOLON, "Expected ;");
                return createNode(AST_BREAK, currentToken);
            }
        case TOKEN_SWITCH:
            {
                ASTNode *switchStmt = createNode(AST_SWITCH, currentToken);
                expect(TOKEN_SWITCH, "Expected switch");
                expect(TOKEN_LPAREN, "Expected (");
                ASTNode *expr = parseExpression();
                switchStmt->condition = expr;
                expect(TOKEN_RPAREN, "Expected )");
                expect(TOKEN_LBRACE, "Expected {");
                
                ASTNode *cases = NULL;
                ASTNode *currentCase = NULL;
                
                while (currentToken.type != TOKEN_RBRACE && currentToken.type != TOKEN_EOF) {
                    if (currentToken.type == TOKEN_CASE) {
                        ASTNode *caseNode = createNode(AST_CASE, currentToken);
                        expect(TOKEN_CASE, "Expected case");
                        ASTNode *caseValue = parseExpression();
                        caseNode->left = caseValue;
                        expect(TOKEN_COLON, "Expected :");
                        
                        ASTNode *caseBody = createNode(AST_STATEMENT, currentToken);
                        ASTNode *currentBodyStmt = caseBody;
                        while (currentToken.type != TOKEN_CASE && 
                               currentToken.type != TOKEN_DEFAULT && 
                               currentToken.type != TOKEN_RBRACE && 
                               currentToken.type != TOKEN_EOF) {
                            ASTNode *stmt = parseStatement();
                            if (stmt) {
                                currentBodyStmt->next = stmt;
                                currentBodyStmt = stmt;
                            } else {
                                currentToken = getNextToken();
                            }
                        }
                        caseNode->body = caseBody;
                        
                        if (!cases) {
                            cases = caseNode;
                            currentCase = caseNode;
                        } else {
                            currentCase->next = caseNode;
                            currentCase = caseNode;
                        }
                    } else if (currentToken.type == TOKEN_DEFAULT) {
                        ASTNode *defaultNode = createNode(AST_CASE, currentToken);
                        expect(TOKEN_DEFAULT, "Expected default");
                        expect(TOKEN_COLON, "Expected :");
                        
                        ASTNode *defaultBody = createNode(AST_STATEMENT, currentToken);
                        ASTNode *currentBodyStmt = defaultBody;
                        while (currentToken.type != TOKEN_RBRACE && currentToken.type != TOKEN_EOF) {
                            ASTNode *stmt = parseStatement();
                            if (stmt) {
                                currentBodyStmt->next = stmt;
                                currentBodyStmt = stmt;
                            } else {
                                currentToken = getNextToken();
                            }
                        }
                        defaultNode->body = defaultBody;
                        strcpy(defaultNode->namespace, "default");
                        
                        if (!cases) {
                            cases = defaultNode;
                            currentCase = defaultNode;
                        } else {
                            currentCase->next = defaultNode;
                            currentCase = defaultNode;
                        }
                    } else {
                        currentToken = getNextToken();
                    }
                }
                expect(TOKEN_RBRACE, "Expected }");
                switchStmt->body = cases;
                return switchStmt;
            }
        default:
            {
                // 处理表达式语句（如函数调用）
                ASTNode *expr = parseExpression();
                if (expr) {
                    expect(TOKEN_SEMICOLON, "Expected ;");
                    ASTNode *stmt = createNode(AST_STATEMENT, currentToken);
                    stmt->left = expr;
                    return stmt;
                }
                return NULL;
            }
    }
}

ASTNode *parseExpression() {
    ASTNode *left = parseTerm();
    while (left && currentToken.type != 0 && currentToken.type != TOKEN_EOF && currentToken.type != TOKEN_SEMICOLON && currentToken.type != TOKEN_RPAREN && currentToken.type != TOKEN_COMMA) {
        if (currentToken.type == TOKEN_PLUS) {
            Token op = currentToken;
            expect(TOKEN_PLUS, "Expected +");
            ASTNode *right = parseTerm();
            ASTNode *node = createNode(AST_BINARY_OP, op);
            node->left = left;
            node->right = right;
            left = node;
        } else if (currentToken.type == TOKEN_MINUS) {
            Token op = currentToken;
            expect(TOKEN_MINUS, "Expected -");
            ASTNode *right = parseTerm();
            ASTNode *node = createNode(AST_BINARY_OP, op);
            node->left = left;
            node->right = right;
            left = node;
        } else if (currentToken.type == TOKEN_AMPERSAND) {
            Token op = currentToken;
            expect(TOKEN_AMPERSAND, "Expected &");
            // 解析右侧表达式，支持 endl 等特殊关键字
            ASTNode *right = parseTerm();
            ASTNode *node = createNode(AST_STRING_CONCAT, op);
            node->left = left;
            node->right = right;
            left = node;
        } else if (currentToken.type == TOKEN_EQ) {
            Token op = currentToken;
            expect(TOKEN_EQ, "Expected ==");
            ASTNode *right = parseTerm();
            ASTNode *node = createNode(AST_BINARY_OP, op);
            node->left = left;
            node->right = right;
            left = node;
        } else if (currentToken.type == TOKEN_NE) {
            Token op = currentToken;
            expect(TOKEN_NE, "Expected !=");
            ASTNode *right = parseTerm();
            ASTNode *node = createNode(AST_BINARY_OP, op);
            node->left = left;
            node->right = right;
            left = node;
        } else if (currentToken.type == TOKEN_LT) {
            Token op = currentToken;
            expect(TOKEN_LT, "Expected <");
            ASTNode *right = parseTerm();
            ASTNode *node = createNode(AST_BINARY_OP, op);
            node->left = left;
            node->right = right;
            left = node;
        } else if (currentToken.type == TOKEN_LE) {
            Token op = currentToken;
            expect(TOKEN_LE, "Expected <=");
            ASTNode *right = parseTerm();
            ASTNode *node = createNode(AST_BINARY_OP, op);
            node->left = left;
            node->right = right;
            left = node;
        } else if (currentToken.type == TOKEN_GT) {
            Token op = currentToken;
            expect(TOKEN_GT, "Expected >");
            ASTNode *right = parseTerm();
            ASTNode *node = createNode(AST_BINARY_OP, op);
            node->left = left;
            node->right = right;
            left = node;
        } else if (currentToken.type == TOKEN_GE) {
            Token op = currentToken;
            expect(TOKEN_GE, "Expected >=");
            ASTNode *right = parseTerm();
            ASTNode *node = createNode(AST_BINARY_OP, op);
            node->left = left;
            node->right = right;
            left = node;
        } else {
            break;
        }
    }
    return left;
}

ASTNode *parseTerm() {
    ASTNode *left = parseFactor();
    while (currentToken.type == TOKEN_MUL || currentToken.type == TOKEN_DIV) {
        Token op = currentToken;
        if (currentToken.type == TOKEN_MUL) {
            expect(TOKEN_MUL, "Expected *");
        } else if (currentToken.type == TOKEN_DIV) {
            expect(TOKEN_DIV, "Expected /");
        }
        ASTNode *right = parseFactor();
        ASTNode *node = createNode(AST_BINARY_OP, op);
        node->left = left;
        node->right = right;
        left = node;
    }
    return left;
}

ASTNode *parseFactor() {
    if (currentToken.type == TOKEN_LPAREN) {
        expect(TOKEN_LPAREN, "Expected (");
        ASTNode *expr = parseExpression();
        expect(TOKEN_RPAREN, "Expected )");
        return expr;
    } else if (currentToken.type == TOKEN_PLUS || currentToken.type == TOKEN_MINUS || currentToken.type == TOKEN_NOT) {
        Token op = currentToken;
        
        if (currentToken.type == TOKEN_PLUS) {
            expect(TOKEN_PLUS, "Expected +");
        } else if (currentToken.type == TOKEN_MINUS) {
            expect(TOKEN_MINUS, "Expected -");
        } else if (currentToken.type == TOKEN_NOT) {
            expect(TOKEN_NOT, "Expected !");
        }
        
        ASTNode *expr = parseFactor();
        
        // 处理一元操作
        ASTNode *node = createNode(AST_UNARY_OP, op);
        node->left = expr;
        return node;
    } else {
        return parsePrimary();
    }
}

ASTNode *parsePrimary() {
    // 处理二级宏调用
    if (currentToken.type == TOKEN_SECONDARY_MACRO) {
        Token macroToken = currentToken;
        expect(TOKEN_SECONDARY_MACRO, "Expected secondary macro");
        
        // 创建二级宏调用节点
        ASTNode *macroCall = createNode(AST_SECONDARY_MACRO_INVOCATION, macroToken);
        strcpy(macroCall->namespace, macroToken.value);
        
        // 处理参数
        if (currentToken.type == TOKEN_LPAREN) {
            expect(TOKEN_LPAREN, "Expected (");
            ASTNode *args = NULL;
            ASTNode *currentArg = NULL;
            while (currentToken.type != TOKEN_RPAREN) {
                ASTNode *arg = parseExpression();
                if (!args) {
                    args = arg;
                    currentArg = arg;
                } else {
                    currentArg->next = arg;
                    currentArg = arg;
                }
                if (currentToken.type == TOKEN_COMMA) {
                    expect(TOKEN_COMMA, "Expected ,");
                }
            }
            expect(TOKEN_RPAREN, "Expected )");
            macroCall->params = args;
        }
        
        return macroCall;
    }
    
    // 处理 asm 内联汇编
    if (currentToken.type == TOKEN_ASM) {
        Token asmToken = currentToken;
        expect(TOKEN_ASM, "Expected asm");
        expect(TOKEN_LPAREN, "Expected (");
        
        if (currentToken.type == TOKEN_STRING) {
            Token asmCode = currentToken;
            expect(TOKEN_STRING, "Expected assembly code string");
            
            ASTNode *asmNode = createNode(AST_ASM, asmToken);
            // 将汇编代码存储在 token.value 中
            strcpy(asmNode->filename, asmCode.value);
            
            expect(TOKEN_RPAREN, "Expected )");
            
            // 处理数组索引访问 [0] 或 [1]
            if (currentToken.type == TOKEN_LBRACKET) {
                expect(TOKEN_LBRACKET, "Expected [");
                if (currentToken.type == TOKEN_NUMBER) {
                    int index = atoi(currentToken.value);
                    expect(TOKEN_NUMBER, "Expected index");
                    asmNode->left = createNode(AST_LITERAL, currentToken);
                    expect(TOKEN_RBRACKET, "Expected ]");
                }
            }
            
            return asmNode;
        } else {
            fprintf(stderr, "Error: Expected assembly code string at line %d, column %d\n", currentToken.line, currentToken.column);
            errorCount++;
            return NULL;
        }
    }
    
    // 特殊处理类型转换函数
    if (currentToken.type == TOKEN_ID || currentToken.type == TOKEN_INT || currentToken.type == TOKEN_STRING || 
        currentToken.type == TOKEN_CHAR || currentToken.type == TOKEN_DIR || 
        currentToken.type == TOKEN_STRING_TYPE || currentToken.type == TOKEN_CHAR_TYPE) {
        Token idToken = currentToken;
        char tokenValue[100];
        strcpy(tokenValue, idToken.value);
        
        // 检查是否是类型转换函数
            if (strcmp(tokenValue, "string") == 0 || 
                strcmp(tokenValue, "int") == 0 || 
                strcmp(tokenValue, "char") == 0 || 
                strcmp(tokenValue, "dir") == 0) {
                // 消耗当前 token
                if (currentToken.type == TOKEN_ID || currentToken.type == TOKEN_STRING_TYPE || currentToken.type == TOKEN_CHAR_TYPE) {
                    currentToken = getNextToken();
                }
                
                if (currentToken.type == TOKEN_LPAREN) {
                    // 处理类型转换函数
                    ASTNode *cast = createNode(AST_FUNCTION_CALL, idToken);
                    expect(TOKEN_LPAREN, "Expected (");
                    ASTNode *arg = parseExpression();
                    cast->params = arg;
                    expect(TOKEN_RPAREN, "Expected )");
                    return cast;
                }
            }
    }
    
    // 处理其他情况
    switch (currentToken.type) {
        case TOKEN_ID:
            {
                Token idToken = currentToken;
                expect(TOKEN_ID, "Expected identifier");

                if (currentToken.type == TOKEN_LPAREN) {
                    // 普通函数调用
                    ASTNode *call = createNode(AST_FUNCTION_CALL, idToken);
                    expect(TOKEN_LPAREN, "Expected (");
                    ASTNode *args = NULL;
                    ASTNode *currentArg = NULL;
                    while (currentToken.type != TOKEN_RPAREN) {
                        ASTNode *arg = parseExpression();
                        if (!args) {
                            args = arg;
                            currentArg = arg;
                        } else {
                            currentArg->next = arg;
                            currentArg = arg;
                        }
                        if (currentToken.type == TOKEN_COMMA) {
                            expect(TOKEN_COMMA, "Expected ,");
                        }
                    }
                    expect(TOKEN_RPAREN, "Expected )");
                    if (currentToken.type == TOKEN_DOUBLE_ARROW) {
                        // 处理 => 语法
                        expect(TOKEN_DOUBLE_ARROW, "Expected =>");
                        Token varToken = currentToken;
                        expect(TOKEN_ID, "Expected variable name");
                        expect(TOKEN_SEMICOLON, "Expected ;");
                        call->params = args;
                        // 创建变量声明节点
                        ASTNode *assign = createNode(AST_VARIABLE_DECLARATION, varToken);
                        assign->left = createNode(AST_IDENTIFIER, varToken);
                        assign->right = call;
                        // 将变量添加到符号表
                        addSymbol(currentSymbolTable, varToken.value, SYMBOL_VARIABLE, "int");
                        return assign;
                    }
                    call->params = args;
                    return call;
                } else {
                    // 普通标识符
                    // 检查符号是否存在
                    SymbolEntry *symbol = lookupSymbol(currentSymbolTable, idToken.value);
                    if (!symbol) {
                        fprintf(stderr, "Error: Undefined symbol '%s' at line %d, column %d\n", idToken.value, idToken.line, idToken.column);
                        errorCount++;
                    }
                    ASTNode *node = createNode(AST_IDENTIFIER, idToken);
                    // 如果符号存在，保存其类型
                    if (symbol) {
                        strcpy(node->namespace, symbol->varType);
                    }
                    
                    // 检查是否是结构体成员访问 (如 obj.member) 或指针解引用 (如 ptr.value)
                    while (currentToken.type == TOKEN_DOT) {
                        expect(TOKEN_DOT, "Expected .");
                        Token memberToken = currentToken;
                        expect(TOKEN_ID, "Expected member name");
                        
                        // 检查是否是指针解引用 (ptr.value)
                        if (strcmp(memberToken.value, "value") == 0) {
                            // 创建解引用节点
                            ASTNode *dereference = createNode(AST_DEREFERENCE, memberToken);
                            dereference->left = node;
                            node = dereference;
                        } else {
                            // 创建成员访问节点
                            ASTNode *memberAccess = createNode(AST_OBJECT_ACCESS, memberToken);
                            memberAccess->left = node; // 左侧是对象
                            memberAccess->right = createNode(AST_IDENTIFIER, memberToken); // 右侧是成员名
                            node = memberAccess;
                        }
                    }
                    
                    // 检查是否是方括号访问 (如 obj["name"] 或 obj[variable])
                    if (currentToken.type == TOKEN_LBRACKET) {
                        expect(TOKEN_LBRACKET, "Expected [");
                        ASTNode *key = parseExpression();
                        expect(TOKEN_RBRACKET, "Expected ]");
                        
                        ASTNode *bracketAccess = createNode(AST_OBJECT_ACCESS, idToken);
                        bracketAccess->left = node; // 左侧是对象
                        bracketAccess->right = key; // 右侧是键表达式
                        node = bracketAccess;
                    }
                    
                    return node;
                }
            }
        case TOKEN_ALLOC:
            {
                // 处理 alloc 函数调用
                Token allocToken = currentToken;
                expect(TOKEN_ALLOC, "Expected alloc");
                if (currentToken.type == TOKEN_LPAREN) {
                    ASTNode *call = createNode(AST_ALLOC_CALL, allocToken);
                    expect(TOKEN_LPAREN, "Expected (");
                    ASTNode *args = NULL;
                    ASTNode *currentArg = NULL;
                    while (currentToken.type != TOKEN_RPAREN) {
                        ASTNode *arg = parseExpression();
                        if (!args) {
                            args = arg;
                            currentArg = arg;
                        } else {
                            currentArg->next = arg;
                            currentArg = arg;
                        }
                        if (currentToken.type == TOKEN_COMMA) {
                            expect(TOKEN_COMMA, "Expected ,");
                        }
                    }
                    expect(TOKEN_RPAREN, "Expected )");
                    call->params = args;
                    return call;
                }
                return NULL;
            }
        case TOKEN_FREE:
            {
                // 处理 free 函数调用
                Token freeToken = currentToken;
                expect(TOKEN_FREE, "Expected free");
                if (currentToken.type == TOKEN_LPAREN) {
                    ASTNode *call = createNode(AST_FREE_CALL, freeToken);
                    expect(TOKEN_LPAREN, "Expected (");
                    ASTNode *args = NULL;
                    ASTNode *currentArg = NULL;
                    while (currentToken.type != TOKEN_RPAREN) {
                        ASTNode *arg = parseExpression();
                        if (!args) {
                            args = arg;
                            currentArg = arg;
                        } else {
                            currentArg->next = arg;
                            currentArg = arg;
                        }
                        if (currentToken.type == TOKEN_COMMA) {
                            expect(TOKEN_COMMA, "Expected ,");
                        }
                    }
                    expect(TOKEN_RPAREN, "Expected )");
                    call->params = args;
                    
                    // 检查 free 的参数是否是内存块或指针
                    if (args && args->type == AST_IDENTIFIER) {
                        SymbolEntry *symbol = lookupSymbol(currentSymbolTable, args->token.value);
                        if (symbol) {
                            if (symbol->type == SYMBOL_MEMORY || symbol->type == SYMBOL_POINTER) {
                                if (symbol->isFreed) {
                                    fprintf(stderr, "Error: Double free of memory '%s' at line %d, column %d\n", args->token.value, currentToken.line, currentToken.column);
                                    errorCount++;
                                } else {
                                    symbol->isFreed = 1;
                                }
                            }
                        }
                    }
                    
                    expect(TOKEN_SEMICOLON, "Expected ;");
                    return call;
                }
                return NULL;
            }
        case TOKEN_RUN:
            {
                Token runToken = currentToken;
                expect(TOKEN_RUN, "Expected run");
                if (currentToken.type == TOKEN_LPAREN) {
                    ASTNode *call = createNode(AST_RUN_CALL, runToken);
                    expect(TOKEN_LPAREN, "Expected (");
                    ASTNode *args = NULL;
                    ASTNode *currentArg = NULL;
                    while (currentToken.type != TOKEN_RPAREN) {
                        ASTNode *arg = parseExpression();
                        if (!args) {
                            args = arg;
                            currentArg = arg;
                        } else {
                            currentArg->next = arg;
                            currentArg = arg;
                        }
                        if (currentToken.type == TOKEN_COMMA) {
                            expect(TOKEN_COMMA, "Expected ,");
                        }
                    }
                    expect(TOKEN_RPAREN, "Expected )");
                    call->params = args;

                    expect(TOKEN_SEMICOLON, "Expected ;");
                    return call;
                }
                return NULL;
            }
        case TOKEN_NUMBER:
        case TOKEN_FLOAT_NUMBER:
            {
                ASTNode *node = createNode(AST_LITERAL, currentToken);
                expect(currentToken.type, "Expected value");
                return node;
            }
        case TOKEN_STRING:
            {
                ASTNode *node = createNode(AST_LITERAL, currentToken);
                expect(TOKEN_STRING, "Expected string");
                return node;
            }
        case TOKEN_CHAR:
            {
                ASTNode *node = createNode(AST_LITERAL, currentToken);
                expect(TOKEN_CHAR, "Expected char");
                return node;
            }
        case TOKEN_DIR:
            {
                ASTNode *node = createNode(AST_LITERAL, currentToken);
                expect(TOKEN_DIR, "Expected dir");
                return node;
            }
        case TOKEN_ENDL:
            {
                ASTNode *node = createNode(AST_LITERAL, currentToken);
                expect(TOKEN_ENDL, "Expected endl");
                return node;
            }
        default:
            fprintf(stderr, "Error: Unexpected token %d at line %d, column %d\n", currentToken.type, currentToken.line, currentToken.column);
            errorCount++;
            
            // 错误恢复：跳过当前标记，直到找到一个可能的恢复点
            while (currentToken.type != 0 && 
                   currentToken.type != TOKEN_EOF && 
                   currentToken.type != TOKEN_SEMICOLON && 
                   currentToken.type != TOKEN_RPAREN && 
                   currentToken.type != TOKEN_RBRACE && 
                   currentToken.type != TOKEN_LBRACE && 
                   currentToken.type != TOKEN_COMMA) {
                currentToken = getNextToken();
            }
            
            return NULL;
    }
}

// 清理函数中创建但未返回的AST节点
void cleanupUnreturnedAST(ASTNode *node) {
    if (!node) return;
    freeAST(node);
}

ASTNode *parseObjectMember() {
    if (currentToken.type == TOKEN_OBJECT) {
        return parseStatement();
    }
    
    if (currentToken.type == TOKEN_FUNC) {
        return parseStatement();
    }
    
    Token typeToken = currentToken;
    // 类型可以是 TOKEN_ID、TOKEN_INT、TOKEN_FLOAT、TOKEN_STRING_TYPE 等
    if (currentToken.type == TOKEN_ID || 
        currentToken.type == TOKEN_INT || 
        currentToken.type == TOKEN_FLOAT || 
        currentToken.type == TOKEN_STRING_TYPE) {
        currentToken = getNextToken();
    } else {
        fprintf(stderr, "Error: Expected type at line %d, column %d\n", currentToken.line, currentToken.column);
        errorCount++;
        return NULL;
    }
    
    Token nameToken = currentToken;
    expect(TOKEN_ID, "Expected member name");
    
    ASTNode *member = createNode(AST_OBJECT_MEMBER, nameToken);
    strcpy(member->namespace, typeToken.value);
    
    if (currentToken.type == TOKEN_ASSIGN) {
        expect(TOKEN_ASSIGN, "Expected =");
        member->right = parseExpression();
    }
    
    // 消费分号
    if (currentToken.type == TOKEN_SEMICOLON) {
        expect(TOKEN_SEMICOLON, "Expected ;");
    }
    
    return member;
}

ASTNode *parseMacroDefinition() {
    Token nameTok = currentToken;
    expect(TOKEN_ID, "Expected macro name");
    
    ASTNode *macroNode = createNode(AST_MACRO_DEFINITION, nameTok);
    strcpy(macroNode->namespace, nameTok.value);
    
    if (currentToken.type == TOKEN_LPAREN) {
        expect(TOKEN_LPAREN, "Expected (");
        
        ASTNode *params = NULL;
        ASTNode *currentParam = NULL;
        
        while (currentToken.type != TOKEN_RPAREN) {
            Token paramTok = currentToken;
            expect(TOKEN_ID, "Expected parameter name");
            
            ASTNode *paramNode = createNode(AST_IDENTIFIER, paramTok);
            
            if (!params) {
                params = paramNode;
                currentParam = paramNode;
            } else {
                currentParam->next = paramNode;
                currentParam = paramNode;
            }
            
            if (currentToken.type == TOKEN_COMMA) {
                expect(TOKEN_COMMA, "Expected ,");
            }
        }
        
        expect(TOKEN_RPAREN, "Expected )");
        macroNode->params = params;
    }
    
    if (currentToken.type == TOKEN_ASSIGN) {
        expect(TOKEN_ASSIGN, "Expected =");
        
        ASTNode *expr = parseExpression();
        macroNode->body = expr;
        expect(TOKEN_SEMICOLON, "Expected ;");
    } else if (currentToken.type == TOKEN_LBRACE) {
        expect(TOKEN_LBRACE, "Expected {");
        
        ASTNode *body = NULL;
        ASTNode *currentStmt = NULL;
        
        while (currentToken.type != TOKEN_RBRACE && currentToken.type != TOKEN_EOF) {
            ASTNode *stmt = parseStatement();
            if (stmt) {
                if (!body) {
                    body = stmt;
                    currentStmt = stmt;
                } else {
                    currentStmt->next = stmt;
                    currentStmt = stmt;
                }
            } else {
                currentToken = getNextToken();
            }
        }
        
        expect(TOKEN_RBRACE, "Expected }");
        macroNode->body = body;
    }
    
    return macroNode;
}

ASTNode *createNode(ASTNodeType type, Token token) {
    ASTNode *node = allocASTNode();
    if (!node) {
        fprintf(stderr, "Error: Failed to allocate AST node from pool\n");
        return NULL;
    }
    node->type = type;
    node->token = token;
    return node;
}

void expect(TokenType type, const char *message) {
    if (currentToken.type == TOKEN_EOF) {
        return;
    }
    if (currentToken.type != type) {
        fprintf(stderr, "Error: %s at line %d, column %d\n", message, currentToken.line, currentToken.column);
        errorCount++;
        return;
    }
    currentToken = getNextToken();
}

// 符号表相关函数
SymbolTable *createSymbolTable(SymbolTable *parent) {
    SymbolTable *table = (SymbolTable *)malloc(sizeof(SymbolTable));
    table->entries = NULL;
    table->parent = parent;
    return table;
}

void addSymbol(SymbolTable *table, const char *name, SymbolType type, const char *varType) {
    // 检查符号是否已存在
    SymbolEntry *existing = lookupSymbol(table, name);
    if (existing) {
        fprintf(stderr, "Error: Symbol '%s' already defined at line %d, column %d\n", name, currentToken.line, currentToken.column);
        errorCount++;
        return;
    }
    
    SymbolEntry *entry = (SymbolEntry *)malloc(sizeof(SymbolEntry));
    strcpy(entry->name, name);
    entry->type = type;
    strcpy(entry->varType, varType);
    entry->isMemoryOwner = 0;
    entry->isFreed = 0;
    entry->next = table->entries;
    table->entries = entry;
}

SymbolEntry *lookupSymbol(SymbolTable *table, const char *name) {
    while (table) {
        SymbolEntry *entry = table->entries;
        while (entry) {
            if (strcmp(entry->name, name) == 0) {
                return entry;
            }
            entry = entry->next;
        }
        table = table->parent;
    }
    return NULL;
}

void checkMemoryLeaks(SymbolTable *table) {
    if (!table) return;
    
    // 递归检查父作用域
    if (table->parent) {
        checkMemoryLeaks(table->parent);
    }
    
    // 检查当前作用域
    SymbolEntry *entry = table->entries;
    while (entry) {
        if (entry->type == SYMBOL_MEMORY && entry->isMemoryOwner && !entry->isFreed) {
            fprintf(stderr, "Error: Memory leak detected: '%s' was not freed\n", entry->name);
            errorCount++;
        }
        entry = entry->next;
    }
}

void freeSymbolTable(SymbolTable *table) {
    if (!table) return;
    
    // 检查内存泄漏
    checkMemoryLeaks(table);
    
    SymbolEntry *entry = table->entries;
    while (entry) {
        SymbolEntry *next = entry->next;
        free(entry);
        entry = next;
    }
    
    free(table);
}

// 释放单个AST节点（不递归）- 内存池分配器不需要单独释放
void freeSingleASTNode(ASTNode *node) {
    if (!node) return;
    // 内存池分配的节点不需要单独释放
    // 清理指针以防止悬空引用
    node->left = NULL;
    node->right = NULL;
    node->next = NULL;
    node->body = NULL;
    node->params = NULL;
}

// 内存管理相关函数 - 内存池分配器不需要递归释放
void freeAST(ASTNode *node) {
    if (!node) return;
    
    // 递归清理子节点指针
    freeAST(node->left);
    freeAST(node->right);
    freeAST(node->next);
    freeAST(node->body);
    freeAST(node->params);
    freeAST(node->condition);
    freeAST(node->init);
    freeAST(node->update);
    freeAST(node->elseBody);
    freeAST(node->elseIfChain);
    
    // 内存池分配的节点不需要单独释放
    // 清理当前节点指针
    node->left = NULL;
    node->right = NULL;
    node->next = NULL;
}

// 释放AST链表（不递归释放子节点）
void freeASTList(ASTNode *head) {
    ASTNode *current = head;
    while (current) {
        ASTNode *next = current->next;
        current->next = NULL;
        freeAST(current);
        current = next;
    }
}

// ==================== AST 内存池分配器实现 ====================

void initASTPool() {
    astPool.first = NULL;
    astPool.current = NULL;
    astPool.totalAllocated = 0;
    astPool.totalNodes = 0;
}

static ASTPoolBlock *createPoolBlock() {
    ASTPoolBlock *block = (ASTPoolBlock *)malloc(sizeof(ASTPoolBlock));
    if (!block) {
        fprintf(stderr, "Error: Failed to allocate AST pool block\n");
        return NULL;
    }
    block->next = NULL;
    block->used = 0;
    astPool.totalAllocated++;
    return block;
}

ASTNode *allocASTNode() {
    if (!astPool.current || astPool.current->used >= AST_POOL_BLOCK_SIZE) {
        ASTPoolBlock *newBlock = createPoolBlock();
        if (!newBlock) {
            return NULL;
        }
        if (!astPool.first) {
            astPool.first = newBlock;
        } else {
            astPool.current->next = newBlock;
        }
        astPool.current = newBlock;
    }
    
    ASTNode *node = &astPool.current->nodes[astPool.current->used++];
    astPool.totalNodes++;
    
    // 初始化节点
    node->type = (ASTNodeType)0;
    node->left = NULL;
    node->right = NULL;
    node->next = NULL;
    node->body = NULL;
    node->params = NULL;
    node->condition = NULL;
    node->init = NULL;
    node->update = NULL;
    node->elseBody = NULL;
    node->elseIfChain = NULL;
    node->namespaceExpr = NULL;
    node->namespace[0] = '\0';
    node->filename[0] = '\0';
    
    return node;
}

void cleanupASTPool() {
    ASTPoolBlock *current = astPool.first;
    while (current) {
        ASTPoolBlock *next = current->next;
        free(current);
        current = next;
    }
    astPool.first = NULL;
    astPool.current = NULL;
    astPool.totalAllocated = 0;
    astPool.totalNodes = 0;
}

void printASTPoolStats() {
    fprintf(stderr, "AST Pool Statistics:\n");
    fprintf(stderr, "  Total blocks allocated: %d\n", astPool.totalAllocated);
    fprintf(stderr, "  Total nodes allocated: %d\n", astPool.totalNodes);
    fprintf(stderr, "  Memory used: %d bytes\n", astPool.totalAllocated * sizeof(ASTPoolBlock));
}
