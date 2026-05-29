#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "codegen_nasm.h"

extern SymbolTable *currentSymbolTable;
extern TargetPlatform targetPlatform;

typedef struct {
    char *label;
    char *value;
    unsigned int rva;
} IRStringData;

IRStringData irStrings[100];
int irStringCount = 0;

typedef struct {
    unsigned char *data;
    int size;
    int capacity;
} CodeBuffer;

CodeBuffer codeBuffer;

typedef enum {
    REG_RAX, REG_RCX, REG_RDX, REG_RBX,
    REG_RSP, REG_RBP, REG_RSI, REG_RDI,
    REG_R8, REG_R9, REG_R10, REG_R11,
    REG_R12, REG_R13, REG_R14, REG_R15
} X64Reg;



typedef enum {
    ENCODING_IMPLICIT,
    ENCODING_REG_IMM32,
    ENCODING_REG_IMM64,
    ENCODING_REG_REG,
    ENCODING_RSP_IMM8,
    ENCODING_CALL_REL,
    ENCODING_REG_IMPLICIT,
    ENCODING_REG_MEM_RIP,
    ENCODING_MEM_REG_RIP,
    ENCODING_REG_MEM_DISP32,
    ENCODING_MEM_REG_DISP32,
    ENCODING_JMP_REL8,
    ENCODING_JCC_REL8,
    ENCODING_MOVSXD
} InstrEncoding;

typedef struct {
    const char *mnemonic;
    InstrEncoding encoding;
    unsigned char base;
    unsigned char rex;
    unsigned char ext;
} InstrDef;

