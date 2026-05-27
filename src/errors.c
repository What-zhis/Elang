#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "errors.h"

extern int errorCount;
int warningCount = 0;
int useColorOutput = 1;

static ErrorInfo errorTable[] = {
    // 词法错误
    {ERR_LEX_INVALID_CHAR, ERROR_LEVEL_ERROR, "非法字符", "请检查输入的字符是否符合语言规范"},
    {ERR_LEX_UNCLOSED_STRING, ERROR_LEVEL_ERROR, "未闭合的字符串字面量", "请确保字符串有匹配的引号"},
    {ERR_LEX_INVALID_NUMBER, ERROR_LEVEL_ERROR, "无效的数字格式", "请检查数字的格式是否正确"},
    {ERR_LEX_UNCLOSED_COMMENT, ERROR_LEVEL_ERROR, "未闭合的注释", "请确保注释有正确的结束标记"},
    
    // 语法错误
    {ERR_SYNTAX_UNEXPECTED_TOKEN, ERROR_LEVEL_ERROR, "意外的 token", "请检查语法是否正确"},
    {ERR_SYNTAX_MISSING_SEMICOLON, ERROR_LEVEL_ERROR, "缺少分号", "请在语句末尾添加分号"},
    {ERR_SYNTAX_MISSING_PAREN, ERROR_LEVEL_ERROR, "缺少括号", "请检查括号是否匹配"},
    {ERR_SYNTAX_MISSING_BRACE, ERROR_LEVEL_ERROR, "缺少大括号", "请检查大括号是否匹配"},
    {ERR_SYNTAX_INVALID_EXPR, ERROR_LEVEL_ERROR, "无效的表达式", "请检查表达式语法"},
    {ERR_SYNTAX_INVALID_STMT, ERROR_LEVEL_ERROR, "无效的语句", "请检查语句语法"},
    {ERR_SYNTAX_MISSING_COLON, ERROR_LEVEL_ERROR, "缺少冒号", "请在适当位置添加冒号"},
    {ERR_SYNTAX_INVALID_FUNCTION, ERROR_LEVEL_ERROR, "无效的函数声明", "请检查函数语法"},
    
    // 语义错误
    {ERR_SEM_UNDEFINED_VAR, ERROR_LEVEL_ERROR, "未定义的变量", "请确保变量在使用前已声明"},
    {ERR_SEM_UNDEFINED_FUNC, ERROR_LEVEL_ERROR, "未定义的函数", "请确保函数已声明或已导入"},
    {ERR_SEM_DUPLICATE_VAR, ERROR_LEVEL_ERROR, "重复定义的变量", "请使用不同的变量名"},
    {ERR_SEM_DUPLICATE_FUNC, ERROR_LEVEL_ERROR, "重复定义的函数", "请使用不同的函数名"},
    {ERR_SEM_TYPE_MISMATCH, ERROR_LEVEL_ERROR, "类型不匹配", "请确保操作数类型兼容"},
    {ERR_SEM_INVALID_OP, ERROR_LEVEL_ERROR, "无效的操作", "请检查操作是否合法"},
    {ERR_SEM_INVALID_ASSIGN, ERROR_LEVEL_ERROR, "无效的赋值", "请检查赋值目标是否正确"},
    {ERR_SEM_ARG_COUNT, ERROR_LEVEL_ERROR, "参数数量不匹配", "请检查函数调用的参数数量"},
    {ERR_SEM_INVALID_TYPE, ERROR_LEVEL_ERROR, "无效的类型", "请使用有效的类型名称"},
    {ERR_SEM_DIVIDE_BY_ZERO, ERROR_LEVEL_ERROR, "除零错误", "请确保除数不为零"},
    {ERR_SEM_CIRCULAR_IMPORT, ERROR_LEVEL_ERROR, "循环导入", "请检查 import 语句的依赖关系"},
    {ERR_SEM_NULL_POINTER, ERROR_LEVEL_ERROR, "空指针解引用", "请确保指针不为空"},
    
    // 代码生成错误
    {ERR_CODE_INVALID_INSTR, ERROR_LEVEL_ERROR, "无效的指令", "请检查代码生成逻辑"},
    {ERR_CODE_UNSUPPORTED, ERROR_LEVEL_ERROR, "不支持的功能", "该功能尚未实现"},
    {ERR_CODE_MEMORY_ERROR, ERROR_LEVEL_ERROR, "内存分配错误", "请检查内存使用情况"},
    
    // 警告
    {WARN_UNUSED_VAR, ERROR_LEVEL_WARNING, "未使用的变量", "可以删除该变量或使用它"},
    {WARN_UNUSED_PARAM, ERROR_LEVEL_WARNING, "未使用的参数", "可以删除该参数或使用它"},
    {WARN_UNREACHABLE_CODE, ERROR_LEVEL_WARNING, "不可达代码", "该代码永远不会被执行"},
    {WARN_UNINITIALIZED_VAR, ERROR_LEVEL_WARNING, "未初始化的变量", "请在使用前初始化变量"},
    {WARN_IMPLICIT_CONVERSION, ERROR_LEVEL_WARNING, "隐式类型转换", "请考虑显式转换"},
    
    {ERR_UNKNOWN, ERROR_LEVEL_ERROR, "未知错误", "请联系开发者"}
};

