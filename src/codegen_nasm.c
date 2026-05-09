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
void generateNASMOutput(ASTNode *node);
void generateNASMOutputRecursive(ASTNode *node);
void generateNASMVariableDeclaration(ASTNode *node);
void generateNASMAssignment(ASTNode *node);
void generateNASMAsm(ASTNode *node);

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

    if (targetPlatform == TARGET_ARM_LINUX || targetPlatform == TARGET_ARM_ANDROID) {
        generateARMProgram(node);
        return;
    }

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
    } else if (node->next) {
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

    if (node->body) {
        generateNASMCode(node->body);
    }

    fprintf(output, "    mov rsp, rbp\n");
    fprintf(output, "    pop rbp\n");
    if (targetPlatform == TARGET_WIN) {
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
    
    ASTNode *caseNode = node->body;
    while (caseNode) {
        if (caseNode->type == AST_CASE) {
            if (strcmp(caseNode->namespace, "default") == 0) {
                generateNASMCode(caseNode->body);
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
        // 文件输入 - 字面量路径（NASM后端不支持文件操作，输出注释）
        fprintf(output, "    ; File input not supported in NASM backend\n");
    } else if (node->namespaceExpr && node->namespaceExpr->type == AST_IDENTIFIER &&
               strcmp(node->namespaceExpr->namespace, "std") != 0) {
        // 文件输入 - 变量路径（NASM后端不支持文件操作，输出注释）
        fprintf(output, "    ; File input not supported in NASM backend\n");
    } else {
        // 标准输入
        if (node->left) {
            // 输出提示信息
            generateNASMOutputRecursive(node->left);
        }
        // 调用输入函数
        fprintf(output, "    call _read_str\n");
        // 将输入存储到变量
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
    fprintf(output, "; Object %s (NASM backend does not support objects natively)\n", node->namespace);
}

void generateNASMObjectMember(ASTNode *node) {
    if (!node) return;
    fprintf(output, "; Object member %s\n", node->token.value);
}

void generateNASMObjectAccess(ASTNode *node) {
    if (!node || !node->left) return;
    generateNASMCode(node->left);
    fprintf(output, "    pop rax\n");
    if (node->right && node->right->type == AST_IDENTIFIER) {
        fprintf(output, "    ; Access member %s\n", node->right->token.value);
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
    fprintf(output, "# AArch64 Assembly Code Generated by E Compiler\n");
    if (targetPlatform == TARGET_ARM_LINUX) {
        fprintf(output, "# Architecture: AArch64 Linux\n\n");
    } else {
        fprintf(output, "# Architecture: AArch64 Android\n\n");
    }

    fprintf(output, ".data\n");
    fprintf(output, "    __newline: .byte 10\n");
    fprintf(output, "    __str_buf: .space 256\n");
    fprintf(output, "    __str_Sum_of: .asciz \"Sum of \"\n");
    fprintf(output, "    __str_and: .asciz \" and \"\n");
    fprintf(output, "    __str_is: .asciz \" is \"\n");
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

    fprintf(output, "# Helper function: print string\n");
    fprintf(output, "_print_str:\n");
    fprintf(output, "    stp x29, x30, [sp, -64]!\n");
    fprintf(output, "    mov x29, sp\n");
    fprintf(output, "    mov x4, x0\n");
    fprintf(output, "    mov w5, #0\n");
    fprintf(output, "print_len:\n");
    fprintf(output, "    ldrb w6, [x4, w5, uxtw]\n");
    fprintf(output, "    cmp w6, #0\n");
    fprintf(output, "    b.eq print_len_done\n");
    fprintf(output, "    add w5, w5, #1\n");
    fprintf(output, "    b print_len\n");
    fprintf(output, "print_len_done:\n");
    fprintf(output, "    mov x0, #1\n");
    fprintf(output, "    mov x1, x4\n");
    fprintf(output, "    mov x2, x5\n");
    fprintf(output, "    mov x8, #64\n");
    fprintf(output, "    svc #0\n");
    fprintf(output, "    ldp x29, x30, [sp], 64\n");
    fprintf(output, "    ret\n\n");

    fprintf(output, "# Helper function: print newline\n");
    fprintf(output, "_print_nl:\n");
    fprintf(output, "    mov x0, #1\n");
    fprintf(output, "    adrp x1, __newline\n");
    fprintf(output, "    add x1, x1, :lo12:__newline\n");
    fprintf(output, "    mov x2, #1\n");
    fprintf(output, "    mov x8, #64\n");
    fprintf(output, "    svc #0\n");
    fprintf(output, "    ret\n\n");

    if (node->next) {
        generateARMCode(node->next);
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
            generateARMExpression(node->right);
            fprintf(output, "    str x0, [sp, -8]!\n");
            generateARMIdentifier(node->left);
            fprintf(output, "    ldr x1, [sp], 8\n");
            fprintf(output, "    str x1, [x0]\n");
            break;
        case AST_FUNCTION_DECLARATION:
            generateARMFunctionDeclaration(node);
            return;
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

void generateARMIdentifier(ASTNode *node) {
    if (!node) return;

    int offset = getVariableOffset(node->token.value);
    if (offset >= 0) {
        fprintf(output, "    add x0, x29, #-%d\n", offset);
    } else {
        fprintf(output, "    adrp x0, %s\n", node->token.value);
        fprintf(output, "    add x0, x0, :lo12:%s\n", node->token.value);
    }
}

void generateARMFunctionDeclaration(ASTNode *node) {
    if (!node) return;

    fprintf(output, "%s:\n", node->token.value);
    fprintf(output, "    stp x29, x30, [sp, -48]!\n");
    fprintf(output, "    mov x29, sp\n");

    if (node->body) {
        generateARMCode(node->body);
    }

    fprintf(output, "    ldp x29, x30, [sp], #48\n");
    fprintf(output, "    ret\n");
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
        fprintf(output, "    str x0, [sp, -8]!\n");
    } else {
        if (node->params) {
            ASTNode *param = node->params;
            while (param) {
                generateARMExpression(param);
                fprintf(output, "    str x0, [sp, -8]!\n");
                param = param->next;
            }
        }
        fprintf(output, "    bl %s\n", node->token.value);
        fprintf(output, "    str x0, [sp, -8]!\n");
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
        fprintf(output, "    ; File input not supported in ARM backend\n");
    } else if (node->namespaceExpr && node->namespaceExpr->type == AST_IDENTIFIER &&
               strcmp(node->namespaceExpr->namespace, "std") != 0) {
        fprintf(output, "    ; File input not supported in ARM backend\n");
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
    fprintf(output, "; Struct %s (ARM backend does not support structs natively)\n", node->namespace);
}

void generateARMEnumDeclaration(ASTNode *node) {
    if (!node) return;
    fprintf(output, "; Enum %s (ARM backend does not support enums natively)\n", node->namespace);
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
    fprintf(output, "; Object %s (ARM backend does not support objects natively)\n", node->namespace);
}

void generateARMObjectMember(ASTNode *node) {
    if (!node) return;
    fprintf(output, "; Object member %s\n", node->token.value);
}

void generateARMObjectAccess(ASTNode *node) {
    if (!node || !node->left) return;
    generateARMCode(node->left);
    fprintf(output, "    ldr x0, [sp], 8\n");
    if (node->right && node->right->type == AST_IDENTIFIER) {
        fprintf(output, "    ; Access member %s\n", node->right->token.value);
    }
}