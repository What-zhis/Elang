#include "preprocessor.h"
#include <ctype.h>
#include <stdarg.h>

static int isIdStart(int c) {
    return isalpha(c) || c == '_';
}

static int isIdChar(int c) {
    return isalnum(c) || c == '_';
}

static void skipWhitespace(const char **p) {
    while (**p == ' ' || **p == '\t' || **p == '\r') (*p)++;
}

static void skipToEOL(const char **p) {
    while (**p && **p != '\n') (*p)++;
    if (**p == '\n') (*p)++;
}

static void skipMacroDefinition(const char **p) {
    while (**p && **p == ' ') (*p)++;
    while (**p && **p != ' ' && **p != '\t' && **p != '\n') (*p)++;
    while (**p && (**p == ' ' || **p == '\t')) (*p)++;
    while (**p && **p != '(' && **p != '\n') (*p)++;
    if (**p == '(') {
        (*p)++;
        int parenCount = 1;
        while (**p && parenCount > 0) {
            if (**p == '(') parenCount++;
            else if (**p == ')') parenCount--;
            (*p)++;
        }
    }
    while (**p && **p == ' ') (*p)++;
    if (**p == '=') {
        (*p)++;
        while (**p && **p == ' ') (*p)++;
        if (**p == '{') {
            (*p)++;
            int braceCount = 1;
            while (**p && braceCount > 0) {
                if (**p == '{') braceCount++;
                else if (**p == '}') braceCount--;
                (*p)++;
            }
        } else {
            while (**p && **p != '\n') (*p)++;
        }
    } else if (**p == '{') {
        (*p)++;
        int braceCount = 1;
        while (**p && braceCount > 0) {
            if (**p == '{') braceCount++;
            else if (**p == '}') braceCount--;
            (*p)++;
        }
    }
    if (**p == '}') (*p)++;
    while (**p && **p != '\n') (*p)++;
    if (**p == '\n') (*p)++;
}

static int parseIdentifier(const char **p, char *buf, int bufsize) {
    skipWhitespace(p);
    if (!isIdStart(**p)) return 0;

    int i = 0;
    while (isIdChar(**p) && i < bufsize - 1) {
        buf[i++] = **p;
        (*p)++;
    }
    buf[i] = '\0';
    return 1;
}

static int parseMacroParams(const char **p, MacroParam *params, int *paramCount) {
    skipWhitespace(p);

    int count = 0;
    while (**p && **p != ')' && count < MAX_MACRO_PARAMS) {
        skipWhitespace(p);
        if (!isIdStart(**p)) break;

        char paramName[64];
        int i = 0;
        while (isIdChar(**p) && i < 63) {
            paramName[i++] = **p;
            (*p)++;
        }
        paramName[i] = '\0';

        strcpy(params[count].name, paramName);
        params[count].hasDefault = 0;
        params[count].defaultValue[0] = '\0';

        skipWhitespace(p);
        if (**p == '=') {
            (*p)++;
            skipWhitespace(p);

            if (**p == '"') {
                (*p)++;
                int j = 0;
                while (**p && **p != '"' && j < 255) {
                    if (**p == '\\') {
                        (*p)++;
                        if (**p) params[count].defaultValue[j++] = *((*p)++);
                    } else {
                        params[count].defaultValue[j++] = *(*p)++;
                    }
                }
                if (**p == '"') (*p)++;
                params[count].defaultValue[j] = '\0';
                params[count].hasDefault = 1;
            } else {
                int j = 0;
                while (**p && !isspace(**p) && **p != ',' && **p != ')' && j < 255) {
                    params[count].defaultValue[j++] = *((*p)++);
                }
                params[count].defaultValue[j] = '\0';
                params[count].hasDefault = 1;
            }
        }

        skipWhitespace(p);
        if (**p == ',') (*p)++;
        skipWhitespace(p);

        count++;
    }

    *paramCount = count;
    return 1;
}

static int parseMacroBody(const char **p, char *body, int bufsize) {
    skipWhitespace(p);

    int i = 0;
    while (**p && **p != '\n' && **p != '\r' && i < bufsize - 1) {
        body[i++] = *((*p)++);
    }
    body[i] = '\0';

    // 去掉首尾的空格
    while (i > 0 && (body[i-1] == ' ' || body[i-1] == '\t')) {
        body[--i] = '\0';
    }
    return i > 0;
}

