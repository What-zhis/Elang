#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "codegen_nasm.h"
#include "pe_generator.h"

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

#define DEFINE_INSTR(m, enc, b, r, e) {#m, enc, b, r, e}

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

typedef struct {
    int offset;
    int type;
} Relocation;

Relocation relocations[100];
int relocationCount = 0;

void cb_init() {
    codeBuffer.data = NULL;
    codeBuffer.size = 0;
    codeBuffer.capacity = 0;
    relocationCount = 0;
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
            relocations[relocationCount].offset = codeBuffer.size;
            relocations[relocationCount].type = 1;
            relocationCount++;
            cb_emit_u32((unsigned long)imm);
            break;
            
        case ENCODING_REG_IMPLICIT:
            cb_emit_u8(def->base | (dest & 0x07));
            break;
            
        case ENCODING_REG_MEM_RIP:
            cb_emit_u8(def->base);
            cb_emit_u8(modrm_build(0, reg_code(dest), 0x05));
            relocations[relocationCount].offset = codeBuffer.size;
            relocations[relocationCount].type = 0;
            relocationCount++;
            cb_emit_u32((unsigned long)imm);
            break;
            
        case ENCODING_MEM_REG_RIP:
            cb_emit_u8(def->base);
            cb_emit_u8(modrm_build(0, reg_code(src), 0x05));
            relocations[relocationCount].offset = codeBuffer.size;
            relocations[relocationCount].type = 0;
            relocationCount++;
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
    if (!stmt || !output) return;

    switch (stmt->type) {
        case AST_OUTPUT: {
            if (stmt->left && stmt->left->type == AST_LITERAL && stmt->left->token.type == TOKEN_STRING) {
                int strIndex = -1;
                char *value = stmt->left->token.value;
                for (int i = 0; i < irStringCount; i++) {
                    if (strcmp(irStrings[i].value, value) == 0) {
                        strIndex = i;
                        break;
                    }
                }
                if (strIndex != -1) {
                    fprintf(output, "    print STR%d\n", strIndex);
                }
            }
            break;
        }
        case AST_RETURN: {
            fprintf(output, "    ret\n");
            break;
        }
        case AST_BINARY_OP: {
            fprintf(output, "    ; Binary operation\n");
            if (stmt->token.type == TOKEN_PLUS) {
                fprintf(output, "    add\n");
            } else if (stmt->token.type == TOKEN_MINUS) {
                fprintf(output, "    sub\n");
            } else if (stmt->token.type == TOKEN_MUL) {
                fprintf(output, "    mul\n");
            } else if (stmt->token.type == TOKEN_DIV) {
                fprintf(output, "    div\n");
            }
            break;
        }
        case AST_VARIABLE_DECLARATION: {
            fprintf(output, "    ; Variable %s\n", stmt->token.value);
            break;
        }
        case AST_ASSIGNMENT: {
            fprintf(output, "    ; Assignment to %s\n", stmt->token.value);
            break;
        }
        default:
            fprintf(output, "    ; Unsupported statement type %d\n", stmt->type);
            break;
    }
}

void generateMachineCode(ASTNode *node) {
    if (!node) return;

    pe_init();
    
    irStringCount = 0;
    irCollectStrings(node);
    
    cb_init();
    
    instr_encode("mov_imm32", REG_RCX, 0, -11);
    instr_encode("call_rel", 0, 0, 0);
    instr_encode("mov_reg", REG_RBX, REG_RAX, 0);
    
    if (irStringCount > 0) {
        for (int i = 0; i < irStringCount; i++) {
            instr_encode("mov_reg", REG_RCX, REG_RBX, 0);
            instr_encode("mov_rip", REG_RDX, 0, 0);
            instr_encode("mov_imm32", REG_R8, 0, (unsigned long)strlen(irStrings[i].value));
            instr_encode("sub_rsp", 0, 0, 40);
            instr_encode("call_rel", 0, 0, 0);
            instr_encode("add_rsp", 0, 0, 40);
        }
    }
    
    instr_encode("mov_imm32", REG_RCX, 0, 0);
    instr_encode("call_rel", 0, 0, 0);
    
    pe_set_text_section(codeBuffer.data, codeBuffer.size);
    
    unsigned char dataSec[4096];
    int dataOffset = 0;
    for (int i = 0; i < irStringCount; i++) {
        irStrings[i].rva = 0x2000 + dataOffset;
        int len = strlen(irStrings[i].value);
        memcpy(&dataSec[dataOffset], irStrings[i].value, len + 1);
        dataOffset += len + 1;
    }
    pe_set_data_section(dataSec, dataOffset);
    
    pe_add_import("kernel32.dll", "GetStdHandle");
    pe_add_import("kernel32.dll", "WriteConsoleA");
    pe_add_import("kernel32.dll", "ExitProcess");
    
    unsigned int textBase = 0x1000;
    unsigned int dataBase = 0x2000;
    
    for (int i = 0; i < relocationCount; i++) {
        if (relocations[i].type == 0) {
            unsigned int offset = relocations[i].offset;
            unsigned int nextInstrOffset = offset + 4;
            unsigned int strIdx = -1;
            unsigned int targetRVA = 0;
            
            for (int j = 0; j < irStringCount; j++) {
                if (j == 0 || strIdx == -1) {
                    strIdx = j;
                    targetRVA = irStrings[j].rva;
                }
            }
            
            if (strIdx != -1) {
                int relOffset = (int)(targetRVA - (textBase + nextInstrOffset));
                memcpy(&codeBuffer.data[offset], &relOffset, 4);
            }
        }
    }
    
    pe_write_file("output.exe");
    
    if (codeBuffer.data) {
        free(codeBuffer.data);
        codeBuffer.data = NULL;
    }
    
    for (int i = 0; i < irStringCount; i++) {
        free(irStrings[i].label);
        free(irStrings[i].value);
    }
}
