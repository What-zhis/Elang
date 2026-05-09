#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lexer.h"
#include "parser.h"

// 全局变量
extern SymbolTable *currentSymbolTable;

FILE *output;
int indentLevel = 0;

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
void generateSwitchStatement(ASTNode *node);
void generateBreakStatement(ASTNode *node);
void generateOutput(ASTNode *node);
void generateInput(ASTNode *node);
void generateVariableDeclaration(ASTNode *node);
void generateImport(ASTNode *node);
void generateStructDeclaration(ASTNode *node);
void generateEnumDeclaration(ASTNode *node);
void generateMemberAccess(ASTNode *node);
void generateAsm(ASTNode *node);
void generateSecondaryMacroInvocation(ASTNode *node);
void generateObjectDeclaration(ASTNode *node);
void generateObjectMember(ASTNode *node);
void generateObjectAccess(ASTNode *node);
void indent();

void generateCode(ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM: generateProgram(node); break;
        case AST_STATEMENT: 
            indent();
            if (node->left) {
                generateCode(node->left);
                fprintf(output, ";");
            }
            fprintf(output, "\n");
            generateStatement(node); 
            break;
        case AST_EXPRESSION: generateExpression(node); break;
        case AST_VARIABLE_DECLARATION: generateVariableDeclaration(node); break;
        case AST_FUNCTION_DECLARATION: generateFunctionDeclaration(node); break;
        case AST_FUNCTION_CALL: generateFunctionCall(node); break;
        case AST_RETURN: generateReturn(node); break;
        case AST_FOR: generateForLoop(node); break;
        case AST_WHILE: generateWhileLoop(node); break;
        case AST_IF: generateIfStatement(node); break;
        case AST_SWITCH: generateSwitchStatement(node); break;
        case AST_BREAK: generateBreakStatement(node); break;
        case AST_OUTPUT: generateOutput(node); break;
        case AST_INPUT: generateInput(node); break;
        case AST_IMPORT: generateImport(node); break;
        case AST_STRING_CONCAT:
        case AST_BINARY_OP: generateBinaryOp(node); break;
        case AST_UNARY_OP: generateUnaryOp(node); break;
        case AST_LITERAL: generateLiteral(node); break;
        case AST_IDENTIFIER: generateIdentifier(node); break;
        case AST_STRUCT_DECLARATION: generateStructDeclaration(node); break;
        case AST_ENUM_DECLARATION: generateEnumDeclaration(node); break;
        case AST_MEMBER_ACCESS: generateMemberAccess(node); break;
        case AST_POINTER_DECLARATION: generatePointerDeclaration(node); break;
        case AST_MEMORY_DECLARATION: generateMemoryDeclaration(node); break;
        case AST_DEREFERENCE: generateDereference(node); break;
        case AST_ADDRESS_OF: generateAddressOf(node); break;
        case AST_POINTER_OP: generatePointerOp(node); break;
        case AST_ALLOC_CALL: generateAllocCall(node); break;
        case AST_FREE_CALL: generateFreeCall(node); break;
        case AST_RUN_CALL: generateRunCall(node); break;
        case AST_ASM: generateAsm(node); break;
        case AST_SECONDARY_MACRO_INVOCATION: generateSecondaryMacroInvocation(node); break;
        case AST_OBJECT_DECLARATION: generateObjectDeclaration(node); break;
        case AST_OBJECT_MEMBER: generateObjectMember(node); break;
        case AST_OBJECT_ACCESS: generateObjectAccess(node); break;
        default: break;
    }
}