static int getErrorIndex(ErrorCode code) {
    for (int i = 0; i < sizeof(errorTable) / sizeof(errorTable[0]); i++) {
        if (errorTable[i].code == code) {
            return i;
        }
    }
    return sizeof(errorTable) / sizeof(errorTable[0]) - 1;
}

const char *getErrorCodeString(ErrorCode code) {
    static char buffer[20];
    if (code >= 1 && code <= 99) {
        snprintf(buffer, sizeof(buffer), "L%03d", code);
    } else if (code >= 101 && code <= 199) {
        snprintf(buffer, sizeof(buffer), "S%03d", code - 100);
    } else if (code >= 201 && code <= 299) {
        snprintf(buffer, sizeof(buffer), "E%03d", code - 200);
    } else if (code >= 301 && code <= 399) {
        snprintf(buffer, sizeof(buffer), "C%03d", code - 300);
    } else if (code >= 401 && code <= 499) {
        snprintf(buffer, sizeof(buffer), "W%03d", code - 400);
    } else {
        snprintf(buffer, sizeof(buffer), "ERR%03d", code);
    }
    return buffer;
}

const char *getErrorCategory(ErrorCode code) {
    if (code >= 1 && code <= 99) {
        return "词法错误";
    } else if (code >= 101 && code <= 199) {
        return "语法错误";
    } else if (code >= 201 && code <= 299) {
        return "语义错误";
    } else if (code >= 301 && code <= 399) {
        return "代码生成错误";
    } else if (code >= 401 && code <= 499) {
        return "警告";
    }
    return "未知错误";
}

void reportError(ErrorCode code, const char *filename, int line, int column, const char *detail) {
    int index = getErrorIndex(code);
    ErrorInfo *info = &errorTable[index];
    
    const char *levelColor = "";
    const char *levelText = "";
    
    if (useColorOutput) {
        switch (info->level) {
            case ERROR_LEVEL_ERROR:
                levelColor = COLOR_RED;
                break;
            case ERROR_LEVEL_WARNING:
                levelColor = COLOR_YELLOW;
                break;
            case ERROR_LEVEL_INFO:
                levelColor = COLOR_BLUE;
                break;
        }
    }
    
    switch (info->level) {
        case ERROR_LEVEL_ERROR:
            levelText = "error";
            errorCount++;
            break;
        case ERROR_LEVEL_WARNING:
            levelText = "warning";
            warningCount++;
            break;
        case ERROR_LEVEL_INFO:
            levelText = "info";
            break;
    }
    
    fprintf(stderr, "%s%s:%d:%d: %s %s: %s", 
            useColorOutput ? COLOR_CYAN : "",
            filename ? filename : "<unknown>", 
            line, 
            column,
            levelColor,
            levelText,
            info->message);
    
    if (detail && detail[0] != '\0') {
        fprintf(stderr, ": %s", detail);
    }
    
    fprintf(stderr, "%s\n", useColorOutput ? COLOR_RESET : "");
    
    if (info->suggestion && info->suggestion[0] != '\0') {
        fprintf(stderr, "%s    提示: %s%s\n", 
                useColorOutput ? COLOR_GREEN : "",
                info->suggestion,
                useColorOutput ? COLOR_RESET : "");
    }
}

void reportWarning(ErrorCode code, const char *filename, int line, int column, const char *detail) {
    reportError(code, filename, line, column, detail);
}

void printErrorSummary(void) {
    if (errorCount > 0 || warningCount > 0) {
        fprintf(stderr, "\n%s编译完成:%s %d 个错误, %d 个警告\n",
                useColorOutput ? COLOR_MAGENTA : "",
                useColorOutput ? COLOR_RESET : "",
                errorCount,
                warningCount);
    }
}
