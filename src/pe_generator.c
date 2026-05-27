#include "pe_generator.h"

#define PE_MAGIC 0x4550

#pragma pack(push, 1)

typedef struct {
    unsigned short e_magic;
    unsigned short e_cblp;
    unsigned short e_cp;
    unsigned short e_crlc;
    unsigned short e_cparhdr;
    unsigned short e_minalloc;
    unsigned short e_maxalloc;
    unsigned short e_ss;
    unsigned short e_sp;
    unsigned short e_csum;
    unsigned short e_ip;
    unsigned short e_cs;
    unsigned short e_lfarlc;
    unsigned short e_ovno;
    unsigned short e_res[4];
    unsigned short e_oemid;
    unsigned short e_oeminfo;
    unsigned short e_res2[10];
    unsigned int e_lfanew;
} DOSHeader;

typedef struct {
    unsigned int machine;
    unsigned short numberOfSections;
    unsigned int timeDateStamp;
    unsigned int pointerToSymbolTable;
    unsigned int numberOfSymbols;
    unsigned short sizeOfOptionalHeader;
    unsigned short characteristics;
} PEFileHeader;

typedef struct {
    unsigned int rva;
    unsigned int size;
} DataDirectoryEntry;

typedef struct {
    unsigned short magic;
    unsigned char majorLinkerVersion;
    unsigned char minorLinkerVersion;
    unsigned int sizeOfCode;
    unsigned int sizeOfInitializedData;
    unsigned int sizeOfUninitializedData;
    unsigned int addressOfEntryPoint;
    unsigned int baseOfCode;
    unsigned int baseOfData;
    unsigned int imageBase;
    unsigned int sectionAlignment;
    unsigned int fileAlignment;
    unsigned short majorOSVersion;
    unsigned short minorOSVersion;
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
    unsigned int sizeOfStackReserve;
    unsigned int sizeOfStackCommit;
    unsigned int sizeOfHeapReserve;
    unsigned int sizeOfHeapCommit;
    unsigned int loaderFlags;
    unsigned int numberOfRvaAndSizes;
    DataDirectoryEntry dataDirectory[16];
} PEOptionalHeader;

typedef struct {
    char name[8];
    unsigned int virtualSize;
    unsigned int virtualAddress;
    unsigned int sizeOfRawData;
    unsigned int pointerToRawData;
    unsigned int pointerToRelocs;
    unsigned int pointerToLineNums;
    unsigned short numberOfRelocs;
    unsigned short numberOfLineNums;
    unsigned int characteristics;
} PESectionHeader;

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
} ImportDirectory;

#pragma pack(pop)

unsigned char *textSection = NULL;
unsigned int textSectionSize = 0;
unsigned char *dataSection = NULL;
unsigned int dataSectionSize = 0;

typedef struct {
    char dllName[64];
    char funcName[64];
} PEImportEntry;

PEImportEntry peImports[100];
int peImportCount = 0;

void pe_init() {
    if (textSection) free(textSection);
    if (dataSection) free(dataSection);
    textSection = malloc(4096);
    textSectionSize = 0;
    dataSection = malloc(4096);
    dataSectionSize = 0;
    peImportCount = 0;
    memset(peImports, 0, sizeof(peImports));
}

void pe_add_import(const char *dllName, const char *funcName) {
    if (peImportCount < 100) {
        strncpy(peImports[peImportCount].dllName, dllName, 63);
        strncpy(peImports[peImportCount].funcName, funcName, 63);
        peImportCount++;
    }
}

void pe_set_text_section(unsigned char *data, unsigned int size) {
    if (textSection) free(textSection);
    textSection = malloc((size + 4095) & ~4095);
    memcpy(textSection, data, size);
    textSectionSize = size;
}

void pe_set_data_section(unsigned char *data, unsigned int size) {
    if (dataSection) free(dataSection);
    dataSection = malloc((size + 4095) & ~4095);
    memcpy(dataSection, data, size);
    dataSectionSize = size;
}

int pe_align(int value, int align) {
    return (value + align - 1) & ~(align - 1);
}

