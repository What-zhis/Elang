#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "codegen_nasm.h"
#include "preprocessor.h"
#include "codegen_ir.h"
#include "optimizer.h"
#include "errors.h"

extern FILE *output;
extern void freeAST(ASTNode *node);
extern SymbolTable *currentSymbolTable;
extern void freeSymbolTable(SymbolTable *table);
extern void initASTPool();
extern void cleanupASTPool();
extern void printASTPoolStats();

Preprocessor globalPP;

char *readFile(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = (char *)malloc(len + 1);
    fread(content, 1, len, f);
    content[len] = '\0';
    fclose(f);
    
    int writePos = 0;
    for (int i = 0; content[i] != '\0'; i++) {
        if (content[i] == '\r') {
            if (content[i+1] == '\n') {
                content[writePos++] = '\n';
                i++;
            } else {
                content[writePos++] = '\n';
            }
        } else {
            content[writePos++] = content[i];
        }
    }
    content[writePos] = '\0';
    
    return content;
}

char **imports = NULL;
int importCount = 0;

void addImport(char *filename) {
    imports = (char **)realloc(imports, (importCount + 1) * sizeof(char *));
    imports[importCount] = strdup(filename);
    importCount++;
}

void collectImports(const char *content) {
    const char *p = content;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') p++;

        if (strncmp(p, "import", 6) == 0) {
            p += 6;
            while (*p == ' ' || *p == '\t') p++;

            if (*p == '"') {
                p++;
                char *filename = (char *)malloc(256);
                int i = 0;
                while (*p && *p != '"' && i < 255) {
                    filename[i++] = *p++;
                }
                filename[i] = '\0';
                if (*p == '"') p++;

                addImport(filename);
                free(filename);
            }
        }

        while (*p && *p != '\n') p++;
        if (*p) p++;
    }
}

