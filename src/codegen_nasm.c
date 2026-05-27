#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lexer.h"
#include "parser.h"
#include "codegen_nasm.h"

#define MAX_VARS 100
#define MAX_LABELS 50

extern SymbolTable *currentSymbolTable;

int nasmLabelCounter = 0;
int macroUniqueIdCounter = 0;
int stringLabelCounter = 0;
char stringDefinitions[8192] = "";
int isCollectingStrings = 0;

#define MAX_STRING_MAP 100
char stringMapStrings[MAX_STRING_MAP][1024];
char stringMapLabels[MAX_STRING_MAP][32];
int stringMapCount = 0;

#define MAX_OBJECTS 50
typedef struct {
    char name[64];
    ASTNode *node;
} ObjectDef;
ObjectDef objectTable[MAX_OBJECTS];
int objectCount = 0;

TargetPlatform targetPlatform = TARGET_WIN;
char *currentSwitchEndLabel = NULL;

typedef struct CodeMacro {
    char name[100];
    ASTNode *params;
    ASTNode *body;
    struct CodeMacro *next;
} CodeMacro;

CodeMacro *codeMacroTable = NULL;

int getMacroUniqueId() {
    return macroUniqueIdCounter++;
}

CodeMacro *findCodeMacro(const char *name) {
    CodeMacro *current = codeMacroTable;
    while (current) {
        if (strcmp(current->name, name) == 0) {
            return current;
        }
        current = current->next;
    }
    return NULL;
}

void addCodeMacro(const char *name, ASTNode *params, ASTNode *body) {
    CodeMacro *macro = (CodeMacro *)malloc(sizeof(CodeMacro));
    strcpy(macro->name, name);
    macro->params = params;
    macro->body = body;
    macro->next = codeMacroTable;
    codeMacroTable = macro;
}

ASTNode *cloneASTNode(ASTNode *node) {
    if (!node) return NULL;
    
    ASTNode *clone = (ASTNode *)malloc(sizeof(ASTNode));
    memcpy(clone, node, sizeof(ASTNode));
    clone->next = NULL;
    clone->left = NULL;
    clone->right = NULL;
    clone->body = NULL;
    clone->params = NULL;
    clone->elseBody = NULL;
    clone->elseIfChain = NULL;
    
    if (node->left) clone->left = cloneASTNode(node->left);
    if (node->right) clone->right = cloneASTNode(node->right);
    if (node->body) clone->body = cloneASTNode(node->body);
    if (node->params) clone->params = cloneASTNode(node->params);
    if (node->next) clone->next = cloneASTNode(node->next);
    if (node->elseBody) clone->elseBody = cloneASTNode(node->elseBody);
    if (node->elseIfChain) clone->elseIfChain = cloneASTNode(node->elseIfChain);
    
    return clone;
}

ASTNode *substituteInNode(ASTNode *node, const char *paramName, ASTNode *arg) {
    if (!node) return NULL;
    
    if (node->type == AST_IDENTIFIER && strcmp(node->token.value, paramName) == 0) {
        return cloneASTNode(arg);
    }
    
    ASTNode *result = (ASTNode *)malloc(sizeof(ASTNode));
    memcpy(result, node, sizeof(ASTNode));
    result->left = NULL;
    result->right = NULL;
    result->next = NULL;
    result->params = NULL;
    result->body = NULL;
    result->elseBody = NULL;
    result->elseIfChain = NULL;
    
    if (node->left) result->left = substituteInNode(node->left, paramName, arg);
    if (node->right) result->right = substituteInNode(node->right, paramName, arg);
    if (node->body) result->body = substituteInNode(node->body, paramName, arg);
    if (node->params) result->params = substituteInNode(node->params, paramName, arg);
    if (node->next) result->next = substituteInNode(node->next, paramName, arg);
    
    return result;
}

ASTNode *substituteParams(ASTNode *body, ASTNode *params, ASTNode *args) {
    if (!body) return NULL;
    
    ASTNode *result = cloneASTNode(body);
    
    ASTNode *param = params;
    ASTNode *arg = args;
    
    while (param && arg) {
        if (param->type == AST_IDENTIFIER) {
            ASTNode *substituted = substituteInNode(result, param->token.value, arg);
            free(result);
            result = substituted;
        }
        param = param->next;
        arg = arg->next;
    }
    
    return result;
}

void renameVariablesInNode(ASTNode *node, int uniqueId) {
    if (!node) return;
    
    if (node->type == AST_VARIABLE_DECLARATION && node->left && node->left->type == AST_IDENTIFIER) {
        char newName[128];
        sprintf(newName, "_m%d_%s", uniqueId, node->left->token.value);
        strcpy(node->left->token.value, newName);
    }
    
    if (node->type == AST_IDENTIFIER) {
        char newName[128];
        sprintf(newName, "_m%d_%s", uniqueId, node->token.value);
        strcpy(node->token.value, newName);
    }
    
    if (node->left) renameVariablesInNode(node->left, uniqueId);
    if (node->right) renameVariablesInNode(node->right, uniqueId);
    if (node->body) renameVariablesInNode(node->body, uniqueId);
    if (node->params) renameVariablesInNode(node->params, uniqueId);
    if (node->next) renameVariablesInNode(node->next, uniqueId);
}

void expandCodeMacro(CodeMacro *macro, ASTNode *args) {
    if (!macro || !macro->body) return;
    
    int uniqueId = getMacroUniqueId();
    
    ASTNode *expandedBody = macro->body;
    
    if (macro->params && args) {
        expandedBody = substituteParams(macro->body, macro->params, args);
    } else {
        expandedBody = cloneASTNode(macro->body);
    }
    
    renameVariablesInNode(expandedBody, uniqueId);
    
    generateNASMCode(expandedBody);
}

typedef struct {
    char name[64];
    int offset;
    int used;
} VarInfo;

VarInfo varTable[MAX_VARS];
int varTableSize = 0;

char elseLabelBuffer[MAX_LABELS][16];
char endLabelBuffer[MAX_LABELS][16];
int labelDepth = 0;

char labelBuffer[32];

char *nasmNewLabel() {
    sprintf(labelBuffer, ".L%d", nasmLabelCounter++);
    return labelBuffer;
}

int getVariableOffset(const char *name) {
    for (int i = 0; i < varTableSize; i++) {
        if (strcmp(varTable[i].name, name) == 0) {
            return varTable[i].offset;
        }
    }
    return -1;
}

int addVariable(const char *name) {
    if (varTableSize >= MAX_VARS) return -1;
    strcpy(varTable[varTableSize].name, name);
    varTable[varTableSize].offset = 8 + varTableSize * 8;
    varTable[varTableSize].used = 1;
    return varTable[varTableSize++].offset;
}

void clearVariableTable() {
    varTableSize = 0;
    memset(varTable, 0, sizeof(varTable));
}

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
void generateNASMSwitchStatement(ASTNode *node);
void generateARMSwitchStatement(ASTNode *node);
void generateARMCode(ASTNode *node);
void generateARMExpression(ASTNode *node);
void generateARMLoadValue(ASTNode *node);
void generateNASMOutput(ASTNode *node);
void generateNASMOutputRecursive(ASTNode *node);
void generateNASMVariableDeclaration(ASTNode *node);
void generateNASMAssignment(ASTNode *node);
void generateNASMAsm(ASTNode *node);
int isObjectName(const char *name);

void generateNASMCode(ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM: generateNASMProgram(node); return;
        case AST_STATEMENT: generateNASMStatement(node); break;
        case AST_EXPRESSION: generateNASMExpression(node); break;
        case AST_VARIABLE_DECLARATION: generateNASMVariableDeclaration(node); break;
        case AST_FUNCTION_DECLARATION: generateNASMFunctionDeclaration(node); break;
        case AST_FUNCTION_CALL: 
            {
                CodeMacro *macro = findCodeMacro(node->token.value);
                if (macro) {
                    expandCodeMacro(macro, node->params);
                    break;
                }
                generateNASMFunctionCall(node); 
                break;
            }
        case AST_RETURN: generateNASMReturn(node); return;
        case AST_FOR: generateNASMForLoop(node); break;
        case AST_WHILE: generateNASMWhileLoop(node); break;
        case AST_IF: generateNASMIfStatement(node); break;
        case AST_SWITCH: generateNASMSwitchStatement(node); break;
        case AST_BREAK: 
            if (currentSwitchEndLabel) {
                fprintf(output, "    jmp %s\n", currentSwitchEndLabel);
            }
            break;
        case AST_CASE:
            // AST_CASE is handled inside generateNASMSwitchStatement
            break;
        case AST_OUTPUT: generateNASMOutput(node); break;
        case AST_INPUT: generateNASMInput(node); break;
        case AST_STRING_CONCAT:
        case AST_BINARY_OP: generateNASMBinaryOp(node); break;
        case AST_UNARY_OP: generateNASMUnaryOp(node); break;
        case AST_LITERAL: generateNASMLiteral(node); break;
        case AST_IDENTIFIER: generateNASMIdentifier(node); break;
        case AST_STRUCT_DECLARATION: generateNASMStructDeclaration(node); break;
        case AST_ENUM_DECLARATION: generateNASMEnumDeclaration(node); break;
        case AST_MEMBER_ACCESS: generateNASMMemberAccess(node); break;
        case AST_DEREFERENCE: generateNASMDereference(node); break;
        case AST_ADDRESS_OF: generateNASMAddressOf(node); break;
        case AST_ALLOC_CALL: generateNASMAllocCall(node); break;
        case AST_FREE_CALL: generateNASMFreeCall(node); break;
        case AST_OBJECT_DECLARATION: generateNASMObjectDeclaration(node); break;
        case AST_OBJECT_MEMBER: generateNASMObjectMember(node); break;
        case AST_OBJECT_ACCESS: generateNASMObjectAccess(node); break;
        case AST_ASM: generateNASMAsm(node); break;
        case AST_MACRO_DEFINITION:
            {
                if (strcmp(node->filename, "undef") == 0) {
                    break;
                }
                addCodeMacro(node->namespace, node->params, node->body);
                break;
            }
        default: break;
    }

    if (node->next) {
        generateNASMCode(node->next);
    }
}

void generateARMProgram(ASTNode *node);

void collectObjects(ASTNode *node) {
    if (!node) return;
    
    if (node->type == AST_OBJECT_DECLARATION && objectCount < MAX_OBJECTS) {
        strcpy(objectTable[objectCount].name, node->namespace);
        objectTable[objectCount].node = node;
        objectCount++;
    }
    
    collectObjects(node->next);
}

void collectStrings(ASTNode *node) {
    if (!node) return;
    
    if (node->type == AST_FUNCTION_DECLARATION) {
        collectStrings(node->body);
    } else if (node->type == AST_STATEMENT) {
        collectStrings(node->body);
        collectStrings(node->next);
    } else if (node->type == AST_OUTPUT) {
        collectStrings(node->left);
    } else if (node->type == AST_STRING_CONCAT) {
        collectStrings(node->left);
        collectStrings(node->right);
    } else if (node->type == AST_OBJECT_DECLARATION) {
        // 收集对象成员的初始值里的字符串
        collectStrings(node->body);
    } else if (node->type == AST_OBJECT_MEMBER) {
        // 收集对象成员的初始值
        if (node->right) {
            collectStrings(node->right);
        }
    } else if (node->type == AST_LITERAL && node->token.type == TOKEN_STRING) {
        int found = 0;
        for (int i = 0; i < stringMapCount; i++) {
            if (strcmp(stringMapStrings[i], node->token.value) == 0) {
                found = 1;
                break;
            }
        }
        
        if (!found && stringMapCount < MAX_STRING_MAP) {
            char labelName[32];
            sprintf(labelName, "__str_%d", stringMapCount);
            
            strcpy(stringMapStrings[stringMapCount], node->token.value);
            strcpy(stringMapLabels[stringMapCount], labelName);
            stringMapCount++;
            
            char def[512];
            sprintf(def, "    %s: db `", labelName);
            strcat(stringDefinitions, def);
            
            for (int i = 0; node->token.value[i] != '\0'; i++) {
                if (node->token.value[i] == '\\') {
                    i++;
                    if (node->token.value[i] == 'n') {
                        strcat(stringDefinitions, "\\n");
                    } else if (node->token.value[i] == 't') {
                        strcat(stringDefinitions, "\\t");
                    } else if (node->token.value[i] == '\\') {
                        strcat(stringDefinitions, "\\");
                    } else if (node->token.value[i] == '`') {
                        strcat(stringDefinitions, "\\`");
                    } else {
                        strcat(stringDefinitions, "\\");
                        char c[2] = {node->token.value[i], '\0'};
                        strcat(stringDefinitions, c);
                    }
                } else if (node->token.value[i] == '`') {
                    strcat(stringDefinitions, "\\`");
                } else {
                    char c[2] = {node->token.value[i], '\0'};
                    strcat(stringDefinitions, c);
                }
            }
            strcat(stringDefinitions, "`, 0\n");
        }
    }
    
    collectStrings(node->next);
}