void initPreprocessor(Preprocessor *pp) {
    pp->globalScope = (MacroScope *)calloc(1, sizeof(MacroScope));
    pp->currentScope = pp->globalScope;
    pp->ifdefDepth = 0;
    pp->skipBlock = 0;
    pp->elseDepth = -1;
    pp->secondaryMacroCount = 0;
    
    // 注册内置二级宏
    
    // $print - 打印输出
    SecondaryMacro printMacro;
    memset(&printMacro, 0, sizeof(printMacro));
    strcpy(printMacro.name, "print");
    printMacro.type = SECONDARY_FUNCTION;
    strcpy(printMacro.execBody, "printf(expr)");
    strcpy(printMacro.params[0].name, "expr");
    printMacro.params[0].hasDefault = 0;
    printMacro.paramCount = 1;
    printMacro.isActive = 1;
    addSecondaryMacro(pp, &printMacro);
    
    // $println - 打印输出并换行
    SecondaryMacro printlnMacro;
    memset(&printlnMacro, 0, sizeof(printlnMacro));
    strcpy(printlnMacro.name, "println");
    printlnMacro.type = SECONDARY_FUNCTION;
    strcpy(printlnMacro.execBody, "printf(expr); printf(\"\\\\n\")");
    strcpy(printlnMacro.params[0].name, "expr");
    printlnMacro.params[0].hasDefault = 0;
    printlnMacro.paramCount = 1;
    printlnMacro.isActive = 1;
    addSecondaryMacro(pp, &printlnMacro);
    
    // $assert - 断言
    SecondaryMacro assertMacro;
    memset(&assertMacro, 0, sizeof(assertMacro));
    strcpy(assertMacro.name, "assert");
    assertMacro.type = SECONDARY_FUNCTION;
    strcpy(assertMacro.execBody, "if(!(condition)) { printf(\"Assertion failed: %s at %s:%d\\\\n\", #condition, __FILE__, __LINE__); exit(1); }");
    strcpy(assertMacro.params[0].name, "condition");
    assertMacro.params[0].hasDefault = 0;
    assertMacro.paramCount = 1;
    assertMacro.isActive = 1;
    addSecondaryMacro(pp, &assertMacro);
    
    // $typeof - 获取类型信息（简化版）
    SecondaryMacro typeofMacro;
    memset(&typeofMacro, 0, sizeof(typeofMacro));
    strcpy(typeofMacro.name, "typeof");
    typeofMacro.type = SECONDARY_FUNCTION;
    strcpy(typeofMacro.execBody, "(_Generic((expr), int: \"int\", long: \"long\", char: \"char\", float: \"float\", double: \"double\", default: \"unknown\"))");
    strcpy(typeofMacro.params[0].name, "expr");
    typeofMacro.params[0].hasDefault = 0;
    typeofMacro.paramCount = 1;
    typeofMacro.isActive = 1;
    addSecondaryMacro(pp, &typeofMacro);
    
    // $sizeof - 获取大小
    SecondaryMacro sizeofMacro;
    memset(&sizeofMacro, 0, sizeof(typeofMacro));
    strcpy(sizeofMacro.name, "sizeof");
    sizeofMacro.type = SECONDARY_FUNCTION;
    strcpy(sizeofMacro.execBody, "sizeof(expr)");
    strcpy(sizeofMacro.params[0].name, "expr");
    sizeofMacro.params[0].hasDefault = 0;
    sizeofMacro.paramCount = 1;
    sizeofMacro.isActive = 1;
    addSecondaryMacro(pp, &sizeofMacro);
}

void freePreprocessor(Preprocessor *pp) {
    MacroScope *scope = pp->globalScope;
    while (scope) {
        MacroScope *next = scope->parent;
        free(scope);
        scope = next;
    }
    pp->globalScope = NULL;
    pp->currentScope = NULL;
}

Macro *findMacro(Preprocessor *pp, const char *name) {
    MacroScope *scope = pp->currentScope;
    while (scope) {
        for (int i = 0; i < scope->macroCount; i++) {
            if (scope->macros[i].isActive &&
                strcmp(scope->macros[i].name, name) == 0) {
                return &scope->macros[i];
            }
        }
        scope = scope->parent;
    }
    return NULL;
}

int addMacro(Preprocessor *pp, const char *name, MacroType type, const char *body) {
    if (pp->currentScope->macroCount >= 256) return 0;

    // 只在当前 scope 检查是否有同名宏，而不是整条链
    int i;
    for (i = 0; i < pp->currentScope->macroCount; i++) {
        if (strcmp(pp->currentScope->macros[i].name, name) == 0) {
            // 在当前 scope 找到，更新这个
            pp->currentScope->macros[i].isActive = 1;
            pp->currentScope->macros[i].type = type;
            strncpy(pp->currentScope->macros[i].body, body, MAX_MACRO_BODY - 1);
            pp->currentScope->macros[i].body[MAX_MACRO_BODY - 1] = '\0';
            return 1;
        }
    }

    // 当前 scope 没有，新建一个
    Macro *m = &pp->currentScope->macros[pp->currentScope->macroCount++];
    strncpy(m->name, name, 127);
    m->name[127] = '\0';
    m->type = type;
    m->paramCount = 0;
    m->isActive = 1;
    strncpy(m->body, body, MAX_MACRO_BODY - 1);
    m->body[MAX_MACRO_BODY - 1] = '\0';
    return 1;
}

int addMacroWithParams(Preprocessor *pp, const char *name, MacroType type,
                       int paramCount, MacroParam *params, const char *body) {
    if (pp->currentScope->macroCount >= 256) return 0;

    // 只在当前 scope 检查是否有同名宏
    int i;
    for (i = 0; i < pp->currentScope->macroCount; i++) {
        if (strcmp(pp->currentScope->macros[i].name, name) == 0) {
            // 在当前 scope 找到，更新这个
            pp->currentScope->macros[i].isActive = 1;
            pp->currentScope->macros[i].type = type;
            pp->currentScope->macros[i].paramCount = paramCount;
            for (int j = 0; j < paramCount && j < MAX_MACRO_PARAMS; j++) {
                pp->currentScope->macros[i].params[j] = params[j];
            }
            strncpy(pp->currentScope->macros[i].body, body, MAX_MACRO_BODY - 1);
            pp->currentScope->macros[i].body[MAX_MACRO_BODY - 1] = '\0';
            return 1;
        }
    }

    // 当前 scope 没有，新建一个
    Macro *m = &pp->currentScope->macros[pp->currentScope->macroCount++];
    strncpy(m->name, name, 127);
    m->name[127] = '\0';
    m->type = type;
    m->paramCount = paramCount;
    for (int j = 0; j < paramCount && j < MAX_MACRO_PARAMS; j++) {
        m->params[j] = params[j];
    }
    m->isActive = 1;
    strncpy(m->body, body, MAX_MACRO_BODY - 1);
    m->body[MAX_MACRO_BODY - 1] = '\0';
    return 1;
}