#define DEFINE_INSTR(name, enc, base_op, rex_p, ext_op) \
    {#name, enc, base_op, rex_p, ext_op}

InstrDef instruction_database[] = {
    DEFINE_INSTR(ret,           ENCODING_IMPLICIT, 0xC3, 0x00, 0x00),
    DEFINE_INSTR(mov_imm32,     ENCODING_REG_IMM32, 0xB8, 0x00, 0x00),
    DEFINE_INSTR(mov_imm64,     ENCODING_REG_IMM64, 0xB8, 0x48, 0x00),
    DEFINE_INSTR(mov_reg,       ENCODING_REG_REG, 0x89, 0x00, 0x00),
    DEFINE_INSTR(mov_reg64,     ENCODING_REG_REG, 0x89, 0x48, 0x00),
    DEFINE_INSTR(mov_rip,       ENCODING_REG_MEM_RIP, 0x8B, 0x48, 0x05),
    DEFINE_INSTR(mov_rip64,     ENCODING_REG_MEM_RIP, 0x8B, 0x48, 0x05),
    DEFINE_INSTR(mov_to_rip,    ENCODING_MEM_REG_RIP, 0x89, 0x48, 0x05),
    DEFINE_INSTR(mov_to_rip64,  ENCODING_MEM_REG_RIP, 0x89, 0x48, 0x05),
    DEFINE_INSTR(mov_disp32,    ENCODING_REG_MEM_DISP32, 0x8B, 0x00, 0x00),
    DEFINE_INSTR(mov_disp32_64, ENCODING_REG_MEM_DISP32, 0x8B, 0x48, 0x00),
    DEFINE_INSTR(xor_reg,       ENCODING_REG_REG, 0x31, 0x00, 0x00),
    DEFINE_INSTR(xor_reg64,     ENCODING_REG_REG, 0x31, 0x48, 0x00),
    DEFINE_INSTR(add_rsp,       ENCODING_RSP_IMM8, 0x83, 0x48, 0xC4),
    DEFINE_INSTR(sub_rsp,       ENCODING_RSP_IMM8, 0x83, 0x48, 0xEC),
    DEFINE_INSTR(call_rel,      ENCODING_CALL_REL, 0xE8, 0x00, 0x00),
    DEFINE_INSTR(push,          ENCODING_REG_IMPLICIT, 0x50, 0x00, 0x00),
    DEFINE_INSTR(pop,           ENCODING_REG_IMPLICIT, 0x58, 0x00, 0x00),
    DEFINE_INSTR(add_reg,       ENCODING_REG_REG, 0x01, 0x00, 0x00),
    DEFINE_INSTR(add_reg64,     ENCODING_REG_REG, 0x01, 0x48, 0x00),
    DEFINE_INSTR(sub_reg,       ENCODING_REG_REG, 0x29, 0x00, 0x00),
    DEFINE_INSTR(sub_reg64,     ENCODING_REG_REG, 0x29, 0x48, 0x00),
    DEFINE_INSTR(mul_reg,       ENCODING_REG_REG, 0x0F, 0x48, 0xAF),
    DEFINE_INSTR(div_reg,       ENCODING_REG_REG, 0x0F, 0x48, 0xF7),
    DEFINE_INSTR(cmp_reg,       ENCODING_REG_REG, 0x39, 0x00, 0x00),
    DEFINE_INSTR(cmp_reg64,     ENCODING_REG_REG, 0x39, 0x48, 0x00),
    DEFINE_INSTR(jmp_rel,       ENCODING_CALL_REL, 0xE9, 0x00, 0x00),
    DEFINE_INSTR(jmp_rel8,      ENCODING_JMP_REL8, 0xEB, 0x00, 0x00),
    DEFINE_INSTR(je_rel,        ENCODING_CALL_REL, 0x0F, 0x00, 0x84),
    DEFINE_INSTR(je_rel8,       ENCODING_JCC_REL8, 0x74, 0x00, 0x00),
    DEFINE_INSTR(jne_rel,       ENCODING_CALL_REL, 0x0F, 0x00, 0x85),
    DEFINE_INSTR(jne_rel8,      ENCODING_JCC_REL8, 0x75, 0x00, 0x00),
    DEFINE_INSTR(jl_rel,        ENCODING_CALL_REL, 0x0F, 0x00, 0x8C),
    DEFINE_INSTR(jl_rel8,       ENCODING_JCC_REL8, 0x7C, 0x00, 0x00),
    DEFINE_INSTR(jg_rel,        ENCODING_CALL_REL, 0x0F, 0x00, 0x8F),
    DEFINE_INSTR(jg_rel8,       ENCODING_JCC_REL8, 0x7F, 0x00, 0x00),
    DEFINE_INSTR(jle_rel,       ENCODING_CALL_REL, 0x0F, 0x00, 0x8E),
    DEFINE_INSTR(jle_rel8,      ENCODING_JCC_REL8, 0x7E, 0x00, 0x00),
    DEFINE_INSTR(jge_rel,       ENCODING_CALL_REL, 0x0F, 0x00, 0x8D),
    DEFINE_INSTR(jge_rel8,      ENCODING_JCC_REL8, 0x7D, 0x00, 0x00),
    DEFINE_INSTR(movsx,         ENCODING_MOVSXD, 0x0F, 0x48, 0xBE),
    DEFINE_INSTR(and_reg,       ENCODING_REG_REG, 0x21, 0x00, 0x00),
    DEFINE_INSTR(and_reg64,     ENCODING_REG_REG, 0x21, 0x48, 0x00),
    DEFINE_INSTR(or_reg,        ENCODING_REG_REG, 0x09, 0x00, 0x00),
    DEFINE_INSTR(or_reg64,      ENCODING_REG_REG, 0x09, 0x48, 0x00),
    DEFINE_INSTR(neg_reg,       ENCODING_REG_IMPLICIT, 0xF7, 0x00, 0xD8),
    DEFINE_INSTR(neg_reg64,     ENCODING_REG_IMPLICIT, 0xF7, 0x48, 0xD8),
    DEFINE_INSTR(not_reg,       ENCODING_REG_IMPLICIT, 0xF7, 0x00, 0xD0),
    DEFINE_INSTR(not_reg64,     ENCODING_REG_IMPLICIT, 0xF7, 0x48, 0xD0),
    DEFINE_INSTR(shl_reg,       ENCODING_REG_REG, 0xD1, 0x00, 0xE0),
    DEFINE_INSTR(shl_reg64,     ENCODING_REG_REG, 0xD1, 0x48, 0xE0),
    DEFINE_INSTR(shr_reg,       ENCODING_REG_REG, 0xD1, 0x00, 0xE8),
    DEFINE_INSTR(shr_reg64,     ENCODING_REG_REG, 0xD1, 0x48, 0xE8),
    DEFINE_INSTR(sar_reg,       ENCODING_REG_REG, 0xD1, 0x00, 0xF8),
    DEFINE_INSTR(sar_reg64,     ENCODING_REG_REG, 0xD1, 0x48, 0xF8),
    DEFINE_INSTR(lea_reg,       ENCODING_REG_REG, 0x8D, 0x00, 0x00),
    DEFINE_INSTR(lea_reg64,     ENCODING_REG_REG, 0x8D, 0x48, 0x00),
    {NULL, 0, 0, 0, 0}
};

#undef DEFINE_INSTR

void cb_init() {
    codeBuffer.data = NULL;
    codeBuffer.size = 0;
    codeBuffer.capacity = 0;
}

void cb_grow(int needed) {
    if (codeBuffer.size + needed > codeBuffer.capacity) {
        codeBuffer.capacity = (codeBuffer.size + needed + 1023) & ~1023;
        codeBuffer.data = realloc(codeBuffer.data, codeBuffer.capacity);
    }
}

void cb_emit_u8(unsigned char value) {
    cb_grow(1);
    codeBuffer.data[codeBuffer.size++] = value;
}

void cb_emit_u32(unsigned long value) {
    cb_grow(4);
    for (int i = 0; i < 4; i++) {
        codeBuffer.data[codeBuffer.size++] = (value >> (8 * i)) & 0xFF;
    }
}

void cb_emit_u64(unsigned long long value) {
    cb_grow(8);
    for (int i = 0; i < 8; i++) {
        codeBuffer.data[codeBuffer.size++] = (value >> (8 * i)) & 0xFF;
    }
}

unsigned char modrm_build(unsigned char mod, unsigned char reg, unsigned char rm) {
    return (mod << 6) | (reg << 3) | rm;
}

unsigned char reg_code(X64Reg reg) {
    return reg < 8 ? reg : (reg - 8);
}

int reg_is_extended(X64Reg reg) {
    return reg >= 8;
}

InstrDef *instr_lookup(const char *mnemonic) {
    for (int i = 0; instruction_database[i].mnemonic != NULL; i++) {
        if (strcmp(instruction_database[i].mnemonic, mnemonic) == 0) {
            return &instruction_database[i];
        }
    }
    return NULL;
}

void instr_encode(const char *mnemonic, X64Reg dest, X64Reg src, unsigned long long imm) {
    InstrDef *def = instr_lookup(mnemonic);
    if (!def) return;

    unsigned char rex_byte = def->rex;
    
    if (def->encoding == ENCODING_REG_REG || def->encoding == ENCODING_REG_IMPLICIT ||
        def->encoding == ENCODING_REG_MEM_RIP || def->encoding == ENCODING_MEM_REG_RIP) {
        if (reg_is_extended(dest)) rex_byte |= 0x48;
        if (reg_is_extended(src)) rex_byte |= 0x01;
    }
    
    if (rex_byte != 0) {
        cb_emit_u8(rex_byte);
    }
    
    switch (def->encoding) {
        case ENCODING_IMPLICIT:
            cb_emit_u8(def->base);
            break;
            
        case ENCODING_REG_IMM32:
            cb_emit_u8(def->base | (dest & 0x07));
            cb_emit_u32((unsigned long)imm);
            break;
            
        case ENCODING_REG_IMM64:
            cb_emit_u8(def->base | (dest & 0x07));
            cb_emit_u64(imm);
            break;
            
        case ENCODING_REG_REG:
            cb_emit_u8(def->base);
            if (def->ext != 0) {
                cb_emit_u8(def->ext);
            }
            cb_emit_u8(modrm_build(3, reg_code(dest), reg_code(src)));
            break;
            
        case ENCODING_RSP_IMM8:
            cb_emit_u8(def->base);
            cb_emit_u8(def->ext);
            cb_emit_u8((unsigned char)imm);
            break;
            
        case ENCODING_CALL_REL:
            cb_emit_u8(def->base);
            if (def->ext != 0) {
                cb_emit_u8(def->ext);
            }
            cb_emit_u32((unsigned long)imm);
            break;
            
        case ENCODING_REG_IMPLICIT:
            cb_emit_u8(def->base | (dest & 0x07));
            break;
            
        case ENCODING_REG_MEM_RIP:
            cb_emit_u8(def->base | (dest & 0x07));
            cb_emit_u32((unsigned long)imm);
            break;
            
        case ENCODING_MEM_REG_RIP:
            cb_emit_u8(def->base | (dest & 0x07));
            cb_emit_u32((unsigned long)imm);
            break;
            
        case ENCODING_REG_MEM_DISP32:
            cb_emit_u8(def->base);
            cb_emit_u8(modrm_build(0, reg_code(dest), reg_code(src)));
            cb_emit_u32((unsigned long)imm);
            break;
            
        case ENCODING_MEM_REG_DISP32:
            cb_emit_u8(def->base);
            cb_emit_u8(modrm_build(0, reg_code(src), reg_code(dest)));
            cb_emit_u32((unsigned long)imm);
            break;
            
        case ENCODING_JMP_REL8:
            cb_emit_u8(def->base);
            cb_emit_u8((unsigned char)imm);
            break;
            
        case ENCODING_JCC_REL8:
            cb_emit_u8(def->base);
            cb_emit_u8((unsigned char)imm);
            break;
            
        case ENCODING_MOVSXD:
            cb_emit_u8(def->base);
            cb_emit_u8(def->ext);
            cb_emit_u8(modrm_build(3, reg_code(dest), reg_code(src)));
            break;
    }
}

int addIRString(const char *label, const char *value) {
    if (irStringCount >= 100) return -1;
    irStrings[irStringCount].label = strdup(label);
    irStrings[irStringCount].value = strdup(value);
    return irStringCount++;
}

void irCollectStrings(ASTNode *node) {
    if (!node) return;
    
    if (node->type == AST_FUNCTION_DECLARATION) {
        irCollectStrings(node->body);
    } else if (node->type == AST_STATEMENT) {
        irCollectStrings(node->left);
    } else if (node->type == AST_OUTPUT) {
        if (node->left && node->left->type == AST_LITERAL && node->left->token.type == TOKEN_STRING) {
            char label[32];
            sprintf(label, "STR%d", irStringCount);
            
            char escaped[1024];
            char *p = escaped;
            const char *s = node->left->token.value;
            while (*s) {
                if (*s == '\\') {
                    s++;
                    if (*s == 'n') *p++ = '\n';
                    else if (*s == 't') *p++ = '\t';
                    else if (*s == '\"') *p++ = '\"';
                    else if (*s == '\\') *p++ = '\\';
                    else { *p++ = '\\'; *p++ = *s; }
                } else {
                    *p++ = *s;
                }
                s++;
            }
            *p = '\0';
            
            addIRString(label, escaped);
        }
    }
    
    irCollectStrings(node->next);
}

void generateIRStatement(ASTNode *stmt);

void generateIR(ASTNode *node) {
    if (!node || !output) return;

    irStringCount = 0;
    irCollectStrings(node);

    fprintf(output, "; IR (Intermediate Representation)\n");
    fprintf(output, "; Target: %s\n",
            targetPlatform == TARGET_WIN ? "Windows x86-64" :
            targetPlatform == TARGET_LINUX ? "Linux x86-64" : "Unknown");
    fprintf(output, "; Architecture: x86-64\n\n");

    fprintf(output, "== STRINGS ==\n");
    for (int i = 0; i < irStringCount; i++) {
        fprintf(output, "%s: data \"%s\"\n", irStrings[i].label, irStrings[i].value);
    }
    
    fprintf(output, "\n== STRUCTS ==\n");
    ASTNode *structCurrent = node->next;
    while (structCurrent) {
        if (structCurrent->type == AST_STRUCT_DECLARATION) {
            fprintf(output, "; Struct %s (IR backend does not support structs natively)\n", structCurrent->namespace);
        } else if (structCurrent->type == AST_ENUM_DECLARATION) {
            fprintf(output, "; Enum %s (IR backend does not support enums natively)\n", structCurrent->namespace);
        } else if (structCurrent->type == AST_OBJECT_DECLARATION) {
            fprintf(output, "; Object %s (IR backend does not support objects natively)\n", structCurrent->namespace);
        }
        structCurrent = structCurrent->next;
    }
    
    fprintf(output, "\n== FUNCTIONS ==\n");

    ASTNode *current = node->next;
    while (current) {
        if (current->type == AST_FUNCTION_DECLARATION) {
            char *funcName = current->token.value;
            fprintf(output, "func %s() {\n", funcName);
            
            ASTNode *funcBody = current->body;
            if (funcBody) {
                ASTNode *body = funcBody->next;
                while (body) {
                    if (body->type == AST_STATEMENT) {
                        generateIRStatement(body->left);
                    }
                    body = body->next;
                }
            }
            
            fprintf(output, "    ret\n");
            fprintf(output, "}\n\n");
        }
        current = current->next;
    }

    fprintf(output, "\n== END ==\n");
}

void generateIRStatement(ASTNode *stmt) {
    if (!stmt) return;
    
    switch (stmt->type) {
        case AST_OUTPUT:
            {
                if (stmt->left && stmt->left->type == AST_LITERAL && stmt->left->token.type == TOKEN_STRING) {
                    for (int i = 0; i < irStringCount; i++) {
                        char original[1024];
                        char *p = original;
                        const char *s = stmt->left->token.value;
                        while (*s) {
                            if (*s == '\\') {
                                s++;
                                if (*s == 'n') *p++ = '\n';
                                else if (*s == 't') *p++ = '\t';
                                else if (*s == '\"') *p++ = '\"';
                                else if (*s == '\\') *p++ = '\\';
                                else { *p++ = '\\'; *p++ = *s; }
                            } else {
                                *p++ = *s;
                            }
                            s++;
                        }
                        *p = '\0';
                        if (strcmp(irStrings[i].value, original) == 0) {
                            fprintf(output, "    print %s\n", irStrings[i].label);
                            break;
                        }
                    }
                }
                break;
            }
        case AST_INPUT:
            {
                if (stmt->right && stmt->right->type == AST_IDENTIFIER) {
                    fprintf(output, "    read %s\n", stmt->right->token.value);
                } else {
                    fprintf(output, "    read tmp\n");
                }
                break;
            }
        case AST_FUNCTION_CALL:
            {
                fprintf(output, "    call %s", stmt->token.value);
                if (stmt->params) {
                    fprintf(output, "(");
                    ASTNode *arg = stmt->params;
                    while (arg) {
                        if (arg->type == AST_LITERAL && arg->token.type == TOKEN_STRING) {
                            for (int i = 0; i < irStringCount; i++) {
                                char original[1024];
                                char *p = original;
                                const char *s = arg->token.value;
                                while (*s) {
                                    if (*s == '\\') {
                                        s++;
                                        if (*s == 'n') *p++ = '\n';
                                        else if (*s == 't') *p++ = '\t';
                                        else if (*s == '\"') *p++ = '\"';
                                        else if (*s == '\\') *p++ = '\\';
                                        else { *p++ = '\\'; *p++ = *s; }
                                    } else {
                                        *p++ = *s;
                                    }
                                    s++;
                                }
                                *p = '\0';
                                if (strcmp(irStrings[i].value, original) == 0) {
                                    fprintf(output, "%s", irStrings[i].label);
                                    break;
                                }
                            }
                        } else if (arg->type == AST_IDENTIFIER) {
                            fprintf(output, "%s", arg->token.value);
                        } else if (arg->type == AST_LITERAL) {
                            fprintf(output, "%s", arg->token.value);
                        }
                        arg = arg->next;
                        if (arg) fprintf(output, ", ");
                    }
                    fprintf(output, ")");
                }
                fprintf(output, "\n");
                break;
            }
        case AST_ALLOC_CALL:
            {
                fprintf(output, "    alloc ");
                if (stmt->params) {
                    if (stmt->params->type == AST_LITERAL) {
                        fprintf(output, "%s", stmt->params->token.value);
                    }
                }
                fprintf(output, "\n");
                break;
            }
        case AST_FREE_CALL:
            {
                fprintf(output, "    free ");
                if (stmt->params && stmt->params->type == AST_IDENTIFIER) {
                    fprintf(output, "%s", stmt->params->token.value);
                }
                fprintf(output, "\n");
                break;
            }
        case AST_UNARY_OP:
            {
                fprintf(output, "    ");
                if (stmt->token.type == TOKEN_MINUS) fprintf(output, "neg ");
                else if (stmt->token.type == TOKEN_NOT) fprintf(output, "not ");
                else if (stmt->token.type == TOKEN_PLUS) fprintf(output, "pos ");
                if (stmt->left && stmt->left->type == AST_IDENTIFIER) {
                    fprintf(output, "%s\n", stmt->left->token.value);
                }
                break;
            }
        case AST_DEREFERENCE:
            {
                fprintf(output, "    deref ");
                if (stmt->left && stmt->left->type == AST_IDENTIFIER) {
                    fprintf(output, "%s\n", stmt->left->token.value);
                }
                break;
            }
        case AST_ADDRESS_OF:
            {
                fprintf(output, "    addr ");
                if (stmt->left && stmt->left->type == AST_IDENTIFIER) {
                    fprintf(output, "%s\n", stmt->left->token.value);
                }
                break;
            }
        case AST_MEMBER_ACCESS:
            {
                fprintf(output, "    member ");
                if (stmt->left && stmt->left->type == AST_IDENTIFIER) {
                    fprintf(output, "%s.", stmt->left->token.value);
                }
                if (stmt->right && stmt->right->type == AST_IDENTIFIER) {
                    fprintf(output, "%s\n", stmt->right->token.value);
                }
                break;
            }
        case AST_OBJECT_ACCESS:
            {
                fprintf(output, "    obj_access ");
                if (stmt->left && stmt->left->type == AST_IDENTIFIER) {
                    fprintf(output, "%s", stmt->left->token.value);
                }
                if (stmt->right && stmt->right->type == AST_IDENTIFIER) {
                    fprintf(output, ".%s\n", stmt->right->token.value);
                } else if (stmt->right) {
                    fprintf(output, "[");
                    if (stmt->right->type == AST_LITERAL && stmt->right->token.type == TOKEN_STRING) {
                        fprintf(output, "\"%s\"", stmt->right->token.value);
                    } else if (stmt->right->type == AST_IDENTIFIER) {
                        fprintf(output, "%s", stmt->right->token.value);
                    }
                    fprintf(output, "]\n");
                }
                break;
            }
        case AST_IDENTIFIER:
            {
                fprintf(output, "    ; identifier %s\n", stmt->token.value);
                break;
            }
        default:
            fprintf(output, "    ; unsupported stmt type %d\n", stmt->type);
            break;
    }
}

void ir_to_x64_print(unsigned long strIdx) {
    unsigned long long strAddr = 0x140002000ULL;
    for (int i = 0; i < strIdx; i++) {
        strAddr += strlen(irStrings[i].value) + 1;
    }
    
    instr_encode("mov_imm64", REG_RAX, 0, strAddr);
    instr_encode("mov_imm32", REG_RDX, 0, (unsigned long)strlen(irStrings[strIdx].value));
    instr_encode("sub_rsp", 0, 0, 0x08);
    instr_encode("mov_reg_r8", REG_R8, REG_R12, 0);
    instr_encode("call_rel", 0, 0, 0x00000016);
    instr_encode("add_rsp", 0, 0, 0x08);
}

void translateIRToX64(const char *irContent) {
    char *line = strdup(irContent);
    char *token;
    char *saveptr;
    
    token = strtok_r(line, "\n", &saveptr);
    while (token) {
        char *trimmed = token;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        
        if (strncmp(trimmed, "    ", 4) == 0) {
            char instr[32], arg1[32];
            int parsed = sscanf(trimmed + 4, "%31s %31s", instr, arg1);
            
            if (strcmp(instr, "print") == 0 && parsed >= 2) {
                unsigned long strIdx = strtoul(arg1 + 3, NULL, 10);
                ir_to_x64_print(strIdx);
            } else if (strcmp(instr, "ret") == 0) {
                instr_encode("ret", 0, 0, 0);
            }
        }
        
        token = strtok_r(NULL, "\n", &saveptr);
    }
    
    free(line);
}

typedef struct {
    unsigned short magic;
    unsigned short usedBytesInLastPage;
    unsigned short fileSizeInPages;
    unsigned short numRelocations;
    unsigned short headerSizeInParagraphs;
    unsigned short minExtraParagraphs;
    unsigned short maxExtraParagraphs;
    unsigned short ss;
    unsigned short sp;
    unsigned short checksum;
    unsigned short ip;
    unsigned short cs;
    unsigned short relocationTableOffset;
    unsigned short overlayNumber;
    unsigned short reserved[4];
    unsigned short oemid;
    unsigned short oeminfo;
    unsigned short reserved2[10];
    unsigned int lfanew;
} DOSHeader;

typedef struct {
    unsigned int signature;
    unsigned short machine;
    unsigned short numSections;
    unsigned int timeDateStamp;
    unsigned int pointerToSymbolTable;
    unsigned int numSymbols;
    unsigned short sizeOfOptionalHeader;
    unsigned short characteristics;
} COFFHeader;

typedef struct {
    unsigned short magic;
    unsigned char majorLinkerVersion;
    unsigned char minorLinkerVersion;
    unsigned int sizeOfCode;
    unsigned int sizeOfInitializedData;
    unsigned int sizeOfUninitializedData;
    unsigned int addressOfEntryPoint;
    unsigned int baseOfCode;
    unsigned long long imageBase;
    unsigned int sectionAlignment;
    unsigned int fileAlignment;
    unsigned short majorOperatingSystemVersion;
    unsigned short minorOperatingSystemVersion;
    unsigned short majorImageVersion;
    unsigned short minorImageVersion;
    unsigned short majorSubsystemVersion;
    unsigned short minorSubsystemVersion;
    unsigned int win32VersionValue;
    unsigned int sizeOfImage;
    unsigned int sizeOfHeaders;
    unsigned int checkSum;
    unsigned short subsystem;
    unsigned short dllCharacteristics;
    unsigned long long sizeOfStackReserve;
    unsigned long long sizeOfStackCommit;
    unsigned long long sizeOfHeapReserve;
    unsigned long long sizeOfHeapCommit;
    unsigned int loaderFlags;
    unsigned int numRvaAndSizes;
} PEOptionalHeader64;

typedef struct {
    char name[8];
    unsigned int virtualSize;
    unsigned int virtualAddress;
    unsigned int sizeOfRawData;
    unsigned int pointerToRawData;
    unsigned int pointerToRelocations;
    unsigned int pointerToLinenumbers;
    unsigned short numRelocations;
    unsigned short numLinenumbers;
    unsigned int characteristics;
} PESectionHeader;

void pe_write_byte(FILE *f, unsigned char b) {
    fwrite(&b, 1, 1, f);
}

void pe_write_word(FILE *f, unsigned short w) {
    fwrite(&w, 2, 1, f);
}

void pe_write_dword(FILE *f, unsigned long dw) {
    fwrite(&dw, 4, 1, f);
}

void pe_write_qword(FILE *f, unsigned long long qw) {
    fwrite(&qw, 8, 1, f);
}

void pe_pad_to_alignment(FILE *f, int alignment) {
    long pos = ftell(f);
    int pad = alignment - (pos % alignment);
    if (pad != alignment) {
        for (int i = 0; i < pad; i++) {
            pe_write_byte(f, 0);
        }
    }
}

typedef struct {
    unsigned int rva;
    unsigned int size;
} DataDirectoryEntry;

typedef struct {
    unsigned int characteristics;
    unsigned int timeDateStamp;
    unsigned short majorVersion;
    unsigned short minorVersion;
    unsigned int name;
    unsigned int base;
    unsigned int numberOfFunctions;
    unsigned int numberOfNames;
    unsigned int addressOfFunctions;
    unsigned int addressOfNames;
    unsigned int addressOfNameOrdinals;
} ImportDirectoryEntry;

typedef struct {
    char dllName[64];
    char funcName[64];
} PEImport;

PEImport peImports[20];
int peImportCount = 0;

void pe_add_import(const char *dllName, const char *funcName) {
    if (peImportCount < 20) {
        strncpy(peImports[peImportCount].dllName, dllName, 63);
        strncpy(peImports[peImportCount].funcName, funcName, 63);
        peImportCount++;
    }
}

void generatePEFile() {
    FILE *peFile = fopen("output.exe", "wb");
    if (!peFile) return;

    unsigned char header[1024];
    memset(header, 0, sizeof(header));

    DOSHeader *dosHeader = (DOSHeader *)header;
    dosHeader->magic = 0x5A4D;
    dosHeader->lfanew = 0x80;

    unsigned char *peSig = header + 0x80;
    peSig[0] = 'P';
    peSig[1] = 'E';
    peSig[2] = 0;
    peSig[3] = 0;

    COFFHeader *coffHeader = (COFFHeader *)(header + 0x84);
    coffHeader->signature = 0x00004550;
    coffHeader->machine = 0x8664;
    coffHeader->numSections = 3;
    coffHeader->sizeOfOptionalHeader = 0xF0;
    coffHeader->characteristics = 0x22;

    PEOptionalHeader64 *optHeader = (PEOptionalHeader64 *)(header + 0x98);
    optHeader->magic = 0x20B;
    optHeader->majorLinkerVersion = 14;
    optHeader->minorLinkerVersion = 0;
    optHeader->sizeOfCode = ((codeBuffer.size + 4095) & ~4095);
    optHeader->sizeOfInitializedData = 4096;
    optHeader->addressOfEntryPoint = 0x1000;
    optHeader->baseOfCode = 0x1000;
    optHeader->imageBase = 0x140000000ULL;
    optHeader->sectionAlignment = 0x1000;
    optHeader->fileAlignment = 0x200;
    optHeader->majorImageVersion = 6;
    optHeader->minorImageVersion = 0;
    optHeader->majorSubsystemVersion = 6;
    optHeader->minorSubsystemVersion = 0;
    optHeader->sizeOfImage = 0x4000;
    optHeader->sizeOfHeaders = 0x200;
    optHeader->subsystem = 3;
    optHeader->numRvaAndSizes = 16;

    unsigned int textVA = 0x1000;
    unsigned int dataVA = 0x2000;
    unsigned int idataVA = 0x3000;

    PESectionHeader *textSection = (PESectionHeader *)(header + 0x178);
    memcpy(textSection->name, ".text\0\0\0", 8);
    textSection->virtualSize = codeBuffer.size;
    textSection->virtualAddress = textVA;
    textSection->sizeOfRawData = ((codeBuffer.size + 511) & ~511);
    textSection->pointerToRawData = 0x200;
    textSection->characteristics = 0x60000020;

    PESectionHeader *dataSection = (PESectionHeader *)(header + 0x190);
    memcpy(dataSection->name, ".data\0\0\0", 8);
    dataSection->virtualSize = 4096;
    dataSection->virtualAddress = dataVA;
    dataSection->sizeOfRawData = 512;
    dataSection->pointerToRawData = 0x200 + ((codeBuffer.size + 511) & ~511);
    dataSection->characteristics = 0xC0000040;

    PESectionHeader *idataSection = (PESectionHeader *)(header + 0x1A8);
    memcpy(idataSection->name, ".idata\0\0", 8);
    idataSection->virtualSize = 1024;
    idataSection->virtualAddress = idataVA;
    idataSection->sizeOfRawData = 512;
    idataSection->pointerToRawData = 0x200 + ((codeBuffer.size + 511) & ~511) + 512;
    idataSection->characteristics = 0xC0000040;

    DataDirectoryEntry *importDir = (DataDirectoryEntry *)&optHeader->loaderFlags;
    importDir[1].rva = idataVA;
    importDir[1].size = 1024;

    fwrite(header, 1, 0x200, peFile);

    int textPaddedSize = ((codeBuffer.size + 511) & ~511);
    fwrite(codeBuffer.data, 1, codeBuffer.size, peFile);
    for (int i = codeBuffer.size; i < textPaddedSize; i++) {
        pe_write_byte(peFile, 0x90);
    }

    unsigned char dataBuf[512] = {0};
    int strOffset = 0;
    for (int i = 0; i < irStringCount; i++) {
        strcpy((char *)(dataBuf + strOffset), irStrings[i].value);
        irStrings[i].rva = dataVA + strOffset;
        strOffset += strlen(irStrings[i].value) + 1;
    }
    fwrite(dataBuf, 1, 512, peFile);

    unsigned char idataBuf[512] = {0};
    ImportDirectoryEntry *ide = (ImportDirectoryEntry *)idataBuf;
    int offset = 0;

    for (int i = 0; i < peImportCount; i++) {
        ide[i].characteristics = 0;
        ide[i].timeDateStamp = 0;
        ide[i].majorVersion = 0;
        ide[i].minorVersion = 0;
        ide[i].name = idataVA + 20 * peImportCount + offset + 20;
        ide[i].base = idataVA + 20 * peImportCount + 40 + i * 8;
        ide[i].numberOfFunctions = 1;
        ide[i].numberOfNames = 1;
        ide[i].addressOfFunctions = ide[i].base;
        ide[i].addressOfNames = idataVA + 20 * peImportCount + 40 + peImportCount * 8;
        ide[i].addressOfNameOrdinals = 0;

        char *namePtr = (char *)(idataBuf + 20 * peImportCount + i * 80 + 20);
        strcpy(namePtr, peImports[i].dllName);

        unsigned int *thunk = (unsigned int *)(idataBuf + 20 * peImportCount + 40 + i * 8);
        *thunk = idataVA + 20 * peImportCount + 40 + peImportCount * 8 + i * 24;

        unsigned int *nameRVA = (unsigned int *)(idataBuf + 20 * peImportCount + 40 + peImportCount * 8 + i * 24);
        *nameRVA = idataVA + 20 * peImportCount + 40 + peImportCount * 8 + peImportCount * 24 + i * 32;

        unsigned short *ordinal = (unsigned short *)(idataBuf + 20 * peImportCount + 40 + peImportCount * 8 + i * 24 + 4);
        *ordinal = 0;

        char *funcName = (char *)(idataBuf + 20 * peImportCount + 40 + peImportCount * 8 + peImportCount * 24 + i * 32);
        strcpy(funcName, peImports[i].funcName);
    }

    fwrite(idataBuf, 1, 512, peFile);

    fclose(peFile);
}

void generateMachineCode(ASTNode *node) {
    peImportCount = 0;
    pe_add_import("kernel32.dll", "WriteConsoleA");
    pe_add_import("kernel32.dll", "GetStdHandle");
    pe_add_import("kernel32.dll", "ExitProcess");
    
    generateIR(node);
    
    FILE *irFile = fopen("output.ir", "r");
    if (!irFile) return;
    
    fseek(irFile, 0, SEEK_END);
    long irSize = ftell(irFile);
    fseek(irFile, 0, SEEK_SET);
    
    char *irContent = malloc(irSize + 1);
    fread(irContent, 1, irSize, irFile);
    irContent[irSize] = '\0';
    fclose(irFile);
    
    cb_init();
    
    instr_encode("call_rel", 0, 0, 16);
    
    instr_encode("sub_rsp", 0, 0, 40);
    instr_encode("mov_imm32", REG_RCX, 0, -11);
    cb_emit_u8(0xE8);
    cb_emit_u8(0x00);
    cb_emit_u8(0x00);
    cb_emit_u8(0x00);
    cb_emit_u8(0x00);
    instr_encode("mov_reg", REG_RBX, REG_RAX, 0);
    
    char *line = strdup(irContent);
    char *token;
    char *saveptr;
    token = strtok_r(line, "\n", &saveptr);
    
    while (token) {
        char *trimmed = token;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        
        if (strncmp(trimmed, "    ", 4) == 0) {
            char instr[32], arg1[32], arg2[32];
            int parsed = sscanf(trimmed + 4, "%31s %31s %31s", instr, arg1, arg2);
            
            if (strcmp(instr, "print") == 0 && parsed >= 2) {
                unsigned long strIdx = strtoul(arg1 + 3, NULL, 10);
                
                instr_encode("mov_reg", REG_RCX, REG_RBX, 0);
                instr_encode("mov_rip", REG_RDX, 0, 0);
                instr_encode("mov_imm32", REG_R8, 0, (unsigned long)strlen(irStrings[strIdx].value));
                instr_encode("sub_rsp", 0, 0, 40);
                cb_emit_u8(0xE8);
                cb_emit_u8(0x00);
                cb_emit_u8(0x00);
                cb_emit_u8(0x00);
                cb_emit_u8(0x00);
                instr_encode("add_rsp", 0, 0, 40);
            } else if (strcmp(instr, "ret") == 0) {
                instr_encode("mov_imm32", REG_RCX, 0, 0);
                cb_emit_u8(0xE8);
                cb_emit_u8(0x00);
                cb_emit_u8(0x00);
                cb_emit_u8(0x00);
                cb_emit_u8(0x00);
            } else if (strcmp(instr, "add") == 0 && parsed >= 3) {
                instr_encode("add_reg", REG_RAX, REG_RCX, 0);
            } else if (strcmp(instr, "sub") == 0 && parsed >= 3) {
                instr_encode("sub_reg", REG_RAX, REG_RCX, 0);
            } else if (strcmp(instr, "mul") == 0 && parsed >= 2) {
                instr_encode("mul_reg", REG_RAX, REG_RCX, 0);
            } else if (strcmp(instr, "div") == 0 && parsed >= 2) {
                instr_encode("div_reg", REG_RAX, REG_RCX, 0);
            } else if (strcmp(instr, "mov") == 0 && parsed >= 3) {
                instr_encode("mov_reg", REG_RAX, REG_RCX, 0);
            }
        }
        
        token = strtok_r(NULL, "\n", &saveptr);
    }
    
    free(line);
    
    generatePEFile();
    
    free(irContent);
    if (codeBuffer.data) {
        free(codeBuffer.data);
        codeBuffer.data = NULL;
    }
    
    for (int i = 0; i < irStringCount; i++) {
        free(irStrings[i].label);
        free(irStrings[i].value);
    }
}