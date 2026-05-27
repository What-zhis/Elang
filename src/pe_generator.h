#ifndef PE_GENERATOR_H
#define PE_GENERATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    unsigned int virtualSize;
    unsigned int virtualAddress;
    unsigned int rawSize;
    unsigned int rawOffset;
    unsigned int characteristics;
    char name[8];
} PESection;

typedef struct {
    unsigned int rva;
    unsigned int size;
} ImportDescriptor;

void pe_init();
void pe_set_text_section(unsigned char *data, unsigned int size);
void pe_set_data_section(unsigned char *data, unsigned int size);
void pe_add_import(const char *dllName, const char *funcName);
void pe_add_string(const char *str);
int pe_write_file(const char *filename);

extern unsigned char *textSection;
extern unsigned int textSectionSize;
extern unsigned char *dataSection;
extern unsigned int dataSectionSize;

#endif