void undefMacro(Preprocessor *pp, const char *name) {
    Macro *m = findMacro(pp, name);
    if (m) {
        m->isActive = 0;
    }
}

void pushScope(Preprocessor *pp) {
    MacroScope *newScope = (MacroScope *)calloc(1, sizeof(MacroScope));
    newScope->parent = pp->currentScope;
    pp->currentScope = newScope;
}

void popScope(Preprocessor *pp) {
    if (pp->currentScope->parent) {
        MacroScope *old = pp->currentScope;
        pp->currentScope = pp->currentScope->parent;
        free(old);
    }
}

static void copyUntil(char **dest, const char **src, int *destSize, int maxSize, const char *until, int untilLen) {
    while (**src && strncmp(*src, until, untilLen) != 0 && *destSize < maxSize - 1) {
        (*dest)[(*destSize)++] = *(*src)++;
    }
}

static char *expandFunctionMacro(Preprocessor *pp, Macro *m, const char **p) {
    static char result[MAX_MACRO_BODY * 2];
    result[0] = '\0';
    int resultSize = 0;

    char argValues[MAX_MACRO_PARAMS][256];
    int argCount = 0;

    skipWhitespace(p);
    if (**p != '(') {
        strcpy(result, m->body);
        return result;
    }
    (*p)++;

    for (int i = 0; i < m->paramCount && **p && **p != ')'; i++) {
        int depth = 0;
        int j = 0;
        argValues[i][0] = '\0';

        skipWhitespace(p);
        while (**p && (**p != ',' || depth > 0) && **p != ')') {
            if (**p == '(') {
                depth++;
                argValues[i][j++] = *(*p)++;
                continue;
            } else if (**p == ')') {
                if (depth == 0) break;
                depth--;
                argValues[i][j++] = *(*p)++;
                continue;
            }
            if (**p == '"') {
                argValues[i][j++] = *(*p)++;
                while (**p && **p != '"') {
                    if (**p == '\\') {
                        argValues[i][j++] = *(*p)++;
                    }
                    argValues[i][j++] = *(*p)++;
                }
                if (**p == '"') argValues[i][j++] = *(*p)++;
            } else {
                argValues[i][j++] = *(*p)++;
            }
        }
        argValues[i][j] = '\0';
        argCount++;

        if (**p == ',') (*p)++;
    }

    if (**p == ')') (*p)++;

    int bodyLen = strlen(m->body);
    int inId = 0;
    for (int i = 0; i < bodyLen && resultSize < MAX_MACRO_BODY * 2 - 1; i++) {
        char c = m->body[i];
        if (isIdStart(c) || (inId && isIdChar(c))) {
            inId = 1;
            char idBuf[64];
            int idLen = 0;
            idBuf[idLen++] = c;
            while (i + 1 < bodyLen && isIdChar(m->body[i + 1])) {
                idBuf[idLen++] = m->body[++i];
            }
            idBuf[idLen] = '\0';

            int matched = 0;
            for (int j = 0; j < m->paramCount; j++) {
                if (strcmp(idBuf, m->params[j].name) == 0) {
                    int argLen = strlen(argValues[j]);
                    if (resultSize + argLen < MAX_MACRO_BODY * 2 - 1) {
                        strcpy(result + resultSize, argValues[j]);
                        resultSize += argLen;
                    }
                    matched = 1;
                    break;
                }
            }
            if (!matched) {
                int k;
                for (k = 0; k < idLen && resultSize < MAX_MACRO_BODY * 2 - 1; k++) {
                    result[resultSize++] = idBuf[k];
                }
            }
        } else if (c == '#' && i + 1 < bodyLen && isIdStart(m->body[i + 1])) {
            // 处理字符串化操作符 #
            i++;
            char idBuf[64];
            int idLen = 0;
            idBuf[idLen++] = m->body[i];
            while (i + 1 < bodyLen && isIdChar(m->body[i + 1])) {
                idBuf[idLen++] = m->body[++i];
            }
            idBuf[idLen] = '\0';

            int found = 0;
            for (int j = 0; j < m->paramCount; j++) {
                if (strcmp(idBuf, m->params[j].name) == 0) {
                    // 字符串化这个参数
                    char *str = stringify(pp, argValues[j]);
                    int strLen = strlen(str);
                    if (resultSize + strLen < MAX_MACRO_BODY * 2 - 1) {
                        strcpy(result + resultSize, str);
                        resultSize += strLen;
                    }
                    found = 1;
                    break;
                }
            }
            if (!found) {
                if (resultSize + 1 < MAX_MACRO_BODY * 2 - 1) {
                    result[resultSize++] = '#';
                    int k;
                    for (k = 0; k < idLen && resultSize < MAX_MACRO_BODY * 2 - 1; k++) {
                        result[resultSize++] = idBuf[k];
                    }
                }
            }
        } else if (c == '#' && i + 1 < bodyLen && m->body[i + 1] == '#') {
            // 处理拼接操作符 ##（先跳过）
            i++;
            // 暂时不处理，我们需要先找到前后的标识符
            // 这里实现一个简化版：跳过 ##，让后续的 expandMacros 再次处理
        } else {
            inId = 0;
            result[resultSize++] = c;
        }
    }
    result[resultSize] = '\0';

    return result;
}