void generateNASMProgram(ASTNode *node) {
    clearVariableTable();
    nasmLabelCounter = 0;
    stringLabelCounter = 0;
    stringDefinitions[0] = '\0';
    stringMapCount = 0;
    objectCount = 0;

    if (targetPlatform == TARGET_ARM_LINUX || targetPlatform == TARGET_ARM_ANDROID) {
        generateARMProgram(node);
        return;
    }

    collectObjects(node);
    collectStrings(node);

    fprintf(output, "; NASM Assembly Code Generated by E Compiler\n");
    if (targetPlatform == TARGET_WIN) {
        fprintf(output, "; Architecture: x86-64 Windows\n\n");
    } else {
        fprintf(output, "; Architecture: x86-64 Linux\n\n");
    }

    fprintf(output, "section .data\n");
    fprintf(output, "    __newline: db 10\n");
    fprintf(output, "    __str_buf: times 256 db 0\n");
    fprintf(output, "%s", stringDefinitions);
    
    // 生成对象定义
    for (int i = 0; i < objectCount; i++) {
        ASTNode *objNode = objectTable[i].node;
        fprintf(output, "\n    ; Object: %s\n", objectTable[i].name);
        ASTNode *member = objNode->body;
        while (member) {
            if (member->type == AST_OBJECT_MEMBER) {
                const char *typeName = member->namespace;
                const char *memberName = member->token.value;
                if (strcmp(typeName, "string") == 0) {
                    fprintf(output, "    __obj_%s_%s: times 256 db 0\n", objectTable[i].name, memberName);
                } else if (strcmp(typeName, "int") == 0 || strcmp(typeName, "float") == 0 || strcmp(typeName, "double") == 0) {
                    fprintf(output, "    __obj_%s_%s: dq 0\n", objectTable[i].name, memberName);
                } else if (strcmp(typeName, "char") == 0) {
                    fprintf(output, "    __obj_%s_%s: db 0\n", objectTable[i].name, memberName);
                } else {
                    fprintf(output, "    __obj_%s_%s: dq 0\n", objectTable[i].name, memberName);
                }
            }
            member = member->next;
        }
    }
    fprintf(output, "\n");

    fprintf(output, "section .text\n");
    
    if (targetPlatform == TARGET_WIN) {
        fprintf(output, "    global _main\n");
        fprintf(output, "    extern GetStdHandle\n");
        fprintf(output, "    extern WriteConsoleA\n");
        fprintf(output, "    extern ExitProcess\n\n");
        
        fprintf(output, "; Helper function: string(int) -> char*\n");
        fprintf(output, "_string:\n");
        fprintf(output, "    push rbp\n");
        fprintf(output, "    mov rbp, rsp\n");
        fprintf(output, "    mov rsi, __str_buf\n");
        fprintf(output, "    mov rax, [rbp + 16]\n");
        fprintf(output, "    mov rcx, 10\n");
        fprintf(output, "    mov rbx, 0\n");
        fprintf(output, "_string_loop:\n");
        fprintf(output, "    xor rdx, rdx\n");
        fprintf(output, "    div rcx\n");
        fprintf(output, "    add dl, '0'\n");
        fprintf(output, "    mov [rsi + rbx], dl\n");
        fprintf(output, "    inc rbx\n");
        fprintf(output, "    test rax, rax\n");
        fprintf(output, "    jnz _string_loop\n");
        fprintf(output, "    mov byte [rsi + rbx], 0\n");
        fprintf(output, "    ; Reverse string\n");
        fprintf(output, "    mov rcx, 0\n");
        fprintf(output, "    dec rbx\n");
        fprintf(output, "_string_rev:\n");
        fprintf(output, "    cmp rcx, rbx\n");
        fprintf(output, "    jge _string_done\n");
        fprintf(output, "    mov dl, [rsi + rcx]\n");
        fprintf(output, "    mov dh, [rsi + rbx]\n");
        fprintf(output, "    mov [rsi + rcx], dh\n");
        fprintf(output, "    mov [rsi + rbx], dl\n");
        fprintf(output, "    inc rcx\n");
        fprintf(output, "    dec rbx\n");
        fprintf(output, "    jmp _string_rev\n");
        fprintf(output, "_string_done:\n");
        fprintf(output, "    mov rax, __str_buf\n");
        fprintf(output, "    pop rbp\n");
        fprintf(output, "    ret\n\n");

        fprintf(output, "; Helper function: print string\n");
        fprintf(output, "_print_str:\n");
        fprintf(output, "    push rbp\n");
        fprintf(output, "    mov rbp, rsp\n");
        fprintf(output, "    mov rsi, [rbp + 16]\n");
        fprintf(output, "    ; Get length\n");
        fprintf(output, "    mov rcx, 0\n");
        fprintf(output, "_print_len:\n");
        fprintf(output, "    cmp byte [rsi + rcx], 0\n");
        fprintf(output, "    je _print_len_done\n");
        fprintf(output, "    inc rcx\n");
        fprintf(output, "    jmp _print_len\n");
        fprintf(output, "_print_len_done:\n");
        fprintf(output, "    push rcx\n");
        fprintf(output, "    ; Get stdout handle\n");
        fprintf(output, "    mov rcx, -11\n");
        fprintf(output, "    call GetStdHandle\n");
        fprintf(output, "    mov rcx, rax\n");
        fprintf(output, "    mov rdx, [rbp + 16]\n");
        fprintf(output, "    pop r8\n");
        fprintf(output, "    sub rsp, 8\n");
        fprintf(output, "    mov r9, rsp\n");
        fprintf(output, "    call WriteConsoleA\n");
        fprintf(output, "    add rsp, 8\n");
        fprintf(output, "    pop rbp\n");
        fprintf(output, "    ret\n\n");

        fprintf(output, "; Helper function: print newline\n");
        fprintf(output, "_print_nl:\n");
        fprintf(output, "    push rbp\n");
        fprintf(output, "    mov rbp, rsp\n");
        fprintf(output, "    mov rcx, -11\n");
        fprintf(output, "    call GetStdHandle\n");
        fprintf(output, "    mov rcx, rax\n");
        fprintf(output, "    mov rdx, __newline\n");
        fprintf(output, "    mov r8, 1\n");
        fprintf(output, "    sub rsp, 8\n");
        fprintf(output, "    mov r9, rsp\n");
        fprintf(output, "    call WriteConsoleA\n");
        fprintf(output, "    add rsp, 8\n");
        fprintf(output, "    pop rbp\n");
        fprintf(output, "    ret\n\n");
    } else {
        fprintf(output, "    global main\n\n");
        
        fprintf(output, "; Helper function: string(int) -> char*\n");
        fprintf(output, "string_func:\n");
        fprintf(output, "    push rbp\n");
        fprintf(output, "    mov rbp, rsp\n");
        fprintf(output, "    mov rsi, __str_buf\n");
        fprintf(output, "    mov rax, [rbp + 16]\n");
        fprintf(output, "    mov rcx, 10\n");
        fprintf(output, "    mov rbx, 0\n");
        fprintf(output, "_string_loop:\n");
        fprintf(output, "    xor rdx, rdx\n");
        fprintf(output, "    div rcx\n");
        fprintf(output, "    add dl, '0'\n");
        fprintf(output, "    mov [rsi + rbx], dl\n");
        fprintf(output, "    inc rbx\n");
        fprintf(output, "    test rax, rax\n");
        fprintf(output, "    jnz _string_loop\n");
        fprintf(output, "    mov byte [rsi + rbx], 0\n");
        fprintf(output, "    ; Reverse string\n");
        fprintf(output, "    mov rcx, 0\n");
        fprintf(output, "    dec rbx\n");
        fprintf(output, "_string_rev:\n");
        fprintf(output, "    cmp rcx, rbx\n");
        fprintf(output, "    jge _string_done\n");
        fprintf(output, "    mov dl, [rsi + rcx]\n");
        fprintf(output, "    mov dh, [rsi + rbx]\n");
        fprintf(output, "    mov [rsi + rcx], dh\n");
        fprintf(output, "    mov [rsi + rbx], dl\n");
        fprintf(output, "    inc rcx\n");
        fprintf(output, "    dec rbx\n");
        fprintf(output, "    jmp _string_rev\n");
        fprintf(output, "_string_done:\n");
        fprintf(output, "    mov rax, __str_buf\n");
        fprintf(output, "    pop rbp\n");
        fprintf(output, "    ret\n\n");

        fprintf(output, "; Helper function: print string (Linux syscall)\n");
        fprintf(output, "_print_str:\n");
        fprintf(output, "    push rbp\n");
        fprintf(output, "    mov rbp, rsp\n");
        fprintf(output, "    mov rsi, [rbp + 16]\n");
        fprintf(output, "    ; Get length\n");
        fprintf(output, "    mov rcx, 0\n");
        fprintf(output, "_print_len:\n");
        fprintf(output, "    cmp byte [rsi + rcx], 0\n");
        fprintf(output, "    je _print_len_done\n");
        fprintf(output, "    inc rcx\n");
        fprintf(output, "    jmp _print_len\n");
        fprintf(output, "_print_len_done:\n");
        fprintf(output, "    ; sys_write(stdout, rsi, rcx)\n");
        fprintf(output, "    mov rax, 1\n");
        fprintf(output, "    mov rdi, 1\n");
        fprintf(output, "    mov rdx, rcx\n");
        fprintf(output, "    syscall\n");
        fprintf(output, "    pop rbp\n");
        fprintf(output, "    ret\n\n");

        fprintf(output, "; Helper function: print newline (Linux syscall)\n");
        fprintf(output, "_print_nl:\n");
        fprintf(output, "    push rbp\n");
        fprintf(output, "    mov rax, 1\n");
        fprintf(output, "    mov rdi, 1\n");
        fprintf(output, "    mov rsi, __newline\n");
        fprintf(output, "    mov rdx, 1\n");
        fprintf(output, "    syscall\n");
        fprintf(output, "    pop rbp\n");
        fprintf(output, "    ret\n\n");
    }

    if (node->body) {
        generateNASMCode(node->body);
    }
    if (node->next) {
        generateNASMCode(node->next);
    }
}

void generateNASMStatement(ASTNode *node) {
    if (!node) return;

    if (node->left) {
        if (node->left->type == AST_ASSIGNMENT) {
            generateNASMAssignment(node->left);
        } else {
            generateNASMCode(node->left);
        }
    }
}

void generateNASMExpression(ASTNode *node) {
    if (!node) return;

    if (node->type == AST_BINARY_OP) {
        generateNASMBinaryOp(node);
    } else if (node->type == AST_LITERAL) {
        generateNASMLiteral(node);
    } else if (node->type == AST_IDENTIFIER) {
        generateNASMIdentifier(node);
    } else if (node->type == AST_FUNCTION_CALL) {
        generateNASMFunctionCall(node);
    } else if (node->type == AST_STRING_CONCAT) {
        generateNASMBinaryOp(node);
    } else if (node->type == AST_ASM) {
        generateNASMAsm(node);
    }
}