void generateProgram(ASTNode *node) {
    fprintf(output, "#include <stdio.h>\n");
    fprintf(output, "#include <stdlib.h>\n");
    fprintf(output, "#include <string.h>\n\n");
    fprintf(output, "static char __string_buffers[16][256];\n");
    fprintf(output, "static int __string_buf_idx = 0;\n");
    fprintf(output, "static char *__string_int(int x) {\n");
    fprintf(output, "    char *buf = __string_buffers[__string_buf_idx];\n");
    fprintf(output, "    __string_buf_idx = (__string_buf_idx + 1) %% 16;\n");
    fprintf(output, "    sprintf(buf, \"%%d\", x);\n");
    fprintf(output, "    return buf;\n");
    fprintf(output, "}\n");
    fprintf(output, "static char *__string_float(float x) {\n");
    fprintf(output, "    char *buf = __string_buffers[__string_buf_idx];\n");
    fprintf(output, "    __string_buf_idx = (__string_buf_idx + 1) %% 16;\n");
    fprintf(output, "    sprintf(buf, \"%%f\", x);\n");
    fprintf(output, "    return buf;\n");
    fprintf(output, "}\n");
    fprintf(output, "static char *__to_string_int(int x) {\n");
    fprintf(output, "    char *buf = __string_buffers[__string_buf_idx];\n");
    fprintf(output, "    __string_buf_idx = (__string_buf_idx + 1) %% 16;\n");
    fprintf(output, "    sprintf(buf, \"%%d\", x);\n");
    fprintf(output, "    return buf;\n");
    fprintf(output, "}\n");
    fprintf(output, "static char *__to_string_str(char *x) { return x; }\n");
    fprintf(output, "static char *__concat_strings(char *s1, char *s2) {\n");
    fprintf(output, "    char *buf = __string_buffers[__string_buf_idx];\n");
    fprintf(output, "    __string_buf_idx = (__string_buf_idx + 1) %% 16;\n");
    fprintf(output, "    strcat(strcpy(buf, s1), s2);\n");
    fprintf(output, "    return buf;\n");
    fprintf(output, "}\n");
    fprintf(output, "static char *__auto_string(void *x) {\n");
    fprintf(output, "    char *buf = __string_buffers[__string_buf_idx];\n");
    fprintf(output, "    __string_buf_idx = (__string_buf_idx + 1) %% 16;\n");
    fprintf(output, "    sprintf(buf, \"%%p\", x);\n");
    fprintf(output, "    return buf;\n");
    fprintf(output, "}\n\n");

    // 第一遍：生成导入的头文件
    ImportNode *imp = importList;
    while (imp) {
        // 跳过已经包含的头文件
        if (strcmp(imp->filename, "stdio") != 0) {
            // 检查是否包含.h后缀
            int hasHext = 0;
            int len = strlen(imp->filename);
            if (len >= 2 && strcmp(imp->filename + len - 2, ".h") == 0) {
                hasHext = 1;
            }
            
            // 检查是否是用户头文件（包含路径或已经是.h文件）
            int isUserHeader = hasHext || strchr(imp->filename, '/') != NULL || strchr(imp->filename, '\\') != NULL;
            
            if (isUserHeader) {
                // 用户头文件，使用引号
                fprintf(output, "#include \"%s\"\n", imp->filename);
            } else {
                // 系统头文件，使用尖括号
                fprintf(output, "#include <%s.h>\n", imp->filename);
            }
        }
        imp = imp->next;
    }
    if (importList) {
        fprintf(output, "\n");
    }

    ASTNode *funcs = NULL;
    ASTNode *funcsTail = NULL;
    ASTNode *stmts = NULL;
    ASTNode *stmtsTail = NULL;
    ASTNode *current = node->next;
    while (current) {
        if (current->type == AST_FUNCTION_DECLARATION) {
            if (!funcs) {
                funcs = current;
                funcsTail = current;
            } else {
                funcsTail->next = current;
                funcsTail = current;
            }
        } else if (current->type == AST_IMPORT) {
            // FFI 函数导入，生成外部函数声明
            if (current->namespace[0] != '\0') {
                // 有函数名，表示是 FFI 函数导入
                fprintf(output, "// FFI function: %s from %s\n", current->namespace, current->filename);
                fprintf(output, "extern int %s();\n\n", current->namespace);
            }
        } else if (current->type == AST_OBJECT_DECLARATION) {
            // 对象声明，直接生成
            generateObjectDeclaration(current);
        } else {
            if (!stmts) {
                stmts = current;
                stmtsTail = current;
            } else {
                stmtsTail->next = current;
                stmtsTail = current;
            }
        }
        current = current->next;
    }

    // 检查是否有main函数
    int hasMain = 0;
    current = funcs;
    while (current) {
        if (current->type == AST_FUNCTION_DECLARATION && strcmp(current->token.value, "main") == 0) {
            hasMain = 1;
            break;
        }
        current = current->next;
    }

    // 保存语句，稍后在main函数中生成
    ASTNode *savedStmts = stmts;

    fprintf(output, "// Function prototypes\n");
    current = funcs;
    while (current) {
        if (current->type == AST_FUNCTION_DECLARATION) {
            char *retType = "int";
            if (current->namespace[0] != '\0') {
                if (strcmp(current->namespace, "float") == 0) retType = "float";
                else if (strcmp(current->namespace, "double") == 0) retType = "double";
                else if (strcmp(current->namespace, "int") == 0) retType = "int";
                else if (strcmp(current->namespace, "string") == 0) retType = "char *";
                else if (strcmp(current->namespace, "char") == 0) retType = "char";
            }
            fprintf(output, "%s %s(", retType, current->token.value);
            ASTNode *param = current->params;
            int first = 1;
            while (param) {
                if (!first) fprintf(output, ", ");
                char *paramType = "int";
                if (param->namespace[0] != '\0') {
                    if (strcmp(param->namespace, "float") == 0) paramType = "float";
                    else if (strcmp(param->namespace, "double") == 0) paramType = "double";
                    else if (strcmp(param->namespace, "int") == 0) paramType = "int";
                    else if (strcmp(param->namespace, "string") == 0) paramType = "char *";
                    else if (strcmp(param->namespace, "char") == 0) paramType = "char";
                }
                fprintf(output, "%s %s", paramType, param->token.value);
                param = param->next;
                first = 0;
            }
            fprintf(output, ");\n");

            // 为函数别名生成函数原型
            if (current->left && current->left->type == AST_IDENTIFIER) {
                // 只有当别名与函数名不同时才生成函数指针原型
                if (strcmp(current->left->token.value, current->token.value) != 0) {
                    fprintf(output, "%s (*%s)(", retType, current->left->token.value);
                    param = current->params;
                    first = 1;
                    while (param) {
                        if (!first) fprintf(output, ", ");
                        char *paramType = "int";
                        if (param->namespace[0] != '\0') {
                            if (strcmp(param->namespace, "float") == 0) paramType = "float";
                            else if (strcmp(param->namespace, "double") == 0) paramType = "double";
                            else if (strcmp(param->namespace, "int") == 0) paramType = "int";
                        }
                        fprintf(output, "%s %s", paramType, param->token.value);
                        param = param->next;
                        first = 0;
                    }
                    fprintf(output, ");\n");
                }
            }
        }
        current = current->next;
    }
    fprintf(output, "\n");

    // 如果没有main函数，生成一个默认的main函数
    if (!hasMain) {
        fprintf(output, "int main() {\n");
        indentLevel++;

        // 生成所有语句
        current = savedStmts;
        while (current) {
            // 跳过所有函数声明，因为它们会在后面单独生成
            if (current->type == AST_FUNCTION_DECLARATION) {
                current = current->next;
                continue;
            }
            generateCode(current);
            current = current->next;
        }

        indent();
        fprintf(output, "return 0;\n");
        indentLevel--;
        fprintf(output, "}\n\n");
    }

    fprintf(output, "// Function definitions\n");
    current = funcs;
    while (current) {
        if (current->type == AST_FUNCTION_DECLARATION) {
            generateCode(current);
        }
        current = current->next;
    }
}

void generateStatement(ASTNode *node) {
    // 生成 next 链表
    ASTNode *current = node->next;
    while (current) {
        indent();
        // 检查当前节点是否是 STATEMENT 节点
        if (current->type == AST_STATEMENT) {
            // 如果是 STATEMENT 节点，直接处理其内容
            if (current->left) {
                generateCode(current->left);
            }
        } else {
            // 如果不是 STATEMENT 节点，直接处理
            generateCode(current);
        }
        fprintf(output, ";\n");
        current = current->next;
    }
}

void generateExpression(ASTNode *node) {
    generateCode(node);
}

void generateBinaryOp(ASTNode *node) {
    if (node->token.type == TOKEN_STRING_CONCAT || node->token.type == TOKEN_AMPERSAND) {
        fprintf(output, "__concat_strings(");
        generateCode(node->left);
        fprintf(output, ", ");
        generateCode(node->right);
        fprintf(output, ")");
        return;
    }

    fprintf(output, "(");
    generateCode(node->left);

    switch (node->token.type) {
        case TOKEN_PLUS: fprintf(output, " + "); break;
        case TOKEN_MINUS: fprintf(output, " - "); break;
        case TOKEN_MUL: fprintf(output, " * "); break;
        case TOKEN_DIV: fprintf(output, " / "); break;
        case TOKEN_EQ: fprintf(output, " == "); break;
        case TOKEN_NE: fprintf(output, " != "); break;
        case TOKEN_LT: fprintf(output, " < "); break;
        case TOKEN_LE: fprintf(output, " <= "); break;
        case TOKEN_GT: fprintf(output, " > "); break;
        case TOKEN_GE: fprintf(output, " >= "); break;
        default: break;
    }

    generateCode(node->right);
    fprintf(output, ")");
}