char *processImports(const char *mainFile) {
    char *mainContent = readFile(mainFile);
    if (!mainContent) return NULL;

    collectImports(mainContent);

    if (importCount == 0) {
        return mainContent;
    }

    size_t totalLen = strlen(mainContent) + 1;
    for (int i = 0; i < importCount; i++) {
        char *importContent = readFile(imports[i]);
        if (importContent) {
            totalLen += strlen(importContent) + 100;
            free(importContent);
        }
    }

    char *result = (char *)malloc(totalLen);
    result[0] = '\0';

    for (int i = 0; i < importCount; i++) {
        char *importContent = readFile(imports[i]);
        if (importContent) {
            strcat(result, "// ===== Import: ");
            strcat(result, imports[i]);
            strcat(result, " =====\n");
            strcat(result, importContent);
            strcat(result, "\n");
            free(importContent);
        } else {
            fprintf(stderr, "Warning: Could not open imported file %s\n", imports[i]);
        }
    }
    strcat(result, "// ===== Main file =====\n");
    strcat(result, mainContent);
    free(mainContent);
    return result;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "%sUsage: %s [--run|--compile|--debug|--ir|--machine|--pool-stats] [--arm] [--linux|--win|--windows|--android] [--opt|-O <level>] [--no-color] <input_file>%s\n", COLOR_CYAN, argv[0], COLOR_RESET);
        fprintf(stderr, "  --run: Compile and run, then delete temporary files (default)\n");
        fprintf(stderr, "  --compile: Compile file but don't run\n");
        fprintf(stderr, "  --debug: Only check for errors, don't run\n");
        fprintf(stderr, "  --ir: Generate IR (intermediate representation)\n");
        fprintf(stderr, "  --machine: Generate machine code directly\n");
        fprintf(stderr, "  --pool-stats: Show AST memory pool statistics\n");
        fprintf(stderr, "  --arm: Generate ARM code (requires --linux or --android)\n");
        fprintf(stderr, "  --linux: Generate code for Linux (x86_64 or ARM with --arm)\n");
        fprintf(stderr, "  --win: Generate code for Windows (x86_64 only)\n");
        fprintf(stderr, "  --windows: Same as --win\n");
        fprintf(stderr, "  --android: Generate code for Android (ARM only)\n");
        fprintf(stderr, "  --opt|-O <level>: Optimization level (0=none, 1=basic, 2=full)\n");
        fprintf(stderr, "  --no-color: Disable color output\n");
        return 1;
    }

    int runMode = 1;  // Default: compile and run
    int compileOnly = 0;
    int debugOnly = 0;
    int showPoolStats = 0;  // Show AST pool statistics
    OutputFormat outputFormat = OUTPUT_FORMAT_NASM;  // Default: NASM
    TargetPlatform target = TARGET_WIN;  // Default: Windows x86_64
    int isARM = 0;
    char *inputFile = NULL;
    optLevel = OPT_LEVEL_BASIC;  // Default optimization level

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--run") == 0) {
            runMode = 1;
            compileOnly = 0;
            debugOnly = 0;
        } else if (strcmp(argv[i], "--compile") == 0) {
            runMode = 0;
            compileOnly = 1;
            debugOnly = 0;
        } else if (strcmp(argv[i], "--debug") == 0) {
            runMode = 0;
            compileOnly = 0;
            debugOnly = 1;
        } else if (strcmp(argv[i], "--pool-stats") == 0) {
            showPoolStats = 1;
        } else if (strcmp(argv[i], "--ir") == 0) {
            outputFormat = OUTPUT_FORMAT_IR;
            runMode = 0;
            compileOnly = 1;
        } else if (strcmp(argv[i], "--machine") == 0) {
            outputFormat = OUTPUT_FORMAT_MACHINE;
            runMode = 0;
            compileOnly = 1;
        } else if (strcmp(argv[i], "--arm") == 0) {
            isARM = 1;
        } else if (strcmp(argv[i], "--linux") == 0) {
            if (isARM) {
                target = TARGET_ARM_LINUX;
            } else {
                target = TARGET_LINUX;
            }
        } else if (strcmp(argv[i], "--win") == 0 || strcmp(argv[i], "--windows") == 0) {
            if (isARM) {
                fprintf(stderr, "Error: ARM is not supported for Windows\n");
                return 1;
            }
            target = TARGET_WIN;
        } else if (strcmp(argv[i], "--android") == 0) {
            if (!isARM) {
                fprintf(stderr, "Error: Android requires --arm flag\n");
                return 1;
            }
            target = TARGET_ARM_ANDROID;
        } else if (strcmp(argv[i], "--opt") == 0 || strcmp(argv[i], "-O") == 0) {
            if (i + 1 < argc) {
                int level = atoi(argv[i+1]);
                if (level >= 0 && level <= 2) {
                    optLevel = level;
                } else {
                    fprintf(stderr, "%sWarning: Invalid optimization level %d, using default (1)%s\n", 
                            COLOR_YELLOW, level, COLOR_RESET);
                }
                i++;
            } else {
                fprintf(stderr, "%sWarning: Missing optimization level, using default (1)%s\n", 
                        COLOR_YELLOW, COLOR_RESET);
            }
        } else if (strcmp(argv[i], "--no-color") == 0) {
            useColorOutput = 0;
        } else {
            inputFile = argv[i];
        }
    }
    
    if (isARM && target == TARGET_WIN) {
        fprintf(stderr, "Error: ARM requires --linux or --android flag\n");
        return 1;
    }

    if (!inputFile) {
        fprintf(stderr, "Error: No input file specified\n");
        return 1;
    }

    initPreprocessor(&globalPP);
    initASTPool();

    char *programContent = processImports(inputFile);
    if (!programContent) {
        fprintf(stderr, "Error: Could not open input file %s\n", inputFile);
        return 1;
    }

    char preprocessedContent[65536];
    strncpy(preprocessedContent, preprocess(&globalPP, inputFile, programContent), 65535);
    preprocessedContent[65535] = '\0';

    fprintf(stderr, "[DEBUG] Preprocessed content:\n%s\n", preprocessedContent);

    // Preprocessed content is ready

    FILE *tempFile = fopen("_temp_processed.e", "wb");
    if (!tempFile) {
        reportError(ERR_CODE_MEMORY_ERROR, inputFile, 0, 0, "Could not create temp file");
        return 1;
    }
    fwrite(preprocessedContent, 1, strlen(preprocessedContent), tempFile);
    fclose(tempFile);

    input = fopen("_temp_processed.e", "rb");
    if (!input) {
        reportError(ERR_CODE_MEMORY_ERROR, inputFile, 0, 0, "Could not open processed file");
        return 1;
    }

    initLexer(input);
    setLexerPreprocessor(&globalPP);
    currentToken = getNextToken();
    ASTNode *program = parseProgram();
    if (!program || errorCount > 0) {
        fprintf(stderr, "%sError: Failed to parse program. Found %d error(s).%s\n", 
                COLOR_RED, errorCount, COLOR_RESET);
        fclose(input);
        remove("_temp_processed.e");
        free(programContent);
        for (int i = 0; i < importCount; i++) {
            free(imports[i]);
        }
        free(imports);
        cleanupASTPool();
        return 1;
    }

    // Optimize AST
    if (optLevel > OPT_LEVEL_NONE && !debugOnly) {
        fprintf(stderr, "%sOptimizing AST (level %d)...%s\n", COLOR_BLUE, optLevel, COLOR_RESET);
        program = optimizeAST(program);
    }

    if (debugOnly) {
        fclose(input);
        remove("_temp_processed.e");
        freeSymbolTable(currentSymbolTable);
        currentSymbolTable = NULL;
        freeAST(program);
        free(programContent);
        for (int i = 0; i < importCount; i++) {
            free(imports[i]);
        }
        free(imports);
        if (showPoolStats) {
            printASTPoolStats();
        }
        cleanupASTPool();
        return 0;
    }

    char outputFilename[100];
    char compileCommand[200];

    if (outputFormat == OUTPUT_FORMAT_NASM) {
        sprintf(outputFilename, "output.asm");
    } else if (outputFormat == OUTPUT_FORMAT_IR) {
        sprintf(outputFilename, "output.ir");
    } else if (outputFormat == OUTPUT_FORMAT_MACHINE) {
        sprintf(outputFilename, "output.ir");
    } else {
        sprintf(outputFilename, "output.c");
    }

    FILE *outputFile = fopen(outputFilename, "w");
    if (!outputFile) {
        fprintf(stderr, "Error: Could not create output file\n");
        fclose(input);
        remove("_temp_processed.e");
        return 1;
    }
    output = outputFile;

    if (outputFormat == OUTPUT_FORMAT_NASM) {
        extern void generateNASMCode(ASTNode *node);
        extern TargetPlatform targetPlatform;
        targetPlatform = target;
        generateNASMCode(program);
    } else if (outputFormat == OUTPUT_FORMAT_IR) {
        extern void generateIR(ASTNode *node);
        extern TargetPlatform targetPlatform;
        targetPlatform = target;
        generateIR(program);
    } else if (outputFormat == OUTPUT_FORMAT_MACHINE) {
        extern void generateMachineCode(ASTNode *node);
        extern TargetPlatform targetPlatform;
        targetPlatform = target;
        generateMachineCode(program);
    }
    fclose(outputFile);

    fclose(input);
    remove("_temp_processed.e");

    // NASM模式仅生成汇编文件，用户自行编译
    if (outputFormat == OUTPUT_FORMAT_NASM) {
        fprintf(stderr, "NASM assembly generated: %s\n", outputFilename);
        fprintf(stderr, "To compile for Windows: nasm -f win64 -o output.obj %s && ld -o output.exe output.obj -lkernel32 -e _main\n", outputFilename);
        fprintf(stderr, "To compile for Linux: nasm -f elf64 -o output.o %s && gcc -o output output.o\n", outputFilename);
        return 0;
    }
    
    // IR和MACHINE模式直接返回，不需要编译
    if (outputFormat == OUTPUT_FORMAT_IR || outputFormat == OUTPUT_FORMAT_MACHINE) {
        fprintf(stderr, "done\n");
        return 0;
    }

    freeSymbolTable(currentSymbolTable);
    currentSymbolTable = NULL;

    freeAST(program);
    free(programContent);
    // preprocessedContent is stack-allocated, no need to free
    // freePreprocessor(&globalPP);  // Causes crash

    for (int i = 0; i < importCount; i++) {
        free(imports[i]);
    }
    free(imports);

    if (showPoolStats) {
        printASTPoolStats();
    }
    
    cleanupASTPool();

    return 0;
}