void generateNASMBinaryOp(ASTNode *node) {
    if (!node) return;

    if (node->left) {
        generateNASMExpression(node->left);
    }
    if (node->right) {
        generateNASMExpression(node->right);
    }

    if (strcmp(node->token.value, "+") == 0) {
        fprintf(output, "    pop rbx\n");
        fprintf(output, "    pop rax\n");
        fprintf(output, "    add rax, rbx\n");
        fprintf(output, "    push rax\n");
    } else if (strcmp(node->token.value, "-") == 0) {
        fprintf(output, "    pop rbx\n");
        fprintf(output, "    pop rax\n");
        fprintf(output, "    sub rax, rbx\n");
        fprintf(output, "    push rax\n");
    } else if (strcmp(node->token.value, "*") == 0) {
        fprintf(output, "    pop rax\n");
        fprintf(output, "    pop rbx\n");
        fprintf(output, "    imul rbx\n");
        fprintf(output, "    push rax\n");
    } else if (strcmp(node->token.value, "/") == 0) {
        fprintf(output, "    pop rbx\n");
        fprintf(output, "    pop rax\n");
        fprintf(output, "    cqo\n");
        fprintf(output, "    idiv rbx\n");
        fprintf(output, "    push rax\n");
    } else if (strcmp(node->token.value, "==") == 0) {
        fprintf(output, "    pop rbx\n");
        fprintf(output, "    pop rax\n");
        fprintf(output, "    cmp rax, rbx\n");
        fprintf(output, "    sete al\n");
        fprintf(output, "    movzx rax, al\n");
        fprintf(output, "    push rax\n");
    } else if (strcmp(node->token.value, "!=") == 0) {
        fprintf(output, "    pop rbx\n");
        fprintf(output, "    pop rax\n");
        fprintf(output, "    cmp rax, rbx\n");
        fprintf(output, "    setne al\n");
        fprintf(output, "    movzx rax, al\n");
        fprintf(output, "    push rax\n");
    } else if (strcmp(node->token.value, "<") == 0) {
        fprintf(output, "    pop rbx\n");
        fprintf(output, "    pop rax\n");
        fprintf(output, "    cmp rax, rbx\n");
        fprintf(output, "    setl al\n");
        fprintf(output, "    movzx rax, al\n");
        fprintf(output, "    push rax\n");
    } else if (strcmp(node->token.value, ">") == 0) {
        fprintf(output, "    pop rbx\n");
        fprintf(output, "    pop rax\n");
        fprintf(output, "    cmp rax, rbx\n");
        fprintf(output, "    setg al\n");
        fprintf(output, "    movzx rax, al\n");
        fprintf(output, "    push rax\n");
    } else if (strcmp(node->token.value, "<=") == 0) {
        fprintf(output, "    pop rbx\n");
        fprintf(output, "    pop rax\n");
        fprintf(output, "    cmp rax, rbx\n");
        fprintf(output, "    setle al\n");
        fprintf(output, "    movzx rax, al\n");
        fprintf(output, "    push rax\n");
    } else if (strcmp(node->token.value, ">=") == 0) {
        fprintf(output, "    pop rbx\n");
        fprintf(output, "    pop rax\n");
        fprintf(output, "    cmp rax, rbx\n");
        fprintf(output, "    setge al\n");
        fprintf(output, "    movzx rax, al\n");
        fprintf(output, "    push rax\n");
    }
}

void generateNASMLiteral(ASTNode *node) {
    if (!node) return;

    if (node->token.type == TOKEN_NUMBER) {
        fprintf(output, "    mov rax, %s\n", node->token.value);
        fprintf(output, "    push rax\n");
    } else if (node->token.type == TOKEN_STRING) {
        for (int i = 0; i < stringMapCount; i++) {
            if (strcmp(stringMapStrings[i], node->token.value) == 0) {
                fprintf(output, "    mov rax, %s\n", stringMapLabels[i]);
                fprintf(output, "    push rax\n");
                break;
            }
        }
    }
}

void generateNASMIdentifier(ASTNode *node) {
    if (!node) return;

    int offset = getVariableOffset(node->token.value);
    if (offset < 0) {
        offset = 8;
    }
    fprintf(output, "    mov rax, [rbp - %d]\n", offset);
    fprintf(output, "    push rax\n");
}

void generateNASMFunctionDeclaration(ASTNode *node) {
    if (!node) return;

    clearVariableTable();
    nasmLabelCounter = 0;
    labelDepth = 0;

    char *funcName = node->token.value;
    char winFuncName[64];
    if (targetPlatform == TARGET_WIN) {
        sprintf(winFuncName, "_%s", funcName);
    } else {
        sprintf(winFuncName, "%s", funcName);
    }
    fprintf(output, "%s:\n", winFuncName);
    fprintf(output, "    push rbp\n");
    fprintf(output, "    mov rbp, rsp\n");
    fprintf(output, "    sub rsp, %d\n", MAX_VARS * 8 + 32);

    // 如果是main函数，初始化对象
    if (strcmp(funcName, "main") == 0) {
        fprintf(output, "    ; Initialize objects\n");
        for (int i = 0; i < objectCount; i++) {
            ASTNode *objNode = objectTable[i].node;
            ASTNode *member = objNode->body;
            while (member) {
                if (member->type == AST_OBJECT_MEMBER && member->right) {
                    const char *typeName = member->namespace;
                    const char *memberName = member->token.value;
                    if (strcmp(typeName, "string") == 0) {
                        // 复制字符串
                        if (member->right->type == AST_LITERAL && member->right->token.type == TOKEN_STRING) {
                            // 找到这个字符串的标签
                            const char *strLabel = NULL;
                            for (int j = 0; j < stringMapCount; j++) {
                                if (strcmp(stringMapStrings[j], member->right->token.value) == 0) {
                                    strLabel = stringMapLabels[j];
                                    break;
                                }
                            }
                            if (strLabel) {
                                fprintf(output, "    ; Copy string to %s.%s\n", objectTable[i].name, memberName);
                                fprintf(output, "    mov rsi, %s\n", strLabel);
                                fprintf(output, "    mov rdi, __obj_%s_%s\n", objectTable[i].name, memberName);
                                fprintf(output, "    mov rcx, 0\n");
                                fprintf(output, "__obj_strcpy_%d:\n", nasmLabelCounter);
                                fprintf(output, "    mov al, [rsi + rcx]\n");
                                fprintf(output, "    mov [rdi + rcx], al\n");
                                fprintf(output, "    cmp al, 0\n");
                                fprintf(output, "    je __obj_strcpy_done_%d\n", nasmLabelCounter);
                                fprintf(output, "    inc rcx\n");
                                fprintf(output, "    cmp rcx, 255\n");
                                fprintf(output, "    jl __obj_strcpy_%d\n", nasmLabelCounter);
                                fprintf(output, "__obj_strcpy_done_%d:\n", nasmLabelCounter);
                                nasmLabelCounter++;
                            }
                        }
                    } else if (strcmp(typeName, "int") == 0) {
                        if (member->right->type == AST_LITERAL) {
                            fprintf(output, "    mov qword [__obj_%s_%s], %s\n", objectTable[i].name, memberName, member->right->token.value);
                        }
                    }
                }
                member = member->next;
            }
        }
    }

    if (node->body) {
        generateNASMCode(node->body);
    }

    fprintf(output, "    mov rsp, rbp\n");
    fprintf(output, "    pop rbp\n");
    if (strcmp(funcName, "main") == 0 && targetPlatform == TARGET_WIN) {
        fprintf(output, "    mov rcx, 0\n");
        fprintf(output, "    call ExitProcess\n");
    } else {
        fprintf(output, "    ret\n");
    }
}

void generateNASMFunctionCall(ASTNode *node) {
    if (!node) return;

    if (node->params) {
        ASTNode *param = node->params;
        while (param) {
            generateNASMExpression(param);
            param = param->next;
        }
    }

    char funcName[64];
    if (targetPlatform == TARGET_WIN) {
        sprintf(funcName, "_%s", node->token.value);
    } else {
        sprintf(funcName, "%s", node->token.value);
    }
    fprintf(output, "    call %s\n", funcName);
    fprintf(output, "    push rax\n");
}

void generateNASMReturn(ASTNode *node) {
    if (!node) return;

    if (node->left) {
        if (node->left->type == AST_IDENTIFIER) {
            int offset = getVariableOffset(node->left->token.value);
            if (offset < 0) offset = 8;
            fprintf(output, "    mov rax, [rbp - %d]\n", offset);
        } else if (node->left->type == AST_LITERAL) {
            fprintf(output, "    mov rax, %s\n", node->left->token.value);
        } else {
            generateNASMExpression(node->left);
            fprintf(output, "    pop rax\n");
        }
    }

    fprintf(output, "    mov rsp, rbp\n");
    fprintf(output, "    pop rbp\n");
    fprintf(output, "    ret\n");
}

void generateNASMForLoop(ASTNode *node) {
    if (!node) return;

    char startLabel[16];
    char endLabel[16];
    sprintf(startLabel, ".L%d", nasmLabelCounter++);
    sprintf(endLabel, ".L%d", nasmLabelCounter++);

    if (node->init) {
        generateNASMCode(node->init);
    }

    fprintf(output, "%s:\n", startLabel);

    if (node->condition) {
        generateNASMExpression(node->condition);
        fprintf(output, "    pop rax\n");
        fprintf(output, "    cmp rax, 0\n");
        fprintf(output, "    je %s\n", endLabel);
    }

    if (node->body) {
        generateNASMCode(node->body);
    }

    if (node->update) {
        generateNASMCode(node->update);
    }

    fprintf(output, "    jmp %s\n", startLabel);
    fprintf(output, "%s:\n", endLabel);
}

void generateNASMWhileLoop(ASTNode *node) {
    if (!node) return;

    char startLabel[16];
    char endLabel[16];
    sprintf(startLabel, ".L%d", nasmLabelCounter++);
    sprintf(endLabel, ".L%d", nasmLabelCounter++);

    fprintf(output, "%s:\n", startLabel);

    if (node->condition) {
        generateNASMExpression(node->condition);
        fprintf(output, "    pop rax\n");
        fprintf(output, "    cmp rax, 0\n");
        fprintf(output, "    je %s\n", endLabel);
    }

    if (node->body) {
        generateNASMCode(node->body);
    }

    fprintf(output, "    jmp %s\n", startLabel);
    fprintf(output, "%s:\n", endLabel);
}

void generateNASMIfStatement(ASTNode *node) {
    if (!node) return;

    char elseLabel[16];
    char endLabel[16];
    sprintf(elseLabel, ".L%d", nasmLabelCounter++);
    sprintf(endLabel, ".L%d", nasmLabelCounter++);

    if (node->condition) {
        generateNASMExpression(node->condition);
        fprintf(output, "    pop rax\n");
        fprintf(output, "    cmp rax, 0\n");
        fprintf(output, "    je %s\n", elseLabel);
    }

    if (node->body) {
        generateNASMCode(node->body);
    }

    fprintf(output, "    jmp %s\n", endLabel);
    fprintf(output, "%s:\n", elseLabel);

    if (node->elseBody) {
        generateNASMCode(node->elseBody);
    }

    fprintf(output, "%s:\n", endLabel);
}

void generateNASMSwitchStatement(ASTNode *node) {
    if (!node) return;
    
    char endLabel[16];
    sprintf(endLabel, ".L%d", nasmLabelCounter++);
    
    char *oldLabel = currentSwitchEndLabel;
    currentSwitchEndLabel = endLabel;
    
    if (node->condition) {
        generateNASMExpression(node->condition);
        fprintf(output, "    pop rax\n");
        fprintf(output, "    push rax\n");
    }
    
    ASTNode *defaultCase = NULL;
    ASTNode *caseNode = node->body;
    
    while (caseNode) {
        if (caseNode->type == AST_CASE) {
            if (strcmp(caseNode->namespace, "default") == 0) {
                defaultCase = caseNode;
            } else {
                char caseLabel[16];
                sprintf(caseLabel, ".L%d", nasmLabelCounter++);
                
                fprintf(output, "    pop rax\n");
                generateNASMExpression(caseNode->left);
                fprintf(output, "    pop rbx\n");
                fprintf(output, "    cmp rax, rbx\n");
                fprintf(output, "    jne %s\n", caseLabel);
                
                generateNASMCode(caseNode->body);
                fprintf(output, "    jmp %s\n", endLabel);
                
                fprintf(output, "%s:\n", caseLabel);
                fprintf(output, "    push rax\n");
            }
        }
        caseNode = caseNode->next;
    }
    
    if (defaultCase) {
        fprintf(output, "    pop rax\n");
        generateNASMCode(defaultCase->body);
        fprintf(output, "    jmp %s\n", endLabel);
    } else {
        fprintf(output, "    pop rax\n");
    }
    
    fprintf(output, "%s:\n", endLabel);
    
    currentSwitchEndLabel = oldLabel;
}