void generateUnaryOp(ASTNode *node) {
    switch (node->token.type) {
        case TOKEN_PLUS: fprintf(output, "+"); break;
        case TOKEN_MINUS: fprintf(output, "-"); break;
        case TOKEN_NOT: fprintf(output, "!"); break;
        default: break;
    }
    generateCode(node->left);
}

void generateLiteral(ASTNode *node) {
    switch (node->token.type) {
        case TOKEN_NUMBER: fprintf(output, "%s", node->token.value); break;
        case TOKEN_FLOAT_NUMBER: fprintf(output, "%s", node->token.value); break;
        case TOKEN_STRING:
            {
                char *str = node->token.value;
                // 检查是否包含插值表达式
                if (strstr(str, "{{") != NULL) {
                    // 有插值，生成复杂的字符串构建表达式
                    fprintf(output, "({char __str_buf[256] = \"\"; ");
                    char *p = str;
                    while (*p) {
                        if (*p == '{' && *(p+1) == '{') {
                            p += 2;
                            char *expr_start = p;
                            while (*p && !(*p == '}' && *(p+1) == '}')) {
                                p++;
                            }
                            if (*p) {
                                char expr[100];
                                strncpy(expr, expr_start, p - expr_start);
                                expr[p - expr_start] = '\0';
                                p += 2;
                                // 生成插值表达式的代码
                                if (strstr(expr, "string(") == expr) {
                                    // string()函数调用，提取参数
                                    char expr_arg[100];
                                    sscanf(expr, "string(%[^)])", expr_arg);
                                    // 始终使用__string()进行转换
                                    fprintf(output, "strcat(__str_buf, __string(%s)); ", expr_arg);
                                } else {
                                    fprintf(output, "strcat(__str_buf, %s); ", expr);
                                }
                            }
                        } else {
                            char *seg_start = p;
                            while (*p && !(*p == '{' && *(p+1) == '{')) {
                                p++;
                            }
                            if (p > seg_start) {
                                char seg[256];
                                strncpy(seg, seg_start, p - seg_start);
                                seg[p - seg_start] = '\0';
                                fprintf(output, "strcat(__str_buf, \"%s\"); ", seg);
                            }
                        }
                    }
                    fprintf(output, "__str_buf; })");
                } else {
                    // 没有插值，直接输出
                    fprintf(output, "\"%s\"", str);
                }
                break;
            }
        case TOKEN_CHAR: fprintf(output, "'%c'", node->token.value[0]); break;
        case TOKEN_DIR: 
            {
                fprintf(output, "({");
                fprintf(output, "char __dir_buf[256] = \"\"; ");
                // 处理dir类型路径中的{{}}插值表达式
                char *path = node->token.value;
                char *p = path;
                while (*p) {
                    if (*p == '{' && *(p+1) == '{') {
                        // 找到插值表达式的开始
                        p += 2; // 跳过 {{ 字符
                        char *expr_start = p;
                        // 找到插值表达式的结束
                        while (*p && !(*p == '}' && *(p+1) == '}')) {
                            p++;
                        }
                        if (*p) {
                            // 提取插值表达式
                            char expr[100];
                            strncpy(expr, expr_start, p - expr_start);
                            expr[p - expr_start] = '\0';
                            p += 2; // 跳过 }} 字符
                            // 生成插值表达式的代码
                            if (strstr(expr, "string(") == expr) {
                                char expr_arg[100];
                                sscanf(expr, "string(%[^)])", expr_arg);
                                int is_number = 1;
                                char *check = expr_arg;
                                if (*check == '-') check++;
                                while (*check) {
                                    if (!isdigit(*check)) {
                                        is_number = 0;
                                        break;
                                    }
                                    check++;
                                }
                                if (is_number) {
                                    fprintf(output, "strcat(__dir_buf, __string(%s)); ", expr_arg);
                                } else {
                                    // 变量已经是字符串类型，直接使用
                                    fprintf(output, "strcat(__dir_buf, %s); ", expr_arg);
                                }
                            } else {
                                // 普通变量
                                fprintf(output, "strcat(__dir_buf, %s); ", expr);
                            }
                        }
                    } else {
                        // 普通字符
                        int start = p - path;
                        while (*p && !(*p == '{' && *(p+1) == '{')) {
                            p++;
                        }
                        int end = p - path;
                        char segment[100];
                        strncpy(segment, path + start, end - start);
                        segment[end - start] = '\0';
                        fprintf(output, "strcat(__dir_buf, \"%s\"); ", segment);
                    }
                }
                // 添加返回语句
                fprintf(output, "__dir_buf; })");
                break;
            }
            break;
        case TOKEN_ENDL: fprintf(output, "\"\\n\""); break;
        default: break;
    }
}

void generateIdentifier(ASTNode *node) {
    fprintf(output, "%s", node->token.value);
}

void generateFunctionDeclaration(ASTNode *node) {
    char *retType = "int";
    if (node->namespace[0] != '\0') {
        if (strcmp(node->namespace, "float") == 0) retType = "float";
        else if (strcmp(node->namespace, "double") == 0) retType = "double";
        else if (strcmp(node->namespace, "int") == 0) retType = "int";
        else if (strcmp(node->namespace, "string") == 0) retType = "char *";
        else if (strcmp(node->namespace, "char") == 0) retType = "char";
    }

    fprintf(output, "%s %s(", retType, node->token.value);

    ASTNode *param = node->params;
    int first = 1;
    while (param) {
        if (!first) fprintf(output, ", ");
        char *paramType = "int";
        if (param->namespace[0] != '\0') {
            if (strcmp(param->namespace, "float") == 0) paramType = "float";
            else if (strcmp(param->namespace, "double") == 0) paramType = "double";
            else if (strcmp(param->namespace, "int") == 0) paramType = "int";
            else if (strcmp(param->namespace, "string") == 0) paramType = "char *";
            else if (strcmp(param->namespace, "char") == 0) paramType = "char";
        }
        fprintf(output, "%s %s", paramType, param->token.value);
        param = param->next;
        first = 0;
    }
    fprintf(output, ") {\n");
    indentLevel++;

    generateCode(node->body);

    indentLevel--;
    fprintf(output, "}\n\n");

    // 处理函数别名
    if (node->left && node->left->type == AST_IDENTIFIER) {
        // 只有当别名与函数名不同时才生成函数指针
        if (strcmp(node->left->token.value, node->token.value) != 0) {
            // 为别名生成函数指针
            fprintf(output, "// Function alias\n");
            fprintf(output, "%s (*%s)(", retType, node->left->token.value);
            param = node->params;
            first = 1;
            while (param) {
                if (!first) fprintf(output, ", ");
                char *paramType = "int";
                if (param->namespace[0] != '\0') {
                    if (strcmp(param->namespace, "float") == 0) paramType = "float";
                    else if (strcmp(param->namespace, "double") == 0) paramType = "double";
                    else if (strcmp(param->namespace, "int") == 0) paramType = "int";
                }
                fprintf(output, "%s %s", paramType, param->token.value);
                param = param->next;
                first = 0;
            }
            fprintf(output, ") = %s;\n\n", node->token.value);
        }
    }
}