int pe_write_file(const char *filename) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) return -1;

    unsigned char header[1024];
    memset(header, 0, sizeof(header));

    DOSHeader *dosHeader = (DOSHeader *)header;
    dosHeader->e_magic = 0x5A4D;
    dosHeader->e_lfanew = 0x80;

    unsigned char *peSig = header + 0x80;
    peSig[0] = 'P';
    peSig[1] = 'E';
    peSig[2] = 0;
    peSig[3] = 0;

    PEFileHeader *fileHeader = (PEFileHeader *)(header + 0x84);
    fileHeader->machine = 0x8664;
    fileHeader->numberOfSections = 3;
    fileHeader->sizeOfOptionalHeader = 0xF0;
    fileHeader->characteristics = 0x22;

    PEOptionalHeader *optHeader = (PEOptionalHeader *)(header + 0x98);
    optHeader->magic = 0x20B;
    optHeader->majorLinkerVersion = 14;
    optHeader->minorLinkerVersion = 0;
    optHeader->sizeOfCode = pe_align(textSectionSize, 0x1000);
    optHeader->sizeOfInitializedData = pe_align(dataSectionSize, 0x1000);
    optHeader->addressOfEntryPoint = 0x1000;
    optHeader->baseOfCode = 0x1000;
    optHeader->sectionAlignment = 0x1000;
    optHeader->fileAlignment = 0x200;
    optHeader->majorOSVersion = 6;
    optHeader->minorOSVersion = 0;
    optHeader->majorSubsystemVersion = 6;
    optHeader->minorSubsystemVersion = 0;
    optHeader->sizeOfImage = 0x3000;
    optHeader->sizeOfHeaders = 0x200;
    optHeader->subsystem = 3;
    optHeader->numberOfRvaAndSizes = 16;

    unsigned int importRVA = 0x3000;
    optHeader->dataDirectory[1].rva = importRVA;
    optHeader->dataDirectory[1].size = peImportCount * 20 + 40;

    unsigned long long imageBase = 0x140000000ULL;
    optHeader->imageBase = (unsigned int)(imageBase & 0xFFFFFFFF);

    PESectionHeader *textSectionHeader = (PESectionHeader *)(header + 0x178);
    memcpy(textSectionHeader->name, ".text\0\0\0", 8);
    textSectionHeader->virtualSize = textSectionSize;
    textSectionHeader->virtualAddress = 0x1000;
    textSectionHeader->sizeOfRawData = pe_align(textSectionSize, 0x200);
    textSectionHeader->pointerToRawData = 0x200;
    textSectionHeader->characteristics = 0x60000020;

    PESectionHeader *dataSectionHeader = (PESectionHeader *)(header + 0x190);
    memcpy(dataSectionHeader->name, ".data\0\0\0", 8);
    dataSectionHeader->virtualSize = dataSectionSize;
    dataSectionHeader->virtualAddress = 0x2000;
    dataSectionHeader->sizeOfRawData = pe_align(dataSectionSize, 0x200);
    dataSectionHeader->pointerToRawData = 0x200 + pe_align(textSectionSize, 0x200);
    dataSectionHeader->characteristics = 0xC0000040;

    PESectionHeader *importSectionHeader = (PESectionHeader *)(header + 0x1A8);
    memcpy(importSectionHeader->name, ".idata\0\0", 8);
    importSectionHeader->virtualSize = peImportCount * 20 + 40 + 256;
    importSectionHeader->virtualAddress = 0x3000;
    importSectionHeader->sizeOfRawData = pe_align(peImportCount * 20 + 40 + 256, 0x200);
    importSectionHeader->pointerToRawData = 0x200 + pe_align(textSectionSize, 0x200) + pe_align(dataSectionSize, 0x200);
    importSectionHeader->characteristics = 0xC0000040;

    fwrite(header, 1, 0x200, fp);

    unsigned char *textData = textSection;
    unsigned int textPaddedSize = pe_align(textSectionSize, 0x200);
    for (unsigned int i = textSectionSize; i < textPaddedSize; i++) {
        textData[i] = 0x90;
    }
    fwrite(textData, 1, textPaddedSize, fp);

    unsigned char *dataPadded = dataSection;
    unsigned int dataPaddedSize = pe_align(dataSectionSize, 0x200);
    if (dataSectionSize < dataPaddedSize) {
        dataPadded = malloc(dataPaddedSize);
        memcpy(dataPadded, dataSection, dataSectionSize);
        memset(dataPadded + dataSectionSize, 0, dataPaddedSize - dataSectionSize);
    }
    fwrite(dataPadded, 1, dataPaddedSize, fp);

    unsigned char *importData = malloc(256);
    memset(importData, 0, 256);

    unsigned int offset = 0;
    for (int i = 0; i < peImportCount; i++) {
        ImportDirectory *importDir = (ImportDirectory *)(importData + offset);
        importDir->characteristics = 0;
        importDir->timeDateStamp = 0;
        importDir->majorVersion = 0;
        importDir->minorVersion = 0;
        importDir->name = importRVA + offset + 20;
        importDir->base = i + 1;
        importDir->numberOfFunctions = 1;
        importDir->numberOfNames = 1;
        importDir->addressOfFunctions = importRVA + offset + 40;
        importDir->addressOfNames = importRVA + offset + 48;
        importDir->addressOfNameOrdinals = 0;

        char *dllNamePtr = (char *)(importData + offset + 20);
        strcpy(dllNamePtr, peImports[i].dllName);

        unsigned int *thunkPtr = (unsigned int *)(importData + offset + 40);
        *thunkPtr = importRVA + offset + 64;

        char *funcNamePtr = (char *)(importData + offset + 64);
        strcpy(funcNamePtr, peImports[i].funcName);

        offset += 20;
    }

    unsigned int *nullImport = (unsigned int *)(importData + offset);
    *nullImport = 0;
    *(nullImport + 1) = 0;
    *(nullImport + 2) = 0;
    *(nullImport + 3) = 0;
    *(nullImport + 4) = 0;

    unsigned int importPaddedSize = pe_align(peImportCount * 20 + 40 + 256, 0x200);
    fwrite(importData, 1, importPaddedSize, fp);

    fclose(fp);

    free(dataPadded);
    free(importData);

    return 0;
}