void generateARMSwitchStatement(ASTNode *node) {
    if (!node) return;
    
    char endLabel[16];
    sprintf(endLabel, ".L%d", nasmLabelCounter++);
    
    char *oldLabel = currentSwitchEndLabel;
    currentSwitchEndLabel = endLabel;
    
    if (node->condition) {
        generateARMExpression(node->condition);
        fprintf(output, "    str x0, [sp, -8]!\n");
    }
    
    ASTNode *caseNode = node->body;
    while (caseNode) {
        if (caseNode->type == AST_CASE) {
            if (strcmp(caseNode->namespace, "default") == 0) {
                generateARMCode(caseNode->body);
            } else {
                char caseLabel[16];
                sprintf(caseLabel, ".L%d", nasmLabelCounter++);
                
                fprintf(output, "    ldr x1, [sp], 8\n");
                generateARMExpression(caseNode->left);
                fprintf(output, "    cmp x0, x1\n");
                fprintf(output, "    b.ne %s\n", caseLabel);
                
                generateARMCode(caseNode->body);
                fprintf(output, "    b %s\n", endLabel);
                
                fprintf(output, "%s:\n", caseLabel);
                fprintf(output, "    str x1, [sp, -8]!\n");
            }
        }
        caseNode = caseNode->next;
    }
    
    fprintf(output, "%s:\n", endLabel);
    
    currentSwitchEndLabel = oldLabel;
}

void generateNASMOutputRecursive(ASTNode *node) {
    if (!node) return;
    
    if (node->type == AST_STRING_CONCAT) {
        generateNASMOutputRecursive(node->left);
        generateNASMOutputRecursive(node->right);
        return;
    } else if (node->type == AST_FUNCTION_CALL && strcmp(node->token.value, "string") == 0) {
        generateNASMCode(node);
        fprintf(output, "    call _print_str\n");
    } else if (node->type == AST_LITERAL && node->token.type == TOKEN_STRING) {
        generateNASMCode(node);
        fprintf(output, "    call _print_str\n");
    } else {
        generateNASMCode(node);
        fprintf(output, "    call _print_str\n");
    }
}

void generateNASMOutput(ASTNode *node) {
    if (!node) return;

    if (node->left) {
        generateNASMOutputRecursive(node->left);
    }
    
    fprintf(output, "    call _print_nl\n");
}

void generateNASMVariableDeclaration(ASTNode *node) {
    if (!node) return;

    if (node->token.type == TOKEN_INT) {
        char *varName = node->left ? node->left->token.value : "unknown";
        int offset = addVariable(varName);

        if (node->right) {
            generateNASMExpression(node->right);
            fprintf(output, "    pop rax\n");
            fprintf(output, "    mov [rbp - %d], rax\n", offset);
        }
    } else if (node->token.type == TOKEN_ID) {
        char *varName = node->token.value;
        int offset = addVariable(varName);

        if (node->right) {
            generateNASMExpression(node->right);
            fprintf(output, "    pop rax\n");
            fprintf(output, "    mov [rbp - %d], rax\n", offset);
        }
    } else if (node->token.type == TOKEN_ASSIGN) {
        generateNASMAssignment(node);
    } else if (node->left && node->left->type == AST_IDENTIFIER) {
        char *varName = node->left->token.value;
        int offset = addVariable(varName);

        if (node->right) {
            generateNASMExpression(node->right);
            fprintf(output, "    pop rax\n");
            fprintf(output, "    mov [rbp - %d], rax\n", offset);
        }
    }
}

void generateNASMInput(ASTNode *node) {
    if (!node) return;

    if (node->namespaceExpr && node->namespaceExpr->type == AST_LITERAL &&
        node->namespaceExpr->token.type == TOKEN_DIR) {
        // 文件输入 - 字面量路径
        if (targetPlatform == TARGET_WIN) {
            // Windows: 使用WinAPI读取文件
            fprintf(output, "    ; File input from: %s\n", node->namespaceExpr->token.value);
            fprintf(output, "    push 0                  ; hTemplateFile = NULL\n");
            fprintf(output, "    push 0                  ; dwFlagsAndAttributes = 0\n");
            fprintf(output, "    push 3                  ; dwCreationDisposition = OPEN_EXISTING\n");
            fprintf(output, "    push 0                  ; lpSecurityAttributes = NULL\n");
            fprintf(output, "    push 0                  ; dwShareMode = 0\n");
            fprintf(output, "    push 080000000h         ; dwDesiredAccess = GENERIC_READ\n");
            fprintf(output, "    push dword [%s_str]     ; lpFileName\n", node->namespaceExpr->token.value);
            fprintf(output, "    call CreateFileA\n");
            fprintf(output, "    test eax, eax\n");
            fprintf(output, "    jl _input_file_error_%d\n", nasmLabelCounter);
            
            // 读取文件内容到缓冲区
            fprintf(output, "    push 0                  ; lpOverlapped = NULL\n");
            fprintf(output, "    push __bytes_read       ; lpNumberOfBytesRead\n");
            fprintf(output, "    push 256                ; nNumberOfBytesToRead\n");
            fprintf(output, "    push __input_buf        ; lpBuffer\n");
            fprintf(output, "    push eax                ; hFile\n");
            fprintf(output, "    call ReadFile\n");
            
            // 关闭文件
            fprintf(output, "    push eax                ; hFile\n");
            fprintf(output, "    call CloseHandle\n");
            
            // 移除换行符
            fprintf(output, "    mov rsi, __input_buf\n");
            fprintf(output, "_input_strip_nl_%d:\n", nasmLabelCounter);
            fprintf(output, "    cmp byte [rsi], 0\n");
            fprintf(output, "    je _input_copy_%d\n", nasmLabelCounter);
            fprintf(output, "    cmp byte [rsi], 10\n");
            fprintf(output, "    jne _input_next_%d\n", nasmLabelCounter);
            fprintf(output, "    mov byte [rsi], 0\n");
            fprintf(output, "_input_next_%d:\n", nasmLabelCounter);
            fprintf(output, "    inc rsi\n");
            fprintf(output, "    jmp _input_strip_nl_%d\n", nasmLabelCounter);
            
            // 复制到目标变量
            fprintf(output, "_input_copy_%d:\n", nasmLabelCounter);
            if (node->right && node->right->type == AST_IDENTIFIER) {
                char *varName = node->right->token.value;
                int offset = getVariableOffset(varName);
                if (offset != -1) {
                    fprintf(output, "    mov rsi, __input_buf\n");
                    fprintf(output, "    mov rdi, rbp\n");
                    fprintf(output, "    sub rdi, %d\n", offset);
                    fprintf(output, "_input_copy_loop_%d:\n", nasmLabelCounter);
                    fprintf(output, "    mov al, byte [rsi]\n");
                    fprintf(output, "    mov byte [rdi], al\n");
                    fprintf(output, "    inc rsi\n");
                    fprintf(output, "    inc rdi\n");
                    fprintf(output, "    test al, al\n");
                    fprintf(output, "    jne _input_copy_loop_%d\n", nasmLabelCounter);
                }
            }
            fprintf(output, "    jmp _input_done_%d\n", nasmLabelCounter);
            fprintf(output, "_input_file_error_%d:\n", nasmLabelCounter);
            fprintf(output, "    ; File open error\n");
            fprintf(output, "_input_done_%d:\n", nasmLabelCounter);
            nasmLabelCounter++;
        } else {
            // Linux: 使用syscall读取文件
            fprintf(output, "    ; File input from: %s\n", node->namespaceExpr->token.value);
            fprintf(output, "    mov rax, 2              ; sys_open\n");
            fprintf(output, "    mov rdi, %s_str         ; filename\n", node->namespaceExpr->token.value);
            fprintf(output, "    mov rsi, 0              ; flags = O_RDONLY\n");
            fprintf(output, "    mov rdx, 0              ; mode\n");
            fprintf(output, "    syscall\n");
            fprintf(output, "    cmp rax, 0\n");
            fprintf(output, "    jl _input_file_error_%d\n", nasmLabelCounter);
            
            // 保存文件描述符
            fprintf(output, "    mov r15, rax\n");
            
            // 读取文件内容
            fprintf(output, "    mov rax, 0              ; sys_read\n");
            fprintf(output, "    mov rdi, r15            ; fd\n");
            fprintf(output, "    mov rsi, __input_buf    ; buf\n");
            fprintf(output, "    mov rdx, 256            ; count\n");
            fprintf(output, "    syscall\n");
            
            // 关闭文件
            fprintf(output, "    mov rax, 3              ; sys_close\n");
            fprintf(output, "    mov rdi, r15\n");
            fprintf(output, "    syscall\n");
            
            // 移除换行符
            fprintf(output, "    mov rsi, __input_buf\n");
            fprintf(output, "_input_strip_nl_%d:\n", nasmLabelCounter);
            fprintf(output, "    cmp byte [rsi], 0\n");
            fprintf(output, "    je _input_copy_%d\n", nasmLabelCounter);
            fprintf(output, "    cmp byte [rsi], 10\n");
            fprintf(output, "    jne _input_next_%d\n", nasmLabelCounter);
            fprintf(output, "    mov byte [rsi], 0\n");
            fprintf(output, "_input_next_%d:\n", nasmLabelCounter);
            fprintf(output, "    inc rsi\n");
            fprintf(output, "    jmp _input_strip_nl_%d\n", nasmLabelCounter);
            
            // 复制到目标变量
            fprintf(output, "_input_copy_%d:\n", nasmLabelCounter);
            if (node->right && node->right->type == AST_IDENTIFIER) {
                char *varName = node->right->token.value;
                int offset = getVariableOffset(varName);
                if (offset != -1) {
                    fprintf(output, "    mov rsi, __input_buf\n");
                    fprintf(output, "    mov rdi, rbp\n");
                    fprintf(output, "    sub rdi, %d\n", offset);
                    fprintf(output, "_input_copy_loop_%d:\n", nasmLabelCounter);
                    fprintf(output, "    mov al, byte [rsi]\n");
                    fprintf(output, "    mov byte [rdi], al\n");
                    fprintf(output, "    inc rsi\n");
                    fprintf(output, "    inc rdi\n");
                    fprintf(output, "    test al, al\n");
                    fprintf(output, "    jne _input_copy_loop_%d\n", nasmLabelCounter);
                }
            }
            fprintf(output, "    jmp _input_done_%d\n", nasmLabelCounter);
            fprintf(output, "_input_file_error_%d:\n", nasmLabelCounter);
            fprintf(output, "    ; File open error\n");
            fprintf(output, "_input_done_%d:\n", nasmLabelCounter);
            nasmLabelCounter++;
        }
    } else if (node->namespaceExpr && node->namespaceExpr->type == AST_IDENTIFIER &&
               strcmp(node->namespaceExpr->namespace, "std") != 0) {
        // 文件输入 - 变量路径（非std标识符）
        if (targetPlatform == TARGET_WIN) {
            fprintf(output, "    ; File input from variable: %s\n", node->namespaceExpr->token.value);
            fprintf(output, "    push 0                  ; hTemplateFile = NULL\n");
            fprintf(output, "    push 0                  ; dwFlagsAndAttributes = 0\n");
            fprintf(output, "    push 3                  ; dwCreationDisposition = OPEN_EXISTING\n");
            fprintf(output, "    push 0                  ; lpSecurityAttributes = NULL\n");
            fprintf(output, "    push 0                  ; dwShareMode = 0\n");
            fprintf(output, "    push 080000000h         ; dwDesiredAccess = GENERIC_READ\n");
            fprintf(output, "    mov rcx, rbp\n");
            fprintf(output, "    sub rcx, %d             ; variable offset\n", getVariableOffset(node->namespaceExpr->token.value));
            fprintf(output, "    push rcx                ; lpFileName\n");
            fprintf(output, "    call CreateFileA\n");
            // ... 后续类似上面的实现
            fprintf(output, "    ; ReadFile and close...\n");
        } else {
            fprintf(output, "    ; File input from variable: %s\n", node->namespaceExpr->token.value);
            fprintf(output, "    mov rax, 2              ; sys_open\n");
            fprintf(output, "    mov rdi, rbp\n");
            fprintf(output, "    sub rdi, %d             ; variable offset\n", getVariableOffset(node->namespaceExpr->token.value));
            fprintf(output, "    mov rsi, 0              ; flags = O_RDONLY\n");
            fprintf(output, "    mov rdx, 0              ; mode\n");
            fprintf(output, "    syscall\n");
            // ... 后续类似上面的实现
            fprintf(output, "    ; read and close...\n");
        }
    } else {
        // 标准输入
        if (node->left) {
            generateNASMOutputRecursive(node->left);
        }
        fprintf(output, "    call _read_str\n");
        if (node->right && node->right->type == AST_IDENTIFIER) {
            char *varName = node->right->token.value;
            int offset = getVariableOffset(varName);
            if (offset != -1) {
                fprintf(output, "    mov [rbp - %d], rax\n", offset);
            }
        }
    }
}