static char *expandSecondaryMacro(Preprocessor *pp, SecondaryMacro *sm, const char **p);

char *expandMacros(Preprocessor *pp, const char *input) {
    static char result[MAX_MACRO_BODY * 4];
    result[0] = '\0';
    int resultSize = 0;

    const char *p = input;
    int inId = 0;
    char currentId[64];
    int currentIdLen = 0;
    int inString = 0;

    while (*p && resultSize < MAX_MACRO_BODY * 4 - 1) {
        // 跳过字符串内部内容
        if (*p == '"' && !inString) {
            inString = 1;
            result[resultSize++] = *p++;
            while (*p && (*p != '"' || (*(p-1) == '\\')) && resultSize < MAX_MACRO_BODY * 4 - 1) {
                result[resultSize++] = *p++;
            }
            if (*p == '"') {
                result[resultSize++] = *p++;
            }
            inString = 0;
            continue;
        }

        // 跳过单引号字符
        if (*p == '\'' && !inString) {
            result[resultSize++] = *p++;
            if (*p == '\\') {
                result[resultSize++] = *p++;
            }
            if (*p) {
                result[resultSize++] = *p++;
            }
            if (*p == '\'') {
                result[resultSize++] = *p++;
            }
            continue;
        }

        // 跳过双单引号（汇编字符串）
        if (*p == '\'' && *(p+1) == '\'' && !inString) {
            result[resultSize++] = *p++;
            result[resultSize++] = *p++;
            while (*p && !(*p == '\'' && *(p+1) == '\'')) {
                if (*p == '\\') {
                    result[resultSize++] = *p++;
                }
                if (*p) {
                    result[resultSize++] = *p++;
                }
            }
            if (*p == '\'' && *(p+1) == '\'') {
                result[resultSize++] = *p++;
                result[resultSize++] = *p++;
            }
            continue;
        }

        if (isspace(*p) || *p == '(' || *p == ')' || *p == ',' || *p == ';' ||
            *p == '[' || *p == ']' || *p == '{' || *p == '}' || *p == '+' ||
            *p == '-' || *p == '*' || *p == '/' || *p == '=' || *p == '<' ||
            *p == '>' || *p == '!' || *p == '&' || *p == '|' || *p == '^' ||
            *p == '%' || *p == '~' || *p == '.' || *p == ':' || *p == '#') {

            if (inId && currentIdLen > 0) {
                currentId[currentIdLen] = '\0';
                SecondaryMacro *sm = findSecondaryMacro(pp, currentId, -1);
                if (sm && sm->isActive && *p == '(') {
                    const char *funcStart = p;
                    char *expanded = expandSecondaryMacro(pp, sm, &funcStart);
                    int expLen = strlen(expanded);
                    if (resultSize + expLen < MAX_MACRO_BODY * 4 - 1) {
                        strcpy(result + resultSize, expanded);
                        resultSize += expLen;
                    }
                    p = funcStart;
                    currentIdLen = 0;
                    inId = 0;
                    continue;
                } else if (sm && sm->isActive) {
                    for (int k = 0; k < currentIdLen; k++) {
                        result[resultSize++] = currentId[k];
                    }
                    currentIdLen = 0;
                    inId = 0;
                    continue;
                }
                Macro *m = findMacro(pp, currentId);
                if (m && m->isActive) {
                    fprintf(stderr, "[DEBUG] Found macro: %s\n", currentId);
                    if (m->type == MACRO_FUNCTION) {
                        const char *funcStart = p;
                        char *expanded = expandFunctionMacro(pp, m, &funcStart);
                        int expLen = strlen(expanded);
                        if (resultSize + expLen < MAX_MACRO_BODY * 4 - 1) {
                            strcpy(result + resultSize, expanded);
                            resultSize += expLen;
                        }
                        p = funcStart;
                    } else {
                        int bodyLen = strlen(m->body);
                        if (resultSize + bodyLen < MAX_MACRO_BODY * 4 - 1) {
                            strcpy(result + resultSize, m->body);
                            resultSize += bodyLen;
                        }
                    }
                    currentIdLen = 0;
                } else {
                    int k;
                    for (k = 0; k < currentIdLen; k++) {
                        result[resultSize++] = currentId[k];
                    }
                    currentIdLen = 0;
                }
            }
            inId = 0;
            result[resultSize++] = *p++;
        } else if (isIdStart(*p)) {
            inId = 1;
            if (currentIdLen < 63) {
                currentId[currentIdLen++] = *p;
            }
            p++;
        } else {
            if (inId && currentIdLen > 0) {
                currentId[currentIdLen] = '\0';
                Macro *m = findMacro(pp, currentId);
                if (m && m->isActive) {
                    int bodyLen = strlen(m->body);
                    if (resultSize + bodyLen < MAX_MACRO_BODY * 4 - 1) {
                        strcpy(result + resultSize, m->body);
                        resultSize += bodyLen;
                    }
                } else {
                    int k;
                    for (k = 0; k < currentIdLen; k++) {
                        result[resultSize++] = currentId[k];
                    }
                }
                currentIdLen = 0;
            }
            inId = 0;
            result[resultSize++] = *p++;
        }
    }

    if (inId && currentIdLen > 0) {
        currentId[currentIdLen] = '\0';
        Macro *m = findMacro(pp, currentId);
        if (m && m->isActive) {
            int bodyLen = strlen(m->body);
            if (resultSize + bodyLen < MAX_MACRO_BODY * 4 - 1) {
                strcpy(result + resultSize, m->body);
                resultSize += bodyLen;
            }
        } else {
            int k;
            for (k = 0; k < currentIdLen; k++) {
                result[resultSize++] = currentId[k];
            }
        }
    }

    result[resultSize] = '\0';
    return result;
}