void generateFunctionCall(ASTNode *node) {
    // 检查是否是内置类型转换函数
    if (strcmp(node->token.value, "string") == 0) {
        // string() 转换
        // 检查参数类型，选择正确的函数
        if (node->params) {
            if (node->params->namespace[0] != '\0' && 
                (strcmp(node->params->namespace, "float") == 0 || 
                 strcmp(node->params->namespace, "double") == 0)) {
                fprintf(output, "__string_float(");
            } else {
                fprintf(output, "__string_int(");
            }
            generateCode(node->params);
        } else {
            fprintf(output, "__string_int(");
        }
        fprintf(output, ")");
    } else if (strcmp(node->token.value, "int") == 0) {
        // int() 转换
        fprintf(output, "(int)");
        if (node->params) {
            generateCode(node->params);
        }
    } else if (strcmp(node->token.value, "char") == 0) {
        // char() 转换
        fprintf(output, "(char)");
        if (node->params) {
            generateCode(node->params);
        }
    } else if (strcmp(node->token.value, "dir") == 0) {
        // dir() 转换
        fprintf(output, "(char*)");
        if (node->params) {
            generateCode(node->params);
        }
    } else {
        // 普通函数调用
        // 作为表达式的一部分时不添加缩进和分号
        fprintf(output, "%s(", node->token.value);

        ASTNode *arg = node->params;
        int first = 1;
        while (arg) {
            if (!first) fprintf(output, ", ");
            generateCode(arg);
            arg = arg->next;
            first = 0;
        }
        fprintf(output, ")");
    }
}

void generateReturn(ASTNode *node) {
    indent();
    fprintf(output, "return ");
    generateCode(node->left);
    fprintf(output, ";\n");
}

void generateForLoop(ASTNode *node) {
    indent();
    fprintf(output, "for (");

    if (node->init) {
        if (node->init->left) {
            fprintf(output, "int %s = ", node->init->left->token.value);
            if (node->init->right) {
                generateCode(node->init->right);
            } else {
                fprintf(output, "0");
            }
        } else {
            fprintf(output, "int i = 0");
        }
    } else {
        fprintf(output, ";");
    }
    fprintf(output, "; ");

    if (node->condition) {
        generateCode(node->condition);
    } else {
        fprintf(output, "1");
    }
    fprintf(output, "; ");

    if (node->update) {
        // update 是 AST_STATEMENT，其 left 是 AST_ASSIGNMENT
        if (node->update->left && node->update->left->left) {
            fprintf(output, "%s++", node->update->left->left->token.value);
        } else {
            fprintf(output, "i++");
        }
    } else {
        fprintf(output, ";");
    }
    fprintf(output, ") {\n");
    indentLevel++;

    if (node->body) {
        generateCode(node->body);
    }

    indentLevel--;
    indent();
    fprintf(output, "}\n");
}

void generateWhileLoop(ASTNode *node) {
    indent();
    fprintf(output, "while (");
    generateCode(node->condition);
    fprintf(output, ") {\n");
    indentLevel++;

    generateCode(node->body);

    indentLevel--;
    indent();
    fprintf(output, "}\n");
}

void generateIfStatement(ASTNode *node) {
    indent();
    fprintf(output, "if (");
    generateCode(node->condition);
    fprintf(output, ") {\n");
    indentLevel++;

    generateCode(node->body);

    indentLevel--;
    indent();
    fprintf(output, "}");

    if (node->elseIfChain) {
        fprintf(output, " else");
        generateCode(node->elseIfChain);
    } else if (node->elseBody) {
        fprintf(output, " else {\n");
        indentLevel++;
        generateCode(node->elseBody);
        indentLevel--;
        indent();
        fprintf(output, "}");
    }
    fprintf(output, "\n");
}

void generateBreakStatement(ASTNode *node) {
    indent();
    fprintf(output, "break;\n");
}

void generateSwitchStatement(ASTNode *node) {
    indent();
    fprintf(output, "switch (");
    generateCode(node->condition);
    fprintf(output, ") {\n");
    indentLevel++;

    if (node->body) {
        ASTNode *caseNode = node->body;
        while (caseNode) {
            if (caseNode->type == AST_CASE) {
                if (strcmp(caseNode->namespace, "default") == 0) {
                    indent();
                    fprintf(output, "default:\n");
                } else {
                    indent();
                    fprintf(output, "case ");
                    generateCode(caseNode->left);
                    fprintf(output, ":\n");
                }
                indentLevel++;
                generateCode(caseNode->body);
                indentLevel--;
            }
            caseNode = caseNode->next;
        }
    }

    indentLevel--;
    indent();
    fprintf(output, "}\n");
}

void generateOutput(ASTNode *node) {
    indent();
    // 检查namespace是否是文件路径（dir类型或标识符）
    if (node->namespaceExpr && node->namespaceExpr->type == AST_LITERAL &&
        node->namespaceExpr->token.type == TOKEN_DIR) {
        // 文件输出 - 字面量路径
        fprintf(output, "({FILE * __fp = fopen(");
        generateCode(node->namespaceExpr);
        fprintf(output, ", \"w\"); if (__fp) { fprintf(__fp, ");
        generateCode(node->left);
        fprintf(output, "); fclose(__fp); } })");
    } else if (node->namespaceExpr && node->namespaceExpr->type == AST_IDENTIFIER &&
               strcmp(node->namespaceExpr->namespace, "std") != 0) {
        // 文件输出 - 变量路径（非std标识符）
        fprintf(output, "({FILE * __fp = fopen(%s, \"w\"); if (__fp) { fprintf(__fp, ", node->namespaceExpr->token.value);
        generateCode(node->left);
        fprintf(output, "); fclose(__fp); } })");
    } else {
        // 标准输出
        fprintf(output, "printf(");
        generateCode(node->left);
        fprintf(output, ")");
    }
    fprintf(output, ";\n");
}

