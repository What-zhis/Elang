#include <stdio.h>
#include <stdlib.h>

int main() {
    FILE *f = fopen("test_struct_obj.e", "r");
    if (!f) {
        printf("Cannot open file\n");
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *content = (char *)malloc(len + 1);
    fread(content, 1, len, f);
    content[len] = '\0';
    fclose(f);
    
    printf("File length: %ld\n", len);
    printf("Content:\n");
    for (int i = 0; i < len; i++) {
        printf("%c", content[i]);
    }
    printf("\n");
    
    free(content);
    return 0;
}