void generateNASMUnaryOp(ASTNode *node) {
    if (!node || !node->left) return;

    generateNASMCode(node->left);
    fprintf(output, "    pop rax\n");
    
    switch (node->token.type) {
        case TOKEN_MINUS:
            fprintf(output, "    neg rax\n");
            break;
        case TOKEN_NOT:
            fprintf(output, "    not rax\n");
            break;
        case TOKEN_PLUS:
            // 一元加号不需要操作
            break;
        default:
            break;
    }
    
    fprintf(output, "    push rax\n");
}

void generateNASMAssignment(ASTNode *node) {
    if (!node) return;

    if (node->left && node->left->type == AST_IDENTIFIER) {
        char *varName = node->left->token.value;
        int offset = getVariableOffset(varName);
        if (offset < 0) {
            offset = 8;
        }

        if (node->right) {
            generateNASMExpression(node->right);
            fprintf(output, "    pop rax\n");
            fprintf(output, "    mov [rbp - %d], rax\n", offset);
        }
    } else if (node->left && node->left->type == AST_OBJECT_ACCESS) {
        // 对象成员赋值
        ASTNode *objAccess = node->left;
        if (objAccess->left->type == AST_IDENTIFIER && objAccess->right && objAccess->right->type == AST_IDENTIFIER) {
            char *objName = objAccess->left->token.value;
            char *memberName = objAccess->right->token.value;
            if (isObjectName(objName)) {
                if (node->right) {
                    generateNASMExpression(node->right);
                    fprintf(output, "    pop rax\n");
                    // 检查对象成员类型，决定mov的大小
                    // 先检查对象成员类型
                    int found = 0;
                    const char *typeName = NULL;
                    for (int i = 0; i < objectCount && !found; i++) {
                        if (strcmp(objectTable[i].name, objName) == 0) {
                            ASTNode *member = objectTable[i].node->body;
                            while (member) {
                                if (member->type == AST_OBJECT_MEMBER && strcmp(member->token.value, memberName) == 0) {
                                    typeName = member->namespace;
                                    found = 1;
                                    break;
                                }
                                member = member->next;
                            }
                        }
                    }
                    if (found) {
                        if (strcmp(typeName, "string") == 0) {
                            // 字符串赋值需要复制
                            // 我们假设右边是字符串字面量或字符串变量
                            fprintf(output, "    mov rsi, rax\n");
                            fprintf(output, "    mov rdi, __obj_%s_%s\n", objName, memberName);
                            fprintf(output, "    mov rcx, 0\n");
                            fprintf(output, "__obj_assign_str_%d:\n", nasmLabelCounter);
                            fprintf(output, "    mov al, [rsi + rcx]\n");
                            fprintf(output, "    mov [rdi + rcx], al\n");
                            fprintf(output, "    cmp al, 0\n");
                            fprintf(output, "    je __obj_assign_str_done_%d\n", nasmLabelCounter);
                            fprintf(output, "    inc rcx\n");
                            fprintf(output, "    cmp rcx, 255\n");
                            fprintf(output, "    jl __obj_assign_str_%d\n", nasmLabelCounter);
                            fprintf(output, "__obj_assign_str_done_%d:\n", nasmLabelCounter);
                            nasmLabelCounter++;
                        } else if (strcmp(typeName, "char") == 0) {
                            fprintf(output, "    mov byte [__obj_%s_%s], al\n", objName, memberName);
                        } else {
                            fprintf(output, "    mov qword [__obj_%s_%s], rax\n", objName, memberName);
                        }
                    } else {
                        fprintf(output, "    mov qword [__obj_%s_%s], rax\n", objName, memberName);
                    }
                }
            }
        }
    }
}

void generateNASMStructDeclaration(ASTNode *node) {
    if (!node) return;
    fprintf(output, "; Struct %s (NASM backend does not support structs natively)\n", node->namespace);
}

void generateNASMEnumDeclaration(ASTNode *node) {
    if (!node) return;
    fprintf(output, "; Enum %s (NASM backend does not support enums natively)\n", node->namespace);
}

void generateNASMMemberAccess(ASTNode *node) {
    if (!node || !node->left) return;
    generateNASMCode(node->left);
    fprintf(output, "    pop rax\n");
    if (node->right && node->right->type == AST_IDENTIFIER) {
        fprintf(output, "    mov rax, [rax + %s_offset]\n", node->right->token.value);
    }
    fprintf(output, "    push rax\n");
}

void generateNASMDereference(ASTNode *node) {
    if (!node || !node->left) return;
    generateNASMCode(node->left);
    fprintf(output, "    pop rax\n");
    fprintf(output, "    mov rax, [rax]\n");
    fprintf(output, "    push rax\n");
}

void generateNASMAddressOf(ASTNode *node) {
    if (!node || !node->left) return;
    if (node->left->type == AST_IDENTIFIER) {
        char *varName = node->left->token.value;
        int offset = getVariableOffset(varName);
        if (offset != -1) {
            fprintf(output, "    lea rax, [rbp - %d]\n", offset);
            fprintf(output, "    push rax\n");
        }
    }
}

void generateNASMAllocCall(ASTNode *node) {
    if (!node) return;
    if (node->params) {
        generateNASMCode(node->params);
        fprintf(output, "    pop rcx\n");
        fprintf(output, "    call _malloc\n");
        fprintf(output, "    push rax\n");
    }
}

void generateNASMFreeCall(ASTNode *node) {
    if (!node) return;
    if (node->params) {
        generateNASMCode(node->params);
        fprintf(output, "    pop rcx\n");
        fprintf(output, "    call _free\n");
    }
}

void generateNASMObjectDeclaration(ASTNode *node) {
    if (!node) return;
    // 对象定义已经在数据段处理了，这里不需要做什么
}

void generateNASMObjectMember(ASTNode *node) {
    if (!node) return;
    // 对象成员已经在对象定义时处理了
}

// 辅助函数：检查是否是对象名
int isObjectName(const char *name) {
    for (int i = 0; i < objectCount; i++) {
        if (strcmp(objectTable[i].name, name) == 0) {
            return 1;
        }
    }
    return 0;
}

void generateNASMObjectAccess(ASTNode *node) {
    if (!node || !node->left) return;
    
    if (node->left->type == AST_IDENTIFIER && node->right && node->right->type == AST_IDENTIFIER) {
        char *objName = node->left->token.value;
        char *memberName = node->right->token.value;
        
        // 检查是否是对象名
        if (isObjectName(objName)) {
            // 获取对象成员类型
            const char *typeName = NULL;
            int found = 0;
            for (int i = 0; i < objectCount && !found; i++) {
                if (strcmp(objectTable[i].name, objName) == 0) {
                    ASTNode *member = objectTable[i].node->body;
                    while (member) {
                        if (member->type == AST_OBJECT_MEMBER && strcmp(member->token.value, memberName) == 0) {
                            typeName = member->namespace;
                            found = 1;
                            break;
                        }
                        member = member->next;
                    }
                }
            }
            
            if (found) {
                if (strcmp(typeName, "string") == 0) {
                    // 字符串：推地址
                    fprintf(output, "    push __obj_%s_%s\n", objName, memberName);
                } else if (strcmp(typeName, "char") == 0) {
                    // 字符：加载值并推
                    fprintf(output, "    mov al, [__obj_%s_%s]\n", objName, memberName);
                    fprintf(output, "    movzx rax, al\n");
                    fprintf(output, "    push rax\n");
                } else {
                    // 其他类型：加载值并推
                    fprintf(output, "    mov rax, [__obj_%s_%s]\n", objName, memberName);
                    fprintf(output, "    push rax\n");
                }
            } else {
                // 默认：加载值并推
                fprintf(output, "    mov rax, [__obj_%s_%s]\n", objName, memberName);
                fprintf(output, "    push rax\n");
            }
        } else {
            // 不是已知对象，尝试正常处理
            generateNASMCode(node->left);
            fprintf(output, "    pop rax\n");
        }
    } else {
        // 其他情况
        generateNASMCode(node->left);
        fprintf(output, "    pop rax\n");
    }
}

void generateNASMAsm(ASTNode *node) {
    if (!node) return;

    char *asmCode = node->filename;
    char buffer[1024];
    char *p = asmCode;
    char *q = buffer;
    int bufferLen = 0;
    
    while (*p) {
        // 处理虚拟寄存器 xl 和 xh
        if (p[0] == 'x' && p[1] == 'l') {
            fprintf(output, "    ; xl (虚拟通讯寄存器1)\n");
            fprintf(output, "    mov rax, [rbp - 808]  ; 从保留位置读取 xl\n");
            fprintf(output, "    push rax\n");
            p += 2;
            continue;
        }
        if (p[0] == 'x' && p[1] == 'h') {
            fprintf(output, "    ; xh (虚拟通讯寄存器2)\n");
            fprintf(output, "    mov rax, [rbp - 816]  ; 从保留位置读取 xh\n");
            fprintf(output, "    push rax\n");
            p += 2;
            continue;
        }
        
        // 处理 mov xl, ... 指令
        if (strncmp(p, "mov xl,", 6) == 0) {
            p += 6;
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
            
            // 解析右边的值
            if (*p == '"') {
                // 字符串
                char str[256];
                int i = 0;
                p++;
                while (*p && *p != '"' && i < 255) {
                    str[i++] = *p++;
                }
                str[i] = '\0';
                if (*p == '"') p++;
                
                // 创建字符串标签
                char strLabel[32];
                sprintf(strLabel, ".str_%d", nasmLabelCounter++);
                fprintf(output, "    %s: db \"%s\", 0\n", strLabel, str);
                fprintf(output, "    mov rax, %s\n", strLabel);
                fprintf(output, "    mov qword [rbp - 808], rax  ; 保存到 xl\n");
            } else if (isdigit(*p) || *p == '-') {
                // 数字
                char num[32];
                int i = 0;
                while (*p && (isdigit(*p) || *p == '-') && i < 31) {
                    num[i++] = *p++;
                }
                num[i] = '\0';
                fprintf(output, "    mov rax, %s\n", num);
                fprintf(output, "    mov qword [rbp - 808], rax  ; 保存到 xl\n");
            } else {
                // 其他情况，直接输出
                fprintf(output, "    mov qword [rbp - 808], ");
                while (*p && *p != ';' && *p != '\n') {
                    fprintf(output, "%c", *p);
                    p++;
                }
                fprintf(output, "\n");
            }
            continue;
        }
        
        // 处理 mov xh, ... 指令
        if (strncmp(p, "mov xh,", 6) == 0) {
            p += 6;
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;
            
            if (*p == '"') {
                char str[256];
                int i = 0;
                p++;
                while (*p && *p != '"' && i < 255) {
                    str[i++] = *p++;
                }
                str[i] = '\0';
                if (*p == '"') p++;
                
                char strLabel[32];
                sprintf(strLabel, ".str_%d", nasmLabelCounter++);
                fprintf(output, "    %s: db \"%s\", 0\n", strLabel, str);
                fprintf(output, "    mov rax, %s\n", strLabel);
                fprintf(output, "    mov qword [rbp - 816], rax  ; 保存到 xh\n");
            } else if (isdigit(*p) || *p == '-') {
                char num[32];
                int i = 0;
                while (*p && (isdigit(*p) || *p == '-') && i < 31) {
                    num[i++] = *p++;
                }
                num[i] = '\0';
                fprintf(output, "    mov rax, %s\n", num);
                fprintf(output, "    mov qword [rbp - 816], rax  ; 保存到 xh\n");
            } else {
                fprintf(output, "    mov qword [rbp - 816], ");
                while (*p && *p != ';' && *p != '\n') {
                    fprintf(output, "%c", *p);
                    p++;
                }
                fprintf(output, "\n");
            }
            continue;
        }
        
        // 处理 call 指令
        if (strncmp(p, "call ", 5) == 0) {
            p += 5;
            while (*p == ' ' || *p == '\t') p++;
            
            // 检查是否是 eout 调用
            if (strncmp(p, "eout", 4) == 0) {
                p += 4;
                while (*p == ' ' || *p == '\t') p++;
                
                // 解析命名空间 [std]
                char ns[32] = "std";
                if (*p == '[') {
                    p++;
                    int i = 0;
                    while (*p && *p != ']' && i < 31) {
                        ns[i++] = *p++;
                    }
                    ns[i] = '\0';
                    if (*p == ']') p++;
                }
                
                // 解析参数列表
                fprintf(output, "    ; eout call with namespace %s\n", ns);
                
                while (*p && *p != ';' && *p != '\n') {
                    while (*p == ' ' || *p == '\t') p++;
                    
                    // 处理 _al, _ah 寄存器引用
                        if (p[0] == '_' && p[1] == 'a' && p[2] == 'l') {
                            fprintf(output, "    push rax\n");
                            fprintf(output, "    call _print_str\n");
                            p += 3;
                        } else if (p[0] == '_' && p[1] == 'a' && p[2] == 'h') {
                            fprintf(output, "    movzx rcx, ah\n");
                            fprintf(output, "    push rcx\n");
                            fprintf(output, "    call _string\n");
                            fprintf(output, "    push rax\n");
                            fprintf(output, "    call _print_str\n");
                            p += 3;
                        } else if (p[0] == '_' && p[1] == 'x' && p[2] == 'l') {
                            fprintf(output, "    mov rcx, [rbp - 808]\n");
                            fprintf(output, "    push rcx\n");
                            fprintf(output, "    call _print_str\n");
                            p += 3;
                        } else if (p[0] == '_' && p[1] == 'x' && p[2] == 'h') {
                            fprintf(output, "    mov rcx, [rbp - 816]\n");
                            fprintf(output, "    push rcx\n");
                            fprintf(output, "    call _print_str\n");
                            p += 3;
                        } else if (strcmp(p, "endl") == 0) {
                            fprintf(output, "    call _print_nl\n");
                            p += 5;
                    } else if (*p == '&') {
                        p++;
                    } else {
                        p++;
                    }
                }
                continue;
            }
            
            // 处理其他函数调用
            char funcName[64];
            int i = 0;
            while (*p && *p != ' ' && *p != '\t' && *p != ',' && *p != ';' && i < 63) {
                funcName[i++] = *p++;
            }
            funcName[i] = '\0';
            
            fprintf(output, "    call %s\n", funcName);
            
            // 处理参数（_ah 等寄存器引用）
            while (*p && *p != ';' && *p != '\n') {
                if (p[0] == '_' && p[1] == 'a' && p[2] == 'h') {
                    fprintf(output, "    mov rdi, ah\n");
                    p += 3;
                } else if (*p == ',') {
                    p++;
                } else {
                    p++;
                }
            }
            continue;
        }
        
        // 直接输出其他汇编指令
        if (bufferLen < 1023) {
            buffer[bufferLen++] = *p;
        }
        p++;
        
        // 检查是否需要输出
        if (*(p-1) == '\n' || *(p-1) == ';' || *(p-1) == '\r') {
            buffer[bufferLen] = '\0';
            fprintf(output, "    %s", buffer);
            bufferLen = 0;
        }
    }
    
    // 输出剩余的缓冲区内容
    if (bufferLen > 0) {
        buffer[bufferLen] = '\0';
        fprintf(output, "    %s\n", buffer);
    }
    
    // 如果没有指定索引，默认将 rax 的值压入栈
    if (!node->left) {
        fprintf(output, "    push rax\n");
    } else {
        // 处理数组索引访问
        int index = atoi(node->left->token.value);
        if (index == 0) {
            fprintf(output, "    mov rax, [rbp - 808]  ; 返回 xl\n");
            fprintf(output, "    push rax\n");
        } else if (index == 1) {
            fprintf(output, "    mov rax, [rbp - 816]  ; 返回 xh\n");
            fprintf(output, "    push rax\n");
        }
    }
}

