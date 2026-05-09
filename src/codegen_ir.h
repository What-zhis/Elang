#ifndef CODEGEN_IR_H
#define CODEGEN_IR_H

#include "parser.h"

void generateIR(ASTNode *node);
void generateMachineCode(ASTNode *node);

#endif