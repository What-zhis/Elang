#include <stdio.h>
#include <stdlib.h>
#include "lexer.h"

int main() {
    FILE *f = fopen("test_simple_func.e", "rb");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file\n");
        return 1;
    }
    
    // Write to temp file with same logic as main.c
    FILE *tempFile = fopen("_temp_debug.e", "wb");
    if (!tempFile) {
        fprintf(stderr, "Error: Cannot create temp file\n");
        fclose(f);
        return 1;
    }
    
    char buffer[8192];
    size_t bytesRead = fread(buffer, 1, sizeof(buffer) - 1, f);
    fwrite(buffer, 1, bytesRead, tempFile);
    fclose(tempFile);
    fclose(f);
    
    // Now test the lexer on the temp file
    FILE *input = fopen("_temp_debug.e", "rb");
    if (!input) {
        fprintf(stderr, "Error: Cannot open temp file\n");
        return 1;
    }
    
    initLexer(input);
    Token token;
    fprintf(stdout, "Tokens found:\n");
    fprintf(stdout, "----------------\n");
    
    do {
        token = getNextToken();
        fprintf(stdout, "Type: %d, Value: '%s', Line: %d, Column: %d\n", 
                token.type, token.value, token.line, token.column);
    } while (token.type != TOKEN_EOF);
    
    fclose(input);
    return 0;
}