void generateARMCode(ASTNode *node);
void generateARMStatement(ASTNode *node);
void generateARMExpression(ASTNode *node);
void generateARMBinaryOp(ASTNode *node);
void generateARMLiteral(ASTNode *node);
void generateARMIdentifier(ASTNode *node);
void generateARMFunctionDeclaration(ASTNode *node);
void generateARMFunctionCall(ASTNode *node);
void generateARMOutput(ASTNode *node);

void generateARMProgram(ASTNode *node) {
    clearVariableTable();
    nasmLabelCounter = 0;
    objectCount = 0;

    collectObjects(node);
    collectStrings(node);

    fprintf(output, "# AArch64 Assembly Code Generated by E Compiler\n");
    if (targetPlatform == TARGET_ARM_LINUX) {
        fprintf(output, "# Architecture: AArch64 Linux\n\n");
    } else {
        fprintf(output, "# Architecture: AArch64 Android\n\n");
    }

    fprintf(output, ".data\n");
    fprintf(output, "    __newline: .byte 10\n");
    fprintf(output, "    __str_buf: .space 256\n");
    
    // 输出收集到的字符串
    for (int i = 0; i < stringMapCount; i++) {
        fprintf(output, "    %s: .asciz \"%s\"\n", stringMapLabels[i], stringMapStrings[i]);
    }
    
    fprintf(output, "    __str_Sum_of: .asciz \"Sum of \"\n");
    fprintf(output, "    __str_and: .asciz \" and \"\n");
    fprintf(output, "    __str_is: .asciz \" is \"\n");
    
    // 输出对象定义
    for (int i = 0; i < objectCount; i++) {
        ASTNode *objNode = objectTable[i].node;
        fprintf(output, "\n    # Object: %s\n", objectTable[i].name);
        ASTNode *member = objNode->body;
        while (member) {
            if (member->type == AST_OBJECT_MEMBER) {
                const char *typeName = member->namespace;
                const char *memberName = member->token.value;
                if (strcmp(typeName, "string") == 0) {
                    fprintf(output, "    __obj_%s_%s: .space 256\n", objectTable[i].name, memberName);
                } else if (strcmp(typeName, "int") == 0 || strcmp(typeName, "float") == 0 || strcmp(typeName, "double") == 0) {
                    fprintf(output, "    __obj_%s_%s: .quad 0\n", objectTable[i].name, memberName);
                } else if (strcmp(typeName, "char") == 0) {
                    fprintf(output, "    __obj_%s_%s: .byte 0\n", objectTable[i].name, memberName);
                } else {
                    fprintf(output, "    __obj_%s_%s: .quad 0\n", objectTable[i].name, memberName);
                }
            }
            member = member->next;
        }
    }
    
    fprintf(output, "    __stdout_buf: .space 1024\n");
    fprintf(output, "    __stdout_buf_pos: .word 0\n");
    fprintf(output, "\n");

    fprintf(output, ".text\n");
    fprintf(output, "    .global _start\n");
    fprintf(output, "    .global main\n\n");

    fprintf(output, "# Entry point\n");
    fprintf(output, "_start:\n");
    fprintf(output, "    bl main\n");
    fprintf(output, "    # Exit syscall\n");
    fprintf(output, "    mov x0, #0\n");
    fprintf(output, "    mov x8, #93\n");
    fprintf(output, "    svc #0\n\n");

    fprintf(output, "# Helper function: string(int) -> char*\n");
    fprintf(output, "string_func:\n");
    fprintf(output, "    stp x29, x30, [sp, -64]!\n");
    fprintf(output, "    mov x29, sp\n");
    fprintf(output, "    adrp x4, __str_buf\n");
    fprintf(output, "    add x4, x4, :lo12:__str_buf\n");
    fprintf(output, "    mov w5, #0\n");
    fprintf(output, "    mov w6, #10\n");
    fprintf(output, "string_loop:\n");
    fprintf(output, "    mov w7, w0\n");
    fprintf(output, "    bl div10\n");
    fprintf(output, "    add w2, w1, #'0'\n");
    fprintf(output, "    strb w2, [x4, w5, uxtw]\n");
    fprintf(output, "    add w5, w5, #1\n");
    fprintf(output, "    cmp w0, #0\n");
    fprintf(output, "    b.ne string_loop\n");
    fprintf(output, "    mov w2, #0\n");
    fprintf(output, "    strb w2, [x4, w5, uxtw]\n");
    fprintf(output, "    # Reverse string\n");
    fprintf(output, "    mov w2, #0\n");
    fprintf(output, "    sub w3, w5, #1\n");
    fprintf(output, "string_rev:\n");
    fprintf(output, "    cmp w2, w3\n");
    fprintf(output, "    b.ge string_done\n");
    fprintf(output, "    ldrb w6, [x4, w2, uxtw]\n");
    fprintf(output, "    ldrb w7, [x4, w3, uxtw]\n");
    fprintf(output, "    strb w7, [x4, w2, uxtw]\n");
    fprintf(output, "    strb w6, [x4, w3, uxtw]\n");
    fprintf(output, "    add w2, w2, #1\n");
    fprintf(output, "    sub w3, w3, #1\n");
    fprintf(output, "    b string_rev\n");
    fprintf(output, "string_done:\n");
    fprintf(output, "    adrp x0, __str_buf\n");
    fprintf(output, "    add x0, x0, :lo12:__str_buf\n");
    fprintf(output, "    ldp x29, x30, [sp], 64\n");
    fprintf(output, "    ret\n\n");

    fprintf(output, "# Helper function: divide by 10\n");
    fprintf(output, "div10:\n");
    fprintf(output, "    mov w1, #0\n");
    fprintf(output, "div10_loop:\n");
    fprintf(output, "    cmp w0, #10\n");
    fprintf(output, "    b.lt div10_done\n");
    fprintf(output, "    sub w0, w0, #10\n");
    fprintf(output, "    add w1, w1, #1\n");
    fprintf(output, "    b div10_loop\n");
    fprintf(output, "div10_done:\n");
    fprintf(output, "    ret\n\n");

    fprintf(output, "# Helper function: flush stdout buffer\n");
    fprintf(output, "_flush_stdout:\n");
    fprintf(output, "    stp x29, x30, [sp, -64]!\n");
    fprintf(output, "    mov x29, sp\n");
    fprintf(output, "    adrp x0, __stdout_buf_pos\n");
    fprintf(output, "    add x0, x0, :lo12:__stdout_buf_pos\n");
    fprintf(output, "    ldr w1, [x0]\n");
    fprintf(output, "    cmp w1, #0\n");
    fprintf(output, "    b.eq _flush_done\n");
    fprintf(output, "    mov x0, #1\n");
    fprintf(output, "    adrp x1, __stdout_buf\n");
    fprintf(output, "    add x1, x1, :lo12:__stdout_buf\n");
    fprintf(output, "    adrp x2, __stdout_buf_pos\n");
    fprintf(output, "    add x2, x2, :lo12:__stdout_buf_pos\n");
    fprintf(output, "    ldr w2, [x2]\n");
    fprintf(output, "    mov x8, #64\n");
    fprintf(output, "    svc #0\n");
    fprintf(output, "    adrp x0, __stdout_buf_pos\n");
    fprintf(output, "    add x0, x0, :lo12:__stdout_buf_pos\n");
    fprintf(output, "    mov w1, #0\n");
    fprintf(output, "    str w1, [x0]\n");
    fprintf(output, "_flush_done:\n");
    fprintf(output, "    ldp x29, x30, [sp], 64\n");
    fprintf(output, "    ret\n\n");

    fprintf(output, "# Helper function: print string (buffered)\n");
    fprintf(output, "_print_str:\n");
    fprintf(output, "    stp x29, x30, [sp, -64]!\n");
    fprintf(output, "    mov x29, sp\n");
    fprintf(output, "    mov x4, x0\n");
    fprintf(output, "    adrp x5, __stdout_buf\n");
    fprintf(output, "    add x5, x5, :lo12:__stdout_buf\n");
    fprintf(output, "    adrp x6, __stdout_buf_pos\n");
    fprintf(output, "    add x6, x6, :lo12:__stdout_buf_pos\n");
    fprintf(output, "    ldr w7, [x6]\n");
    fprintf(output, "_print_str_loop:\n");
    fprintf(output, "    ldrb w8, [x4], #1\n");
    fprintf(output, "    cmp w8, #0\n");
    fprintf(output, "    b.eq _print_str_done\n");
    fprintf(output, "    strb w8, [x5, w7, uxtw]\n");
    fprintf(output, "    add w7, w7, #1\n");
    fprintf(output, "    cmp w7, #1024\n");
    fprintf(output, "    b.eq _print_str_flush\n");
    fprintf(output, "    b _print_str_loop\n");
    fprintf(output, "_print_str_flush:\n");
    fprintf(output, "    str w7, [x6]\n");
    fprintf(output, "    bl _flush_stdout\n");
    fprintf(output, "    ldr w7, [x6]\n");
    fprintf(output, "    b _print_str_loop\n");
    fprintf(output, "_print_str_done:\n");
    fprintf(output, "    str w7, [x6]\n");
    fprintf(output, "    ldp x29, x30, [sp], 64\n");
    fprintf(output, "    ret\n\n");

    fprintf(output, "# Helper function: print newline (buffered)\n");
    fprintf(output, "_print_nl:\n");
    fprintf(output, "    stp x29, x30, [sp, -64]!\n");
    fprintf(output, "    mov x29, sp\n");
    fprintf(output, "    adrp x0, __stdout_buf\n");
    fprintf(output, "    add x0, x0, :lo12:__stdout_buf\n");
    fprintf(output, "    adrp x1, __stdout_buf_pos\n");
    fprintf(output, "    add x1, x1, :lo12:__stdout_buf_pos\n");
    fprintf(output, "    ldr w2, [x1]\n");
    fprintf(output, "    mov w3, #10\n");
    fprintf(output, "    strb w3, [x0, w2, uxtw]\n");
    fprintf(output, "    add w2, w2, #1\n");
    fprintf(output, "    str w2, [x1]\n");
    fprintf(output, "    cmp w2, #1024\n");
    fprintf(output, "    b.ne _print_nl_done\n");
    fprintf(output, "    bl _flush_stdout\n");
    fprintf(output, "_print_nl_done:\n");
    fprintf(output, "    ldp x29, x30, [sp], 64\n");
    fprintf(output, "    ret\n\n");

    if (node->body) {
        if (node->body->type == AST_PROGRAM) {
            generateARMProgram(node->body);
        } else {
            generateARMCode(node->body);
        }
    }
    if (node->next) {
        if (node->next->type == AST_PROGRAM) {
            generateARMProgram(node->next);
        } else {
            generateARMCode(node->next);
        }
    }
}

