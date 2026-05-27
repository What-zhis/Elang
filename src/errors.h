#ifndef ERRORS_H
#define ERRORS_H

#include <stdio.h>
#include <stdlib.h>

// 错误类型枚举
typedef enum {
    // 词法错误 (L001-L099)
    ERR_LEX_INVALID_CHAR = 1,           // L001: 非法字符
    ERR_LEX_UNCLOSED_STRING = 2,        // L002: 未闭合的字符串
    ERR_LEX_INVALID_NUMBER = 3,         // L003: 无效的数字格式
    ERR_LEX_UNCLOSED_COMMENT = 4,       // L004: 未闭合的注释
    
    // 语法错误 (S001-S099)
    ERR_SYNTAX_UNEXPECTED_TOKEN = 101,  // S001: 意外的 token
    ERR_SYNTAX_MISSING_SEMICOLON = 102, // S002: 缺少分号
    ERR_SYNTAX_MISSING_PAREN = 103,     // S003: 缺少括号
    ERR_SYNTAX_MISSING_BRACE = 104,     // S004: 缺少大括号
    ERR_SYNTAX_INVALID_EXPR = 105,      // S005: 无效的表达式
    ERR_SYNTAX_INVALID_STMT = 106,      // S006: 无效的语句
    ERR_SYNTAX_MISSING_COLON = 107,     // S007: 缺少冒号
    ERR_SYNTAX_INVALID_FUNCTION = 108,  // S008: 无效的函数声明
    
    // 语义错误 (E001-E099)
    ERR_SEM_UNDEFINED_VAR = 201,        // E001: 未定义的变量
    ERR_SEM_UNDEFINED_FUNC = 202,       // E002: 未定义的函数
    ERR_SEM_DUPLICATE_VAR = 203,        // E003: 重复定义的变量
    ERR_SEM_DUPLICATE_FUNC = 204,       // E004: 重复定义的函数
    ERR_SEM_TYPE_MISMATCH = 205,        // E005: 类型不匹配
    ERR_SEM_INVALID_OP = 206,           // E006: 无效的操作
    ERR_SEM_INVALID_ASSIGN = 207,        // E007: 无效的赋值
    ERR_SEM_ARG_COUNT = 208,            // E008: 参数数量不匹配
    ERR_SEM_INVALID_TYPE = 209,         // E009: 无效的类型
    ERR_SEM_DIVIDE_BY_ZERO = 210,       // E010: 除零错误
    ERR_SEM_CIRCULAR_IMPORT = 211,      // E011: 循环导入
    ERR_SEM_NULL_POINTER = 212,         // E012: 空指针解引用
    
    // 代码生成错误 (C001-C099)
    ERR_CODE_INVALID_INSTR = 301,        // C001: 无效的指令
    ERR_CODE_UNSUPPORTED = 302,          // C002: 不支持的功能
    ERR_CODE_MEMORY_ERROR = 303,         // C003: 内存分配错误
    
    // 警告 (W001-W099)
    WARN_UNUSED_VAR = 401,              // W001: 未使用的变量
    WARN_UNUSED_PARAM = 402,            // W002: 未使用的参数
    WARN_UNREACHABLE_CODE = 403,        // W003: 不可达代码
    WARN_UNINITIALIZED_VAR = 404,       // W004: 未初始化的变量
    WARN_IMPLICIT_CONVERSION = 405,      // W005: 隐式类型转换
    
    ERR_UNKNOWN = 999                   // 未知错误
} ErrorCode;

// 错误级别
typedef enum {
    ERROR_LEVEL_ERROR,
    ERROR_LEVEL_WARNING,
    ERROR_LEVEL_INFO
} ErrorLevel;

// 错误信息结构
typedef struct {
    ErrorCode code;
    ErrorLevel level;
    const char *message;
    const char *suggestion;
} ErrorInfo;

// 全局错误计数
extern int errorCount;
extern int warningCount;

// 颜色输出开关
extern int useColorOutput;

// 错误处理函数
void reportError(ErrorCode code, const char *filename, int line, int column, const char *detail);
void reportWarning(ErrorCode code, const char *filename, int line, int column, const char *detail);
void printErrorSummary(void);
const char *getErrorCodeString(ErrorCode code);
const char *getErrorCategory(ErrorCode code);

// 颜色代码
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_RESET   "\x1b[0m"

#endif