void generateInput(ASTNode *node) {
    indent();
    // 检查namespace是否是文件路径（dir类型或标识符）
    if (node->namespaceExpr && node->namespaceExpr->type == AST_LITERAL &&
        node->namespaceExpr->token.type == TOKEN_DIR) {
        // 文件输入 - 字面量路径
        fprintf(output, "({FILE * __fp = fopen(");
        generateCode(node->namespaceExpr);
        fprintf(output, ", \"r\"); if (__fp) { ");
        fprintf(output, "char __buf[1000]; ");
        fprintf(output, "fgets(__buf, 1000, __fp); ");
        fprintf(output, "__buf[strcspn(__buf, \"\\n\")] = 0; "); // 移除换行符
        fprintf(output, "strcpy(%s, __buf); ", node->right->token.value);
        fprintf(output, "fclose(__fp); } })");
        fprintf(output, ";\n");
    } else if (node->namespaceExpr && node->namespaceExpr->type == AST_IDENTIFIER &&
               strcmp(node->namespaceExpr->namespace, "std") != 0) {
        // 文件输入 - 变量路径（非std标识符）
        fprintf(output, "({FILE * __fp = fopen(%s, \"r\"); if (__fp) { ", node->namespaceExpr->token.value);
        fprintf(output, "char __buf[1000]; ");
        fprintf(output, "fgets(__buf, 1000, __fp); ");
        fprintf(output, "__buf[strcspn(__buf, \"\\n\")] = 0; "); // 移除换行符
        fprintf(output, "strcpy(%s, __buf); ", node->right->token.value);
        fprintf(output, "fclose(__fp); } })");
        fprintf(output, ";\n");
    } else {
        // 标准输入
        fprintf(output, "char %s[100];\n", node->right->token.value);
        if (node->left) {
            indent();
            fprintf(output, "printf(%s);\n", node->left->token.value);
        }
        indent();
        fprintf(output, "scanf(\"%%s\", %s);\n", node->right->token.value);
    }
}

void generateImport(ASTNode *node) {
    // 导入语句在生成程序时已经处理，这里不需要生成额外代码
    // FFI 函数导入会在 generateProgram 中生成函数声明
}

void generateVariableDeclaration(ASTNode *node) {
    indent();

    if (!node->left) {
        fprintf(output, "int var = 0;\n");
        return;
    }

    ASTNode *varNode = node->left;

    // 检查是否是赋值语句
    if (node->token.type == TOKEN_ASSIGN) {
        // 处理赋值语句
        generateCode(varNode);
        fprintf(output, " = ");
        if (node->right) {
            generateCode(node->right);
        } else {
            fprintf(output, "0");
        }
        fprintf(output, ";\n");
        return;
    }

    // 检查是否是结构体成员赋值或指针解引用赋值
    if (varNode->type == AST_MEMBER_ACCESS || varNode->type == AST_DEREFERENCE) {
        // 处理结构体成员赋值或指针解引用赋值
        generateCode(varNode);
        fprintf(output, " = ");
        if (node->right) {
            generateCode(node->right);
        } else {
            fprintf(output, "0");
        }
        fprintf(output, ";\n");
        return;
    }

    if (node->token.type == TOKEN_ID || node->token.type == TOKEN_STRING_TYPE ||
        node->token.type == TOKEN_CHAR_TYPE || node->token.type == TOKEN_DIR) {
        // 检查是否是类型转换函数
        if (strcmp(node->token.value, "string") == 0 || strcmp(node->token.value, "dir") == 0) {
            // 字符串类型转换
            fprintf(output, "char %s[256];\n", varNode->token.value);
            indent();
            fprintf(output, "strcpy(%s, ", varNode->token.value);
            if (node->right) {
                generateCode(node->right);
            } else {
                fprintf(output, "\"\"");
            }
            fprintf(output, ");\n");
            return;
        } else if (strcmp(node->token.value, "char") == 0) {
            // 字符类型转换
            fprintf(output, "char %s = ", varNode->token.value);
            if (node->right) {
                generateCode(node->right);
            } else {
                fprintf(output, "'\\0'");
            }
            fprintf(output, ";\n");
            return;
        } else if (strcmp(node->token.value, "int") == 0) {
            // 整数类型转换
            fprintf(output, "int %s = ", varNode->token.value);
            if (node->right) {
                generateCode(node->right);
            } else {
                fprintf(output, "0");
            }
            fprintf(output, ";\n");
            return;
        } else if (node->right && node->right->type == AST_FUNCTION_CALL) {
            // 其他函数调用
            fprintf(output, "int %s = ", varNode->token.value);
            generateCode(node->right);
            fprintf(output, ";\n");
            return;
        } else {
            // 检查是否是结构体类型
            if (strcmp(node->token.value, "Point") == 0 || strcmp(node->token.value, "Rectangle") == 0) {
                // 结构体类型变量声明
                fprintf(output, "struct %s %s", node->token.value, varNode->token.value);
                if (node->right) {
                    fprintf(output, " = ");
                    generateCode(node->right);
                }
                fprintf(output, ";\n");
                return;
            } else {
                // 其他类型变量声明
                fprintf(output, "int %s = ", varNode->token.value);
            }
        }
    }

    if (node->token.type != TOKEN_ID) {
        switch (node->token.type) {
            case TOKEN_INT: fprintf(output, "int %s = ", varNode->token.value); break;
            case TOKEN_STRING: fprintf(output, "char %s[100];\n", varNode->token.value); indent(); fprintf(output, "strcpy(%s, ", varNode->token.value); break;
            case TOKEN_CHAR: fprintf(output, "char %s = ", varNode->token.value); break;
            case TOKEN_FLOAT: fprintf(output, "float %s = ", varNode->token.value); break;
            case TOKEN_DOUBLE: fprintf(output, "double %s = ", varNode->token.value); break;
            case TOKEN_DIR: fprintf(output, "char %s[100];\n", varNode->token.value); indent(); fprintf(output, "strcpy(%s, ", varNode->token.value); break;
            default: fprintf(output, "int %s = ", varNode->token.value); break;
        }

        if (node->right) {
            generateCode(node->right);
        } else {
            fprintf(output, "0");
        }
        if (node->token.type == TOKEN_STRING || node->token.type == TOKEN_DIR) {
            fprintf(output, ");\n");
        } else {
            fprintf(output, ";\n");
        }
    }
}

