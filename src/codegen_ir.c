#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "codegen_nasm.h"

extern FILE *output;
extern SymbolTable *currentSymbolTable;
extern TargetPlatform targetPlatform;

typedef struct {
    char *label;
    char *value;
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
    ENCODING_REG_IMPLICIT
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
    DEFINE_INSTR(ret,        ENCODING_IMPLICIT, 0xC3, 0x00, 0x00),
    DEFINE_INSTR(mov_imm32,  ENCODING_REG_IMM32, 0xB8, 0x00, 0x00),
    DEFINE_INSTR(mov_imm64,  ENCODING_REG_IMM64, 0xB8, 0x48, 0x00),
    DEFINE_INSTR(mov_reg,    ENCODING_REG_REG, 0x89, 0x00, 0x00),
    DEFINE_INSTR(mov_reg64,  ENCODING_REG_REG, 0x89, 0x48, 0x00),
    DEFINE_INSTR(xor_reg,    ENCODING_REG_REG, 0x31, 0x00, 0x00),
    DEFINE_INSTR(xor_reg64,  ENCODING_REG_REG, 0x31, 0x48, 0x00),
    DEFINE_INSTR(add_rsp,    ENCODING_RSP_IMM8, 0x83, 0x48, 0xC4),
    DEFINE_INSTR(sub_rsp,    ENCODING_RSP_IMM8, 0x83, 0x48, 0xEC),
    DEFINE_INSTR(call_rel,   ENCODING_CALL_REL, 0xE8, 0x00, 0x00),
    DEFINE_INSTR(push,       ENCODING_REG_IMPLICIT, 0x50, 0x00, 0x00),
    DEFINE_INSTR(pop,        ENCODING_REG_IMPLICIT, 0x58, 0x00, 0x00),
    DEFINE_INSTR(add_reg,    ENCODING_REG_REG, 0x01, 0x00, 0x00),
    DEFINE_INSTR(add_reg64,  ENCODING_REG_REG, 0x01, 0x48, 0x00),
    DEFINE_INSTR(sub_reg,    ENCODING_REG_REG, 0x29, 0x00, 0x00),
    DEFINE_INSTR(sub_reg64,  ENCODING_REG_REG, 0x29, 0x48, 0x00),
    DEFINE_INSTR(mul_reg,    ENCODING_REG_REG, 0x0F, 0x48, 0xAF),
    DEFINE_INSTR(div_reg,    ENCODING_REG_REG, 0x0F, 0x48, 0xF7),
    DEFINE_INSTR(cmp_reg,    ENCODING_REG_REG, 0x39, 0x00, 0x00),
    DEFINE_INSTR(cmp_reg64,  ENCODING_REG_REG, 0x39, 0x48, 0x00),
    DEFINE_INSTR(jmp_rel,    ENCODING_CALL_REL, 0xE9, 0x00, 0x00),
    DEFINE_INSTR(je_rel,     ENCODING_CALL_REL, 0x0F, 0x00, 0x84),
    DEFINE_INSTR(jne_rel,    ENCODING_CALL_REL, 0x0F, 0x00, 0x85),
    DEFINE_INSTR(jl_rel,     ENCODING_CALL_REL, 0x0F, 0x00, 0x8C),
    DEFINE_INSTR(jg_rel,     ENCODING_CALL_REL, 0x0F, 0x00, 0x8F),
    DEFINE_INSTR(jle_rel,    ENCODING_CALL_REL, 0x0F, 0x00, 0x8E),
    DEFINE_INSTR(jge_rel,    ENCODING_CALL_REL, 0x0F, 0x00, 0x8D),
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
    
    if (def->encoding == ENCODING_REG_REG || def->encoding == ENCODING_REG_IMPLICIT) {
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
            cb_emit_u8(def->ext);
            cb_emit_u32((unsigned long)imm);
            break;
            
        case ENCODING_REG_IMPLICIT:
            cb_emit_u8(def->base | (dest & 0x07));
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

void generatePEFile() {
    FILE *minFile = fopen("minimal.exe", "rb");
    if (!minFile) return;
    
    fseek(minFile, 0, SEEK_END);
    long minSize = ftell(minFile);
    fseek(minFile, 0, SEEK_SET);
    
    unsigned char *peData = malloc(minSize);
    fread(peData, 1, minSize, minFile);
    fclose(minFile);
    
    memcpy(peData + 0x400, codeBuffer.data, codeBuffer.size);
    
    FILE *peFile = fopen("output.exe", "wb");
    if (!peFile) {
        free(peData);
        return;
    }
    
    fwrite(peData, 1, minSize, peFile);
    fclose(peFile);
    free(peData);
}

void generateMachineCode(ASTNode *node) {
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
    
    instr_encode("mov_imm32", REG_RCX, 0, 0);
    cb_emit_u8(0xE8);
    cb_emit_u8(0x02);
    cb_emit_u8(0x00);
    cb_emit_u8(0x00);
    cb_emit_u8(0x00);
    cb_emit_u8(0x66);
    cb_emit_u8(0x90);
    cb_emit_u8(0xFF);
    cb_emit_u8(0x25);
    cb_emit_u8(0x26);
    cb_emit_u8(0x20);
    cb_emit_u8(0x00);
    cb_emit_u8(0x00);
    
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