void expandCodeMacroARM(CodeMacro *macro, ASTNode *args) {
    if (!macro || !macro->body) return;
    
    int uniqueId = getMacroUniqueId();
    
    ASTNode *expandedBody = macro->body;
    
    if (macro->params && args) {
        expandedBody = substituteParams(macro->body, macro->params, args);
    } else {
        expandedBody = cloneASTNode(macro->body);
    }
    
    renameVariablesInNode(expandedBody, uniqueId);
    
    generateARMCode(expandedBody);
}

void generateARMCode(ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case AST_PROGRAM:
            generateARMProgram(node);
            break;
        case AST_STATEMENT:
            generateARMStatement(node);
            break;
        case AST_EXPRESSION:
            generateARMExpression(node);
            break;
        case AST_ASSIGNMENT:
            if (node->left && node->left->type == AST_OBJECT_ACCESS) {
                ASTNode *objAccess = node->left;
                if (objAccess->left->type == AST_IDENTIFIER && objAccess->right && objAccess->right->type == AST_IDENTIFIER) {
                    char *objName = objAccess->left->token.value;
                    char *memberName = objAccess->right->token.value;
                    if (isObjectName(objName)) {
                        const char *typeName = NULL;
                        int found = 0;
                        for (int i = 0; i < objectCount && !found; i++) {
                            if (strcmp(objectTable[i].name, objName) == 0) {
                                ASTNode *member = objectTable[i].node->body;
                                while (member) {
                                    if (member->type == AST_OBJECT_MEMBER && strcmp(member->token.value, memberName) == 0) {
                                        typeName = member->namespace;
                                        found = 1;
                                        break;
                                    }
                                    member = member->next;
                                }
                            }
                        }
                        
                        generateARMExpression(node->right);
                        
                        if (found) {
                            if (strcmp(typeName, "string") == 0) {
                                fprintf(output, "    mov x1, x0\n");
                                fprintf(output, "    adrp x0, __obj_%s_%s\n", objName, memberName);
                                fprintf(output, "    add x0, x0, :lo12:__obj_%s_%s\n", objName, memberName);
                                fprintf(output, "    mov w2, #0\n");
                                fprintf(output, "__obj_assign_str_%d:\n", nasmLabelCounter);
                                fprintf(output, "    ldrb w3, [x1, w2, uxtw]\n");
                                fprintf(output, "    strb w3, [x0, w2, uxtw]\n");
                                fprintf(output, "    cmp w3, #0\n");
                                fprintf(output, "    b.eq __obj_assign_str_done_%d\n", nasmLabelCounter);
                                fprintf(output, "    add w2, w2, #1\n");
                                fprintf(output, "    cmp w2, #255\n");
                                fprintf(output, "    b.lt __obj_assign_str_%d\n", nasmLabelCounter);
                                fprintf(output, "__obj_assign_str_done_%d:\n", nasmLabelCounter);
                                nasmLabelCounter++;
                            } else if (strcmp(typeName, "char") == 0) {
                                fprintf(output, "    adrp x1, __obj_%s_%s\n", objName, memberName);
                                fprintf(output, "    add x1, x1, :lo12:__obj_%s_%s\n", objName, memberName);
                                fprintf(output, "    strb w0, [x1]\n");
                            } else {
                                fprintf(output, "    adrp x1, __obj_%s_%s\n", objName, memberName);
                                fprintf(output, "    add x1, x1, :lo12:__obj_%s_%s\n", objName, memberName);
                                fprintf(output, "    str x0, [x1]\n");
                            }
                        }
                        break;
                    }
                }
            }
            generateARMExpression(node->right);
            fprintf(output, "    str x0, [sp, -8]!\n");
            generateARMIdentifier(node->left);
            fprintf(output, "    ldr x1, [sp], 8\n");
            fprintf(output, "    str x1, [x0]\n");
            break;
        case AST_FUNCTION_DECLARATION:
            generateARMFunctionDeclaration(node);
            break;
        case AST_FUNCTION_CALL:
            {
                CodeMacro *macro = findCodeMacro(node->token.value);
                if (macro) {
                    expandCodeMacroARM(macro, node->params);
                    break;
                }
                generateARMFunctionCall(node);
                break;
            }
        case AST_OUTPUT:
            generateARMOutput(node);
            break;
        case AST_INPUT:
            generateARMInput(node);
            break;
        case AST_LITERAL:
            generateARMLiteral(node);
            break;
        case AST_IDENTIFIER:
            generateARMIdentifier(node);
            break;
        case AST_BINARY_OP:
            generateARMBinaryOp(node);
            break;
        case AST_STRING_CONCAT:
            generateARMBinaryOp(node);
            break;
        case AST_UNARY_OP:
            generateARMUnaryOp(node);
            break;
        case AST_STRUCT_DECLARATION:
            generateARMStructDeclaration(node);
            break;
        case AST_ENUM_DECLARATION:
            generateARMEnumDeclaration(node);
            break;
        case AST_MEMBER_ACCESS:
            generateARMMemberAccess(node);
            break;
        case AST_DEREFERENCE:
            generateARMDereference(node);
            break;
        case AST_ADDRESS_OF:
            generateARMAddressOf(node);
            break;
        case AST_ALLOC_CALL:
            generateARMAllocCall(node);
            break;
        case AST_FREE_CALL:
            generateARMFreeCall(node);
            break;
        case AST_OBJECT_DECLARATION:
            generateARMObjectDeclaration(node);
            break;
        case AST_OBJECT_MEMBER:
            generateARMObjectMember(node);
            break;
        case AST_OBJECT_ACCESS:
            generateARMObjectAccess(node);
            break;
        case AST_MACRO_DEFINITION:
            {
                if (strcmp(node->filename, "undef") == 0) {
                    break;
                }
                addCodeMacro(node->namespace, node->params, node->body);
                break;
            }
        case AST_SWITCH:
            generateARMSwitchStatement(node);
            break;
        case AST_CASE:
            // handled in generateARMSwitchStatement
            break;
        case AST_BREAK:
            if (currentSwitchEndLabel) {
                fprintf(output, "    b %s\n", currentSwitchEndLabel);
            }
            break;
        case AST_RETURN:
            if (node->left) {
                generateARMExpression(node->left);
            }
            // We need the same frameSize as in function declaration
            // For simplicity, use a fixed safe size for now
            // TODO: Store frameSize in a variable during function declaration
            fprintf(output, "    ldp x29, x30, [sp], #64\n");
            fprintf(output, "    ret\n");
            break;
        case AST_FOR:
        case AST_WHILE:
        case AST_IF:
            generateARMStatement(node);
            break;
        case AST_VARIABLE_DECLARATION:
            {
                ASTNode *decl = node;
                while (decl) {
                    // Variable name is stored in decl->left->token.value
                    char *varName = "";
                    if (decl->left) {
                        varName = decl->left->token.value;
                    }
                    if (varName[0] != '\0') {
                        int offset = addVariable(varName);
                        if (decl->right) {
                            // Generate expression to evaluate RHS into x0
                            generateARMExpression(decl->right);
                            // Store result to local variable
                            fprintf(output, "    str x0, [x29, #%d]\n", offset);
                        }
                    }
                    decl = decl->next;
                }
            }
            break;
        default:
            break;
    }

    if (node->next) {
        generateARMCode(node->next);
    }
}

void generateARMStatement(ASTNode *node) {
    if (!node) return;

    if (node->left) {
        if (node->left->type == AST_ASSIGNMENT) {
            generateARMExpression(node->left->right);
            fprintf(output, "    str x0, [sp, -8]!\n");
            generateARMIdentifier(node->left->left);
            fprintf(output, "    ldr x1, [sp], 8\n");
            fprintf(output, "    str x1, [x0]\n");
        } else {
            generateARMCode(node->left);
        }
    }
}

void generateARMExpression(ASTNode *node) {
    if (!node) return;

    if (node->type == AST_BINARY_OP) {
        generateARMBinaryOp(node);
    } else if (node->type == AST_LITERAL) {
        generateARMLiteral(node);
    } else if (node->type == AST_IDENTIFIER) {
        generateARMIdentifier(node);
        fprintf(output, "    ldr x0, [x0]\n");
    } else if (node->type == AST_FUNCTION_CALL) {
        generateARMFunctionCall(node);
    } else if (node->type == AST_IDENTIFIER) {
        generateARMLoadValue(node);
    } else {
        generateARMCode(node);
    }
}

void generateARMBinaryOp(ASTNode *node) {
    if (!node) return;

    generateARMExpression(node->left);
    fprintf(output, "    str x0, [sp, -8]!\n");
    generateARMExpression(node->right);
    fprintf(output, "    ldr x1, [sp], 8\n");

    switch (node->token.type) {
        case TOKEN_PLUS:
            fprintf(output, "    add x0, x1, x0\n");
            break;
        case TOKEN_MINUS:
            fprintf(output, "    sub x0, x1, x0\n");
            break;
        case TOKEN_MUL:
            fprintf(output, "    mul x0, x1, x0\n");
            break;
        case TOKEN_DIV:
            fprintf(output, "    bl divide\n");
            break;
        default:
            break;
    }
}

void generateARMLiteral(ASTNode *node) {
    if (!node) return;

    if (node->token.type == TOKEN_NUMBER) {
        fprintf(output, "    mov x0, #%s\n", node->token.value);
    } else if (node->token.type == TOKEN_STRING) {
        fprintf(output, "    adrp x0, __str_buf\n");
        fprintf(output, "    add x0, x0, :lo12:__str_buf\n");
        fprintf(output, "    mov x1, x0\n");
        int len = strlen(node->token.value);
        for (int i = 0; i < len; i++) {
            fprintf(output, "    mov w2, #%d\n", node->token.value[i]);
            fprintf(output, "    strb w2, [x1]\n");
            fprintf(output, "    add x1, x1, #1\n");
        }
        fprintf(output, "    mov w2, #0\n");
        fprintf(output, "    strb w2, [x1]\n");
    }
}

void generateARMLoadValue(ASTNode *node) {
    if (!node) return;

    int offset = getVariableOffset(node->token.value);
    if (offset >= 0) {
        // Local variable - load value from stack
        fprintf(output, "    ldr x0, [x29, #%d]\n", offset);
    } else {
        // Global variable - load from data section
        fprintf(output, "    adrp x1, %s\n", node->token.value);
        fprintf(output, "    ldr x0, [x1, :lo12:%s]\n", node->token.value);
    }
}

void generateARMIdentifier(ASTNode *node) {
    if (!node) return;

    int offset = getVariableOffset(node->token.value);
    if (offset >= 0) {
        // Local variable - compute address (base pointer + offset)
        fprintf(output, "    add x0, x29, #%d\n", offset);
    } else {
        // Global variable - load address
        fprintf(output, "    adrp x0, %s\n", node->token.value);
        fprintf(output, "    ldr x0, [x0, :lo12:%s]\n", node->token.value);
    }
}