void indent() {
    switch (indentLevel) {
        case 1:
            fprintf(output, "    ");
            break;
        case 2:
            fprintf(output, "        ");
            break;
        case 3:
            fprintf(output, "            ");
            break;
        default:
            break;
    }
}

void generateStructDeclaration(ASTNode *node) {
    fprintf(output, "struct %s {\n", node->namespace);
    indentLevel++;
    
    ASTNode *member = node->body;
    while (member) {
        indent();
        // 检查是否是结构体类型
        if (strcmp(member->token.value, "Point") == 0 || strcmp(member->token.value, "Rectangle") == 0) {
            // 结构体类型成员
            fprintf(output, "struct %s %s;\n", member->token.value, member->left->token.value);
        } else {
            // 其他类型成员
            fprintf(output, "%s %s;\n", member->token.value, member->left->token.value);
        }
        member = member->next;
    }
    
    indentLevel--;
    fprintf(output, "};\n\n");
}

void generateEnumDeclaration(ASTNode *node) {
    fprintf(output, "enum %s {\n", node->namespace);
    indentLevel++;
    
    ASTNode *member = node->body;
    int value = 0;
    while (member) {
        indent();
        fprintf(output, "%s", member->token.value);
        if (member->right) {
            fprintf(output, " = ");
            generateCode(member->right);
        } else if (value > 0) {
            fprintf(output, " = %d", value);
        }
        if (member->next) {
            fprintf(output, ",");
        }
        fprintf(output, "\n");
        value++;
        member = member->next;
    }
    
    indentLevel--;
    fprintf(output, "};\n\n");
}

void generateMemberAccess(ASTNode *node) {
    // 生成结构体成员访问的代码
    // 左侧是结构体对象
    generateCode(node->left);
    
    // 生成点运算符
    fprintf(output, ".");
    
    // 右侧是成员名
    generateCode(node->right);
}

void generatePointerDeclaration(ASTNode *node) {
    // 生成指针声明
    const char *pointedType = node->namespace;
    fprintf(output, "%s* %s", pointedType, node->left->token.value);
    if (node->right) {
        fprintf(output, " = ");
        generateCode(node->right);
    }
    fprintf(output, ";\n");
}

void generateMemoryDeclaration(ASTNode *node) {
    // 生成内存块声明
    fprintf(output, "void* %s", node->left->token.value);
    if (node->right) {
        fprintf(output, " = ");
        generateCode(node->right);
    }
    fprintf(output, ";\n");
}

void generateDereference(ASTNode *node) {
    // 生成指针解引用
    fprintf(output, "*(");
    generateCode(node->left);
    fprintf(output, ")");
}

void generateAddressOf(ASTNode *node) {
    // 生成取地址操作
    fprintf(output, "&");
    generateCode(node->left);
}

void generatePointerOp(ASTNode *node) {
    // 生成指针运算
    generateCode(node->left);
    fprintf(output, " %s ", node->token.value);
    generateCode(node->right);
}

void generateAllocCall(ASTNode *node) {
    // 生成 alloc 函数调用
    fprintf(output, "malloc(");
    ASTNode *arg = node->params;
    if (arg) {
        if (arg->type == AST_IDENTIFIER) {
            // 处理类型大小的情况，如 alloc(int)
            const char *typeName = arg->token.value;
            if (strcmp(typeName, "int") == 0) {
                fprintf(output, "sizeof(int)");
            } else if (strcmp(typeName, "float") == 0) {
                fprintf(output, "sizeof(float)");
            } else if (strcmp(typeName, "double") == 0) {
                fprintf(output, "sizeof(double)");
            } else if (strcmp(typeName, "char") == 0) {
                fprintf(output, "sizeof(char)");
            } else if (strcmp(typeName, "string") == 0) {
                fprintf(output, "sizeof(char*)");
            } else {
                // 结构体或其他类型
                fprintf(output, "sizeof(%s)", typeName);
            }
        } else {
            // 处理数值大小的情况，如 alloc(1024)
            generateCode(arg);
        }
    }
    fprintf(output, ")");
}

void generateFreeCall(ASTNode *node) {
    // 生成 free 函数调用
    fprintf(output, "free(");
    ASTNode *arg = node->params;
    if (arg) {
        generateCode(arg);
    }
    fprintf(output, ");");
}

void generateRunCall(ASTNode *node) {
    indent();

    ASTNode *arg = node->params;
    if (arg && arg->type == AST_LITERAL && arg->token.type == TOKEN_STRING) {
        FILE *tempE = fopen("_temp_run.e", "w");
        if (tempE) {
            char *code = arg->token.value;
            char *p = code;
            while (*p) {
                if (*p == '\\') {
                    p++;
                    if (*p == 'n') {
                        fputc('\n', tempE);
                    } else if (*p == 't') {
                        fputc('\t', tempE);
                    } else if (*p == '"') {
                        fputc('"', tempE);
                    } else if (*p == '\\') {
                        fputc('\\', tempE);
                    } else {
                        fputc('\\', tempE);
                        fputc(*p, tempE);
                    }
                } else {
                    fputc(*p, tempE);
                }
                p++;
            }
            fclose(tempE);

            int compile_result = system("elang.exe _temp_run.e --compile 2>NUL");

            if (compile_result == 0) {
                compile_result = system("gcc -o _temp_run.exe _temp_run.c 2>NUL");

                if (compile_result == 0) {
                    system("_temp_run.exe > _temp_run.out 2>&1");

                    FILE *outFile = fopen("_temp_run.out", "r");
                    if (outFile) {
                        char run_buf[1024];
                        if (fgets(run_buf, sizeof(run_buf), outFile)) {
                            int len = strlen(run_buf);
                            if (len > 0 && run_buf[len-1] == '\n') {
                                run_buf[len-1] = '\0';
                            }
                            fprintf(output, "\"%s\"", run_buf);
                        } else {
                            fprintf(output, "\"\"");
                        }
                        fclose(outFile);
                    } else {
                        fprintf(output, "\"\"");
                    }

                    remove("_temp_run.e");
                    remove("_temp_run.c");
                    remove("_temp_run.exe");
                    remove("_temp_run.out");
                } else {
                    remove("_temp_run.e");
                    remove("_temp_run.c");
                    fprintf(output, "\"\"");
                }
            } else {
                remove("_temp_run.e");
                fprintf(output, "\"\"");
            }
        } else {
            fprintf(output, "\"\"");
        }
    } else {
        fprintf(output, "0");
    }
}