char *stringify(Preprocessor *pp, const char *input) {
    static char result[MAX_MACRO_BODY * 2];
    result[0] = '"';
    int resultSize = 1;

    const char *p = input;
    while (*p && resultSize < MAX_MACRO_BODY * 2 - 3) {
        if (*p == '"') {
            result[resultSize++] = '\\';
            result[resultSize++] = '"';
            p++;
        } else if (*p == '\\') {
            result[resultSize++] = '\\';
            result[resultSize++] = '\\';
            p++;
        } else {
            result[resultSize++] = *p++;
        }
    }

    result[resultSize++] = '"';
    result[resultSize] = '\0';
    return result;
}

char *concatenate(Preprocessor *pp, const char *left, const char *right) {
    static char result[MAX_MACRO_BODY * 2];
    int leftLen = strlen(left);
    int rightLen = strlen(right);

    if (leftLen + rightLen >= MAX_MACRO_BODY * 2) {
        result[0] = '\0';
        return result;
    }

    strcpy(result, left);
    strcat(result, right);
    return result;
}

int addSecondaryMacro(Preprocessor *pp, SecondaryMacro *sm) {
    if (pp->secondaryMacroCount >= MAX_SECONDARY_MACROS) return 0;
    
    SecondaryMacro *dest = &pp->secondaryMacros[pp->secondaryMacroCount++];
    memcpy(dest, sm, sizeof(SecondaryMacro));
    return 1;
}

SecondaryMacro *findSecondaryMacro(Preprocessor *pp, const char *name, SecondaryMacroType type) {
    for (int i = 0; i < pp->secondaryMacroCount; i++) {
        if (pp->secondaryMacros[i].isActive &&
            strcmp(pp->secondaryMacros[i].name, name) == 0 &&
            (type == -1 || pp->secondaryMacros[i].type == type)) {
            return &pp->secondaryMacros[i];
        }
    }
    return NULL;
}

static char *expandSecondaryMacro(Preprocessor *pp, SecondaryMacro *sm, const char **p) {
    static char result[MAX_MACRO_BODY * 2];
    result[0] = '\0';
    int resultSize = 0;

    char argValues[MAX_MACRO_PARAMS][1024];
    int argCount = 0;

    skipWhitespace(p);
    if (**p != '(') {
        strcpy(result, sm->execBody);
        return result;
    }
    (*p)++;

    for (int i = 0; i < sm->paramCount && **p && **p != ')'; i++) {
        int depth = 0;
        int j = 0;
        argValues[i][0] = '\0';

        skipWhitespace(p);
        while (**p && (**p != ',' || depth > 0)) {
            if (**p == '\n' || **p == '\r') {
                if (depth == 0) break;
                (*p)++;
                continue;
            }
            if (**p == '}') {
                if (depth == 0) break;
                (*p)++;
                continue;
            }
            if (**p == ')') {
                if (depth == 0) break;
                depth--;
                argValues[i][j++] = *(*p)++;
                continue;
            }
            if (**p == '(') {
                depth++;
                argValues[i][j++] = *(*p)++;
                continue;
            }
            if (**p == '"') {
                argValues[i][j++] = *(*p)++;
                while (**p && **p != '"') {
                    if (**p == '\\') {
                        argValues[i][j++] = *(*p)++;
                    }
                    argValues[i][j++] = *(*p)++;
                }
                if (**p == '"') argValues[i][j++] = *(*p)++;
            } else {
                argValues[i][j++] = *(*p)++;
            }
        }
        argValues[i][j] = '\0';
        argCount++;

        if (**p == ',') (*p)++;
    }

    // 跳过换行和空白，找到右括号
    while (**p && (**p == '\n' || **p == '\r' || **p == ' ' || **p == '\t')) {
        (*p)++;
    }
    if (**p == ')') (*p)++;

    int bodyLen = strlen(sm->execBody);
    int inId = 0;
    for (int i = 0; i < bodyLen && resultSize < MAX_MACRO_BODY * 2 - 1; i++) {
        char c = sm->execBody[i];
        if (isIdStart(c) || (inId && isIdChar(c))) {
            inId = 1;
            char idBuf[64];
            int idLen = 0;
            idBuf[idLen++] = c;
            while (i + 1 < bodyLen && isIdChar(sm->execBody[i + 1])) {
                idBuf[idLen++] = sm->execBody[++i];
            }
            idBuf[idLen] = '\0';

            int matched = 0;
            for (int j = 0; j < sm->paramCount; j++) {
                if (strcmp(idBuf, sm->params[j].name) == 0) {
                    const char *argValue = argValues[j];
                    if (j >= argCount && sm->params[j].hasDefault) {
                        argValue = sm->params[j].defaultValue;
                    }
                    int argLen = strlen(argValue);
                    if (resultSize + argLen < MAX_MACRO_BODY * 2 - 1) {
                        strcpy(result + resultSize, argValue);
                        resultSize += argLen;
                    }
                    matched = 1;
                    break;
                }
            }
            if (!matched) {
                int k;
                for (k = 0; k < idLen && resultSize < MAX_MACRO_BODY * 2 - 1; k++) {
                    result[resultSize++] = idBuf[k];
                }
            }
        } else {
            inId = 0;
            result[resultSize++] = c;
        }
    }
    result[resultSize] = '\0';

    return result;
}

