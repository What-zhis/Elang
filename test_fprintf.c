#include <stdio.h>
#include <string.h>

int main() {
    char test[] = "struct Point {\n    int x;\n};\n";
    
    fprintf(stderr, "[DEBUG] Preprocessed content:\n%s\n", test);
    
    return 0;
}