void generateAsm(ASTNode *node) {
    if (!node || strlen(node->filename) == 0) {
        fprintf(output, "\"\"");
        return;
    }

    int hasStringResult = 0;
    char *asmCode = node->filename;
    char *p = asmCode;

    while (*p) {
        if (strstr(p, "call string")) {
            hasStringResult = 1;
            break;
        }
        if (strncmp(p, "mov xl,", 7) == 0) {
            char *afterMov = p + 7;
            while (*afterMov == ' ') afterMov++;
            if (*afterMov == '"') {
                hasStringResult = 1;
                break;
            }
        }
        p++;
    }

    fprintf(output, "({\n");
    fprintf(output, "    static char _tmp_asm_buf[256];\n");
    fprintf(output, "    long long _tmp_asm_xl = 0, _tmp_asm_xh = 0;\n");
    fprintf(output, "    char _tmp_asm_al = 0, _tmp_asm_ah = 0;\n");
    fprintf(output, "    char _tmp_asm_bl = 0, _tmp_asm_bh = 0;\n");

    p = asmCode;

    while (*p) {
        char *lineEnd = p;
        while (*lineEnd && *lineEnd != '\n') lineEnd++;

        char line[256];
        int len = lineEnd - p;
        if (len > 255) len = 255;
        strncpy(line, p, len);
        line[len] = '\0';

        char *linePtr = line;
        while (*linePtr) {
            while (*linePtr == ' ') linePtr++;

            if (strncmp(linePtr, "call string", 11) == 0) {
                linePtr += 11;
                while (*linePtr == ' ') linePtr++;
                if (*linePtr == ',') {
                    linePtr++;
                    while (*linePtr == ' ') linePtr++;
                }
                if (linePtr[0] == '_' && linePtr[1] != '\0') {
                    char regName[10];
                    sscanf(linePtr + 1, "%2s", regName);
                    fprintf(output, "    _tmp_asm_xl = (long long)__string((int)_tmp_asm_%c%c);\n",
                            regName[0], regName[1]);
                }
            } else if (strncmp(linePtr, "call ", 5) == 0) {
                linePtr += 5;
                while (*linePtr == ' ') linePtr++;
                char *comma = strchr(linePtr, ',');
                if (comma) {
                    *comma = '\0';
                }
                char *regPart = comma ? comma + 1 : NULL;
                while (regPart && *regPart == ' ') regPart++;

                if (regPart && regPart[0] == '_' && regPart[1] != '\0') {
                    char regName[10] = "";
                    sscanf(regPart + 1, "%2s", regName);
                    fprintf(output, "    _tmp_asm_%s = (long long)__%s((int)_tmp_asm_%s);\n",
                            regName, linePtr, regName);
                }
            } else if (strncmp(linePtr, "mov xl,", 7) == 0) {
                char *value = linePtr + 7;
                while (*value == ' ') value++;

                if (*value == '"') {
                    value++;
                    char *endQuote = strchr(value, '"');
                    if (endQuote) {
                        int len = endQuote - value;
                        if (len > 48) len = 48;
                        char strVal[50];
                        strncpy(strVal, value, len);
                        strVal[len] = '\0';
                        fprintf(output, "    _tmp_asm_xl = (long long)\"%s\";\n", strVal);
                        linePtr = endQuote + 1;
                    } else {
                        fprintf(output, "    _tmp_asm_xl = 0;\n");
                        linePtr = value + strlen(value);
                    }
                } else {
                    char valStr[50];
                    int i = 0;
                    while (*value && *value != ';' && *value != '\n' && *value != ' ') {
                        if (i < 49) valStr[i++] = *value;
                        value++;
                    }
                    valStr[i] = '\0';
                    fprintf(output, "    _tmp_asm_xl = atoi(\"%s\");\n", valStr);
                    linePtr = value;
                }
            } else if (strncmp(linePtr, "mov al,", 7) == 0) {
                char *value = linePtr + 7;
                while (*value == ' ') value++;
                char valStr[50];
                int i = 0;
                while (*value && *value != ';' && *value != '\n' && *value != ' ') {
                    if (i < 49) valStr[i++] = *value;
                    value++;
                }
                valStr[i] = '\0';
                fprintf(output, "    _tmp_asm_al = atoi(\"%s\");\n", valStr);
                linePtr = value;
            } else if (strncmp(linePtr, "mov ah,", 7) == 0) {
                char *value = linePtr + 7;
                while (*value == ' ') value++;
                char valStr[50];
                int i = 0;
                while (*value && *value != ';' && *value != '\n' && *value != ' ') {
                    if (i < 49) valStr[i++] = *value;
                    value++;
                }
                valStr[i] = '\0';
                fprintf(output, "    _tmp_asm_ah = atoi(\"%s\");\n", valStr);
                linePtr = value;
            } else if (strncmp(linePtr, "mov bl,", 7) == 0) {
                char *value = linePtr + 7;
                while (*value == ' ') value++;
                char valStr[50];
                int i = 0;
                while (*value && *value != ';' && *value != '\n' && *value != ' ') {
                    if (i < 49) valStr[i++] = *value;
                    value++;
                }
                valStr[i] = '\0';
                fprintf(output, "    _tmp_asm_bl = atoi(\"%s\");\n", valStr);
                linePtr = value;
            } else if (strncmp(linePtr, "mov bh,", 7) == 0) {
                char *value = linePtr + 7;
                while (*value == ' ') value++;
                char valStr[50];
                int i = 0;
                while (*value && *value != ';' && *value != '\n' && *value != ' ') {
                    if (i < 49) valStr[i++] = *value;
                    value++;
                }
                valStr[i] = '\0';
                fprintf(output, "    _tmp_asm_bh = atoi(\"%s\");\n", valStr);
                linePtr = value;
            } else if (strncmp(linePtr, "mov xh,", 7) == 0) {
                char *value = linePtr + 7;
                while (*value == ' ') value++;
                char valStr[50];
                int i = 0;
                while (*value && *value != ';' && *value != '\n' && *value != ' ') {
                    if (i < 49) valStr[i++] = *value;
                    value++;
                }
                valStr[i] = '\0';
                fprintf(output, "    _tmp_asm_xh = atoi(\"%s\");\n", valStr);
                linePtr = value;
            } else {
                linePtr++;
                continue;
            }

            while (*linePtr && *linePtr != ';') linePtr++;
            if (*linePtr == ';') {
                linePtr++;
                while (*linePtr && (*linePtr == ' ' || *linePtr == '\t')) linePtr++;
            }
        }

        p = lineEnd;
        if (*p == '\n') p++;
    }

    if (hasStringResult) {
        fprintf(output, "    _tmp_asm_buf[0] = '\\0'; strcat(_tmp_asm_buf, (char*)_tmp_asm_xl);\n");
    } else if (node->left) {
        int index = atoi(node->left->token.value);
        fprintf(output, "    sprintf(_tmp_asm_buf, \"%%lld\", _tmp_asm_%s);\n", index == 0 ? "xl" : "xh");
    } else {
        fprintf(output, "    sprintf(_tmp_asm_buf, \"%%lld:%%lld\", _tmp_asm_xl, _tmp_asm_xh);\n");
    }
    fprintf(output, "    _tmp_asm_buf;\n})");
}