static SecondaryMacroType parseSecondaryType(const char *typeStr) {
    if (strcmp(typeStr, "keyword") == 0) return SECONDARY_KEYWORD;
    if (strcmp(typeStr, "function") == 0) return SECONDARY_FUNCTION;
    if (strcmp(typeStr, "operator") == 0) return SECONDARY_OPERATOR;
    if (strcmp(typeStr, "syntax") == 0) return SECONDARY_SYNTAX;
    if (strcmp(typeStr, "type") == 0) return SECONDARY_TYPEDEF;
    return SECONDARY_KEYWORD;
}

static int handleDirective(Preprocessor *pp, const char **input) {
    // 检查是否是二级宏（通过 $ 标识）
    if (**input == '$') {
        (*input)++;
        skipWhitespace(input);
        
        char name[128];
        if (!parseIdentifier(input, name, sizeof(name))) {
            int i = 0;
            while (**input && !isspace(**input) && **input != ':' && **input != ';' && i < 127) {
                name[i++] = *(*input)++;
            }
            name[i] = '\0';
            if (i == 0) {
                skipToEOL(input);
                return 1;
            }
        }
        
        skipWhitespace(input);
        if (**input != ':') {
            skipToEOL(input);
            return 1;
        }
        (*input)++;
        skipWhitespace(input);
        
        char typeStr[32];
        if (!parseIdentifier(input, typeStr, sizeof(typeStr))) {
            skipToEOL(input);
            return 1;
        }
        
        SecondaryMacro sm;
        memset(&sm, 0, sizeof(sm));
        strncpy(sm.name, name, sizeof(sm.name) - 1);
        sm.type = parseSecondaryType(typeStr);
        sm.isActive = 1;
        sm.precedence = 0;
        sm.associativity = 0;
        
        // 跳过类型后面的分号
        while (**input && **input != ';') (*input)++;
        if (**input == ';') (*input)++;
        
        // 解析字段（循环处理直到遇到 exec 块或下一个指令）
        while (**input && **input != '#') {
            skipWhitespace(input);
            
            if (**input == '#') {
                break;
            }
            
            if (strncmp(*input, "exec:", 5) == 0) {
                (*input) += 5;
                skipWhitespace(input);
                if (**input == '{') {
                    (*input)++;
                    int depth = 1;
                    int i = 0;
                    while (**input && depth > 0 && i < sizeof(sm.execBody) - 1) {
                        if (**input == '{') depth++;
                        else if (**input == '}') depth--;
                        if (depth > 0) sm.execBody[i++] = *(*input)++;
                    }
                    sm.execBody[i] = '\0';
                    if (**input == '}') (*input)++;
                    if (**input == ';') (*input)++;
                }
                // exec 是最后一个字段，直接退出字段解析
                break;
            } else if (strncmp(*input, "pattern:", 8) == 0) {
                (*input) += 8;
                skipWhitespace(input);
                if (**input == '"') {
                    (*input)++;
                    int i = 0;
                    while (**input && **input != '"' && i < sizeof(sm.pattern) - 1) {
                        sm.pattern[i++] = *(*input)++;
                    }
                    sm.pattern[i] = '\0';
                    if (**input == '"') (*input)++;
                }
                while (**input && **input != ';' && **input != '#') (*input)++;
                if (**input == ';') (*input)++;
            } else if (strncmp(*input, "precedence:", 11) == 0) {
                (*input) += 11;
                skipWhitespace(input);
                sm.precedence = 0;
                while (isdigit(**input)) {
                    sm.precedence = sm.precedence * 10 + (**input - '0');
                    (*input)++;
                }
                while (**input && **input != ';' && **input != '#') (*input)++;
                if (**input == ';') (*input)++;
            } else if (strncmp(*input, "associativity:", 14) == 0) {
                (*input) += 14;
                skipWhitespace(input);
                char assoc[16];
                int i = 0;
                while (isalpha(**input) && i < 15) {
                    assoc[i++] = *(*input)++;
                }
                assoc[i] = '\0';
                sm.associativity = (strcmp(assoc, "right") == 0) ? 1 : 0;
                while (**input && **input != ';' && **input != '#') (*input)++;
                if (**input == ';') (*input)++;
            } else if (strncmp(*input, "param:", 6) == 0) {
                (*input) += 6;
                skipWhitespace(input);
                if (**input == '[') {
                    (*input)++;
                    while (**input && **input != ']') {
                        skipWhitespace(input);
                        char paramName[64];
                        int i = 0;
                        while (isIdChar(**input) && i < 63) {
                            paramName[i++] = *(*input)++;
                        }
                        paramName[i] = '\0';
                        if (strlen(paramName) > 0 && sm.paramCount < MAX_MACRO_PARAMS) {
                            strncpy(sm.params[sm.paramCount].name, paramName, 63);
                            sm.params[sm.paramCount].hasDefault = 0;
                            sm.params[sm.paramCount].defaultValue[0] = '\0';
                            
                            skipWhitespace(input);
                            if (**input == '=') {
                                (*input)++;
                                skipWhitespace(input);
                                int j = 0;
                                while (**input && **input != ',' && **input != ']' && j < 255) {
                                    sm.params[sm.paramCount].defaultValue[j++] = *(*input)++;
                                }
                                sm.params[sm.paramCount].defaultValue[j] = '\0';
                                sm.params[sm.paramCount].hasDefault = 1;
                            }
                            sm.paramCount++;
                        }
                        if (**input == ',') (*input)++;
                    }
                    if (**input == ']') (*input)++;
                }
                while (**input && **input != ';' && **input != '#') (*input)++;
                if (**input == ';') (*input)++;
            } else if (strncmp(*input, "fields:", 7) == 0) {
                (*input) += 7;
                skipWhitespace(input);
                if (**input == '[') {
                    (*input)++;
                    while (**input && **input != ']') {
                        skipWhitespace(input);
                        char fieldName[64];
                        int i = 0;
                        while (isIdChar(**input) && i < 63) {
                            fieldName[i++] = *(*input)++;
                        }
                        fieldName[i] = '\0';
                        
                        skipWhitespace(input);
                        if (**input == ':') {
                            (*input)++;
                            skipWhitespace(input);
                            char fieldType[32];
                            int j = 0;
                            while (isIdChar(**input) && j < 31) {
                                fieldType[j++] = *(*input)++;
                            }
                            fieldType[j] = '\0';
                            
                            if (sm.fieldCount < MAX_MACRO_PARAMS) {
                                strncpy(sm.fields[sm.fieldCount].name, fieldName, 63);
                                strncpy(sm.fields[sm.fieldCount].type, fieldType, 31);
                                sm.fieldCount++;
                            }
                        }
                        if (**input == ',') (*input)++;
                    }
                    if (**input == ']') (*input)++;
                }
                while (**input && **input != ';' && **input != '#') (*input)++;
                if (**input == ';') (*input)++;
            } else {
                // 未知字段，跳过整行
                skipToEOL(input);
            }
        }
        
        addSecondaryMacro(pp, &sm);
        return 1;
    }

    skipWhitespace(input);

    char directive[32];
    if (!parseIdentifier(input, directive, sizeof(directive))) {
        skipToEOL(input);
        return 1;
    }

    if (strcmp(directive, "define") == 0) {
        skipWhitespace(input);

        char name[128];
        if (!parseIdentifier(input, name, sizeof(name))) {
            skipToEOL(input);
            return 1;
        }

        skipWhitespace(input);

        if (**input == '(') {
            MacroParam params[MAX_MACRO_PARAMS];
            int paramCount = 0;
            (*input)++;
            parseMacroParams(input, params, &paramCount);

            if (**input == ')') (*input)++;
            skipWhitespace(input);

            char body[MAX_MACRO_BODY];
            parseMacroBody(input, body, sizeof(body));

            addMacroWithParams(pp, name, MACRO_FUNCTION, paramCount, params, body);
        } else {
            static char value[1024];
            int i = 0;
            while (**input && **input != '\n' && **input != '\r' && i < 1023) {
                value[i++] = *(*input)++;
            }
            value[i] = '\0';

            addMacro(pp, name, MACRO_CONSTANT, value);
        }
    } else if (strcmp(directive, "undef") == 0) {
        skipWhitespace(input);
        char name[128];
        if (parseIdentifier(input, name, sizeof(name))) {
            undefMacro(pp, name);
        }
        skipToEOL(input);
    } else if (strcmp(directive, "ifdef") == 0) {
        skipWhitespace(input);
        char name[128];
        if (parseIdentifier(input, name, sizeof(name))) {
            Macro *m = findMacro(pp, name);
            pp->ifdefStack[pp->ifdefDepth++] = m && m->isActive ? 1 : 0;
            if (!m || !m->isActive) {
                pp->skipBlock = 1;
            }
        }
        skipToEOL(input);
    } else if (strcmp(directive, "ifndef") == 0) {
        skipWhitespace(input);
        char name[128];
        if (parseIdentifier(input, name, sizeof(name))) {
            Macro *m = findMacro(pp, name);
            pp->ifdefStack[pp->ifdefDepth++] = (!m || !m->isActive) ? 1 : 0;
            if (m && m->isActive) {
                pp->skipBlock = 1;
            }
        }
        skipToEOL(input);
    } else if (strcmp(directive, "else") == 0) {
        if (pp->ifdefDepth > 0) {
            if (pp->elseDepth == pp->ifdefDepth - 1) {
            } else {
                pp->elseDepth = pp->ifdefDepth - 1;
                if (pp->ifdefStack[pp->ifdefDepth - 1]) {
                    pp->skipBlock = 1;
                } else {
                    pp->skipBlock = 0;
                }
            }
        }
        skipToEOL(input);
    } else if (strcmp(directive, "endif") == 0) {
        if (pp->ifdefDepth > 0) {
            pp->ifdefDepth--;
            if (pp->ifdefDepth <= pp->elseDepth) {
                pp->skipBlock = 0;
                pp->elseDepth = -1;
            }
        }
        skipToEOL(input);
    } else if (strcmp(directive, "push") == 0) {
        pushScope(pp);
        skipToEOL(input);
    } else if (strcmp(directive, "pop") == 0) {
        popScope(pp);
        skipToEOL(input);
    } else {
        char name[128];
        strncpy(name, directive, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';

        skipWhitespace(input);

        if (**input == ':') {
            (*input)++;
            skipWhitespace(input);
            // 这个分支是 # NAME: VALUE（常量宏）
            static char value[1024];
            int i = 0;
            while (**input && **input != '\n' && **input != '\r' && **input != ';' && i < 1023) {
                value[i++] = *(*input)++;
            }
            value[i] = '\0';

            addMacro(pp, name, MACRO_CONSTANT, value);
        } else if (**input == '(') {
            // 这个分支是 # NAME(...): ...（函数宏）
            MacroParam params[MAX_MACRO_PARAMS];
            int paramCount = 0;
            (*input)++;
            parseMacroParams(input, params, &paramCount);

            if (**input == ')') (*input)++;
            skipWhitespace(input);

            if (**input == ':') {
                (*input)++;
                skipWhitespace(input);

                char body[MAX_MACRO_BODY];
                parseMacroBody(input, body, sizeof(body));
                addMacroWithParams(pp, name, MACRO_FUNCTION, paramCount, params, body);
            } else {
                skipToEOL(input);
            }
        } else {
            skipToEOL(input);
        }
    }

    return 1;
}

char *preprocess(Preprocessor *pp, const char *filename, const char *content) {
    static char intermediate[65536];
    intermediate[0] = '\0';
    int intSize = 0;
    int intMax = 65536;

    const char *p = content;

    // 第一遍：解析二级宏并跳过它们，收集其他内容
    while (*p && intSize < intMax - 1) {
        if (*p == '#' && *(p+1) == '#' && *(p+2) == '$' && !pp->skipBlock) {
            // 处理二级宏 ##$
            p += 2;  // 只跳过 ##，保留 $ 供 handleDirective 识别
            if (!handleDirective(pp, &p)) {
                break;
            }
            // handleDirective 已经处理完整个二级宏定义
            // 跳过空白字符
            while (*p == ' ' || *p == '\t' || *p == '\r') {
                p++;
            }
        } else if (*p == '#' && !pp->skipBlock) {
            // 处理一级宏指令
            p++;
            if (!handleDirective(pp, &p)) {
                break;
            }
        } else if (strncmp(p, "macro ", 6) == 0 && !pp->skipBlock) {
            // 跳过 macro 定义行
            skipMacroDefinition(&p);
        } else if (strncmp(p, "macro(", 6) == 0 && !pp->skipBlock) {
            // 跳过 macro 定义行（无空格情况）
            skipMacroDefinition(&p);
        } else if (*p == '#' && *(p+1) == '#' && *(p+2) == '$' && pp->skipBlock) {
            while (*p && *p != '#') {
                p++;
            }
        } else if (*p == '#' && pp->skipBlock) {
            skipToEOL(&p);
        } else if (pp->skipBlock) {
            skipToEOL(&p);
        } else {
            intermediate[intSize++] = *p++;
        }
    }

    intermediate[intSize] = '\0';

    // 第二遍：展开宏
    static char finalResult[65536];
    finalResult[0] = '\0';
    int finalSize = 0;
    int finalMax = 65536;

    char tempBuffer[65536];
    int tempSize = 0;

    p = intermediate;

    while (*p && finalSize < finalMax - 1) {
        if (*p == '#' && !pp->skipBlock) {
            if (tempSize > 0) {
                tempBuffer[tempSize] = '\0';
                char *expandedPart = expandMacros(pp, tempBuffer);
                int expLen = strlen(expandedPart);
                if (finalSize + expLen < finalMax - 1) {
                    strcpy(finalResult + finalSize, expandedPart);
                    finalSize += expLen;
                }
                tempSize = 0;
            }
            p++;
            if (!handleDirective(pp, &p)) {
                break;
            }
        } else if (*p == '$' && !pp->skipBlock) {
            // 二级宏调用，保留 $ 供词法分析器处理
            if (tempSize > 0) {
                tempBuffer[tempSize] = '\0';
                char *expandedPart = expandMacros(pp, tempBuffer);
                int expLen = strlen(expandedPart);
                if (finalSize + expLen < finalMax - 1) {
                    strcpy(finalResult + finalSize, expandedPart);
                    finalSize += expLen;
                }
                tempSize = 0;
            }
            // 复制 $ 和后续的标识符
            if (finalSize < finalMax - 1) {
                finalResult[finalSize++] = *p++;
            }
            while (*p && (isalnum(*p) || *p == '_') && finalSize < finalMax - 1) {
                finalResult[finalSize++] = *p++;
            }
        } else if (*p == '#' && pp->skipBlock) {
            skipToEOL(&p);
        } else if (pp->skipBlock) {
            skipToEOL(&p);
        } else {
            if (tempSize < 65535) {
                tempBuffer[tempSize++] = *p++;
            }
        }
    }

    if (tempSize > 0) {
        tempBuffer[tempSize] = '\0';
        char *expandedPart = expandMacros(pp, tempBuffer);
        int expLen = strlen(expandedPart);
        if (finalSize + expLen < finalMax - 1) {
            strcpy(finalResult + finalSize, expandedPart);
            finalSize += expLen;
        }
    }
    finalResult[finalSize] = '\0';
    return finalResult;
}