void generateARMFunctionDeclaration(ASTNode *node) {
    if (!node) return;

    // Clear and setup variable table
    clearVariableTable();
    nasmLabelCounter = 0;

    // Register function parameters (ARM64 uses x0-x7 for first 8 args)
    ASTNode *param = node->params;
    int paramIndex = 0;
    while (param) {
        addVariable(param->token.value);
        varTable[paramIndex].offset = paramIndex * 8 + 16;  // x0=16, x1=24, ... after frame pointer
        param = param->next;
        paramIndex++;
    }

    // Fixed stack frame size for simplicity (16-byte aligned)
    int frameSize = 64;  // Fixed size for all functions

    fprintf(output, "%s:\n", node->token.value);
    fprintf(output, "    stp x29, x30, [sp, -%d]!\n", frameSize);
    fprintf(output, "    mov x29, sp\n");

    // Save parameters to stack (x0-x7 to stack)
    param = node->params;
    paramIndex = 0;
    while (param && paramIndex < 8) {
        // Store parameter register to stack
        fprintf(output, "    str x%d, [x29, #%d]\n", paramIndex, 16 + paramIndex * 8);
        param = param->next;
        paramIndex++;
    }

    // 如果是main函数，初始化对象
    if (strcmp(node->token.value, "main") == 0) {
        fprintf(output, "    # Initialize objects\n");
        for (int i = 0; i < objectCount; i++) {
            ASTNode *objNode = objectTable[i].node;
            ASTNode *member = objNode->body;
            while (member) {
                if (member->type == AST_OBJECT_MEMBER && member->right) {
                    const char *typeName = member->namespace;
                    const char *memberName = member->token.value;
                    if (strcmp(typeName, "string") == 0) {
                        if (member->right->type == AST_LITERAL && member->right->token.type == TOKEN_STRING) {
                            const char *strLabel = NULL;
                            for (int j = 0; j < stringMapCount; j++) {
                                if (strcmp(stringMapStrings[j], member->right->token.value) == 0) {
                                    strLabel = stringMapLabels[j];
                                    break;
                                }
                            }
                            if (strLabel) {
                                fprintf(output, "    # Copy string to %s.%s\n", objectTable[i].name, memberName);
                                fprintf(output, "    adrp x0, %s\n", strLabel);
                                fprintf(output, "    add x0, x0, :lo12:%s\n", strLabel);
                                fprintf(output, "    adrp x1, __obj_%s_%s\n", objectTable[i].name, memberName);
                                fprintf(output, "    add x1, x1, :lo12:__obj_%s_%s\n", objectTable[i].name, memberName);
                                fprintf(output, "    mov w2, #0\n");
                                fprintf(output, "__obj_strcpy_%d:\n", nasmLabelCounter);
                                fprintf(output, "    ldrb w3, [x0, w2, uxtw]\n");
                                fprintf(output, "    strb w3, [x1, w2, uxtw]\n");
                                fprintf(output, "    cmp w3, #0\n");
                                fprintf(output, "    b.eq __obj_strcpy_done_%d\n", nasmLabelCounter);
                                fprintf(output, "    add w2, w2, #1\n");
                                fprintf(output, "    cmp w2, #255\n");
                                fprintf(output, "    b.lt __obj_strcpy_%d\n", nasmLabelCounter);
                                fprintf(output, "__obj_strcpy_done_%d:\n", nasmLabelCounter);
                                nasmLabelCounter++;
                            }
                        }
                    } else if (strcmp(typeName, "int") == 0) {
                        if (member->right->type == AST_LITERAL) {
                            fprintf(output, "    mov w0, #%s\n", member->right->token.value);
                            fprintf(output, "    adrp x1, __obj_%s_%s\n", objectTable[i].name, memberName);
                            fprintf(output, "    add x1, x1, :lo12:__obj_%s_%s\n", objectTable[i].name, memberName);
                            fprintf(output, "    str x0, [x1]\n");
                        }
                    }
                }
                member = member->next;
            }
        }
    }

    if (node->body) {
        generateARMCode(node->body);
    }
}

void generateARMOutputRecursive(ASTNode *node) {
    if (!node) return;
    
    if (node->type == AST_STRING_CONCAT) {
        generateARMOutputRecursive(node->left);
        generateARMOutputRecursive(node->right);
        return;
    } else if (node->type == AST_FUNCTION_CALL && strcmp(node->token.value, "string") == 0) {
        generateARMCode(node);
        fprintf(output, "    ldr x0, [sp], 8\n");
        fprintf(output, "    bl _print_str\n");
    } else if (node->type == AST_LITERAL && node->token.type == TOKEN_STRING) {
        generateARMCode(node);
        fprintf(output, "    bl _print_str\n");
    } else {
        generateARMCode(node);
        fprintf(output, "    ldr x0, [sp], 8\n");
        fprintf(output, "    bl _print_str\n");
    }
}

void generateARMFunctionCall(ASTNode *node) {
    if (!node) return;

    if (strcmp(node->token.value, "string") == 0) {
        if (node->params) {
            generateARMExpression(node->params);
        }
        fprintf(output, "    bl string_func\n");
    } else {
        // Count parameters first
        ASTNode *param = node->params;
        int paramCount = 0;
        while (param) {
            paramCount++;
            param = param->next;
        }
        
        // Calculate stack space for parameters (>8 params go on stack)
        int stackParamCount = (paramCount > 8) ? (paramCount - 8) : 0;
        int stackSpace = stackParamCount * 8;
        stackSpace = (stackSpace + 15) & ~15;  // 16-byte alignment
        
        if (stackSpace > 0) {
            fprintf(output, "    sub sp, sp, #%d\n", stackSpace);
        }
        
        // Pass parameters: first 8 in x0-x7, rest on stack
        param = node->params;
        int paramIndex = 0;
        while (param) {
            if (paramIndex < 8) {
                generateARMExpression(param);
                // Move to correct register
                switch (paramIndex) {
                    case 0: break;  // Already in x0
                    case 1: fprintf(output, "    mov x1, x0\n"); break;
                    case 2: fprintf(output, "    mov x2, x0\n"); break;
                    case 3: fprintf(output, "    mov x3, x0\n"); break;
                    case 4: fprintf(output, "    mov x4, x0\n"); break;
                    case 5: fprintf(output, "    mov x5, x0\n"); break;
                    case 6: fprintf(output, "    mov x6, x0\n"); break;
                    case 7: fprintf(output, "    mov x7, x0\n"); break;
                }
            } else {
                // Push to stack
                generateARMExpression(param);
                int stackOffset = (paramIndex - 8) * 8;
                fprintf(output, "    str x0, [sp, #%d]\n", stackOffset);
            }
            param = param->next;
            paramIndex++;
        }
        
        fprintf(output, "    bl %s\n", node->token.value);
        
        if (stackSpace > 0) {
            fprintf(output, "    add sp, sp, #%d\n", stackSpace);
        }
    }
}

void generateARMOutput(ASTNode *node) {
    if (!node) return;

    if (node->left) {
        generateARMOutputRecursive(node->left);
    }

    fprintf(output, "    bl _print_nl\n");
}

void generateARMInput(ASTNode *node) {
    if (!node) return;

    if (node->namespaceExpr && node->namespaceExpr->type == AST_LITERAL &&
        node->namespaceExpr->token.type == TOKEN_DIR) {
        return;
    } else if (node->namespaceExpr && node->namespaceExpr->type == AST_IDENTIFIER &&
               strcmp(node->namespaceExpr->namespace, "std") != 0) {
        return;
    } else {
        if (node->left) {
            generateARMOutputRecursive(node->left);
        }
        fprintf(output, "    bl _read_str\n");
        if (node->right && node->right->type == AST_IDENTIFIER) {
            char *varName = node->right->token.value;
            int offset = getVariableOffset(varName);
            if (offset != -1) {
                fprintf(output, "    str x0, [rbp, #-%d]\n", offset);
            }
        }
    }
}

void generateARMUnaryOp(ASTNode *node) {
    if (!node || !node->left) return;

    generateARMExpression(node->left);
    fprintf(output, "    ldr x0, [sp], 8\n");
    
    switch (node->token.type) {
        case TOKEN_MINUS:
            fprintf(output, "    neg x0, x0\n");
            break;
        case TOKEN_NOT:
            fprintf(output, "    mvn x0, x0\n");
            break;
        case TOKEN_PLUS:
            break;
        default:
            break;
    }
    
    fprintf(output, "    str x0, [sp, -8]!\n");
}

void generateARMStructDeclaration(ASTNode *node) {
    if (!node) return;
}

void generateARMEnumDeclaration(ASTNode *node) {
    if (!node) return;
}

void generateARMMemberAccess(ASTNode *node) {
    if (!node || !node->left) return;
    generateARMCode(node->left);
    fprintf(output, "    ldr x0, [sp], 8\n");
    if (node->right && node->right->type == AST_IDENTIFIER) {
        fprintf(output, "    ldr x0, [x0, #%s_offset]\n", node->right->token.value);
    }
    fprintf(output, "    str x0, [sp, -8]!\n");
}

void generateARMDereference(ASTNode *node) {
    if (!node || !node->left) return;
    generateARMExpression(node->left);
    fprintf(output, "    ldr x0, [sp], 8\n");
    fprintf(output, "    ldr x0, [x0]\n");
    fprintf(output, "    str x0, [sp, -8]!\n");
}

void generateARMAddressOf(ASTNode *node) {
    if (!node || !node->left) return;
    if (node->left->type == AST_IDENTIFIER) {
        char *varName = node->left->token.value;
        int offset = getVariableOffset(varName);
        if (offset != -1) {
            fprintf(output, "    add x0, rbp, #-%d\n", offset);
            fprintf(output, "    str x0, [sp, -8]!\n");
        }
    }
}

void generateARMAllocCall(ASTNode *node) {
    if (!node) return;
    if (node->params) {
        generateARMExpression(node->params);
        fprintf(output, "    ldr x0, [sp], 8\n");
        fprintf(output, "    bl malloc\n");
        fprintf(output, "    str x0, [sp, -8]!\n");
    }
}

void generateARMFreeCall(ASTNode *node) {
    if (!node) return;
    if (node->params) {
        generateARMExpression(node->params);
        fprintf(output, "    ldr x0, [sp], 8\n");
        fprintf(output, "    bl free\n");
    }
}

void generateARMObjectDeclaration(ASTNode *node) {
    if (!node) return;
}

void generateARMObjectMember(ASTNode *node) {
    if (!node) return;
}

void generateARMObjectAccess(ASTNode *node) {
    if (!node || !node->left) return;
    
    if (node->left->type == AST_IDENTIFIER && node->right && node->right->type == AST_IDENTIFIER) {
        char *objName = node->left->token.value;
        char *memberName = node->right->token.value;
        
        if (isObjectName(objName)) {
            const char *typeName = NULL;
            int found = 0;
            for (int i = 0; i < objectCount && !found; i++) {
                if (strcmp(objectTable[i].name, objName) == 0) {
                    ASTNode *member = objectTable[i].node->body;
                    while (member) {
                        if (member->type == AST_OBJECT_MEMBER && strcmp(member->token.value, memberName) == 0) {
                            typeName = member->namespace;
                            found = 1;
                            break;
                        }
                        member = member->next;
                    }
                }
            }
            
            if (found) {
                if (strcmp(typeName, "string") == 0) {
                    fprintf(output, "    adrp x0, __obj_%s_%s\n", objName, memberName);
                    fprintf(output, "    add x0, x0, :lo12:__obj_%s_%s\n", objName, memberName);
                    fprintf(output, "    str x0, [sp, #-16]!\n");
                } else if (strcmp(typeName, "char") == 0) {
                    fprintf(output, "    adrp x0, __obj_%s_%s\n", objName, memberName);
                    fprintf(output, "    add x0, x0, :lo12:__obj_%s_%s\n", objName, memberName);
                    fprintf(output, "    ldrb w0, [x0]\n");
                    fprintf(output, "    str x0, [sp, #-16]!\n");
                } else {
                    fprintf(output, "    adrp x0, __obj_%s_%s\n", objName, memberName);
                    fprintf(output, "    add x0, x0, :lo12:__obj_%s_%s\n", objName, memberName);
                    fprintf(output, "    ldr x0, [x0]\n");
                    fprintf(output, "    str x0, [sp, #-16]!\n");
                }
            }
        } else {
            generateARMCode(node->left);
            fprintf(output, "    ldr x0, [sp], #8\n");
        }
    } else {
        generateARMCode(node->left);
        fprintf(output, "    ldr x0, [sp], #8\n");
    }
}