void generateSecondaryMacroInvocation(ASTNode *node) {
    indent();
    
    fprintf(output, "// Secondary macro: %s\n", node->namespace);
    
    if (strcmp(node->namespace, "println") == 0) {
        if (node->params) {
            fprintf(output, "printf(");
            generateCode(node->params);
            fprintf(output, ");\n");
            indent();
            fprintf(output, "printf(\"\\\\n\");\n");
        }
    } else if (strcmp(node->namespace, "print") == 0) {
        if (node->params) {
            fprintf(output, "printf(");
            generateCode(node->params);
            fprintf(output, ");\n");
        }
    } else if (strcmp(node->namespace, "assert") == 0) {
        if (node->params) {
            fprintf(output, "if (!(");
            generateCode(node->params);
            fprintf(output, ")) {\n");
            indentLevel++;
            indent();
            fprintf(output, "printf(\"Assertion failed at %%s:%%d\\\\n\", __FILE__, __LINE__);\n");
            indent();
            fprintf(output, "exit(1);\n");
            indentLevel--;
            indent();
            fprintf(output, "}\n");
        }
    } else if (strcmp(node->namespace, "typeof") == 0) {
        if (node->params) {
            fprintf(output, "(_Generic((");
            generateCode(node->params);
            fprintf(output, "), int: \"int\", long: \"long\", char: \"char\", float: \"float\", double: \"double\", default: \"unknown\"))");
        }
    } else if (strcmp(node->namespace, "sizeof") == 0) {
        if (node->params) {
            fprintf(output, "sizeof(");
            generateCode(node->params);
            fprintf(output, ")");
        }
    } else if (strcmp(node->namespace, "memset") == 0) {
        fprintf(output, "memset(");
        ASTNode *arg = node->params;
        while (arg) {
            generateCode(arg);
            arg = arg->next;
            if (arg) fprintf(output, ", ");
        }
        fprintf(output, ");\n");
    } else if (strcmp(node->namespace, "memcpy") == 0) {
        fprintf(output, "memcpy(");
        ASTNode *arg = node->params;
        while (arg) {
            generateCode(arg);
            arg = arg->next;
            if (arg) fprintf(output, ", ");
        }
        fprintf(output, ");\n");
    } else if (strcmp(node->namespace, "strlen") == 0) {
        fprintf(output, "strlen(");
        if (node->params) {
            generateCode(node->params);
        }
        fprintf(output, ")");
    } else if (strcmp(node->namespace, "strcpy") == 0) {
        fprintf(output, "strcpy(");
        ASTNode *arg = node->params;
        while (arg) {
            generateCode(arg);
            arg = arg->next;
            if (arg) fprintf(output, ", ");
        }
        fprintf(output, ");\n");
    } else {
        fprintf(output, "_secondary_macro_%s(", node->namespace);
        ASTNode *arg = node->params;
        while (arg) {
            generateCode(arg);
            arg = arg->next;
            if (arg) fprintf(output, ", ");
        }
        fprintf(output, ");\n");
    }
}

void generateObjectDeclaration(ASTNode *node) {
    if (!node) return;
    
    fprintf(output, "struct %s {\n", node->namespace);
    indentLevel++;
    
    ASTNode *member = node->body;
    while (member) {
        if (member->type == AST_OBJECT_MEMBER) {
            indent();
            const char *typeName = member->namespace;
            const char *memberName = member->token.value;
            
            if (strcmp(typeName, "string") == 0) {
                fprintf(output, "char %s[256];\n", memberName);
            } else {
                fprintf(output, "%s %s;\n", typeName, memberName);
            }
        } else if (member->type == AST_OBJECT_DECLARATION) {
            // 嵌套对象
            indent();
            fprintf(output, "struct %s %s;\n", member->namespace, member->namespace);
        } else if (member->type == AST_FUNCTION_DECLARATION) {
            // 对象方法 - 生成函数指针
            indent();
            char returnType[20] = "void";
            if (member->left && member->left->type == AST_IDENTIFIER) {
                strcpy(returnType, member->left->token.value);
            }
            fprintf(output, "%s (*%s)();\n", returnType, member->token.value);
        }
        member = member->next;
    }
    
    indentLevel--;
    fprintf(output, "};\n");
    
    // 创建全局实例
    fprintf(output, "struct %s %s = {", node->namespace, node->namespace);
    
    member = node->body;
    int first = 1;
    while (member) {
        if (member->type == AST_OBJECT_MEMBER) {
            if (!first) fprintf(output, ", ");
            if (member->right) {
                if (strcmp(member->namespace, "string") == 0) {
                    generateCode(member->right);
                } else {
                    generateCode(member->right);
                }
            }
            first = 0;
        }
        member = member->next;
    }
    
    fprintf(output, "};\n\n");
}

void generateObjectMember(ASTNode *node) {
    if (!node) return;
    
    indent();
    const char *typeName = node->namespace;
    const char *memberName = node->token.value;
    
    if (strcmp(typeName, "string") == 0) {
        fprintf(output, "char %s[256]", memberName);
    } else {
        fprintf(output, "%s %s", typeName, memberName);
    }
    
    if (node->right) {
        fprintf(output, " = ");
        generateCode(node->right);
    }
    fprintf(output, ";\n");
}

void generateObjectAccess(ASTNode *node) {
    if (!node || !node->left) return;
    
    generateCode(node->left);
    
    if (node->right->type == AST_IDENTIFIER) {
        fprintf(output, ".%s", node->right->token.value);
    } else {
        fprintf(output, "->%s", node->right->token.value);
    }
}