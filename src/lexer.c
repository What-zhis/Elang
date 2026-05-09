#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lexer.h"
#include "preprocessor.h"

#define MAX_TOKEN_LEN 100

FILE *input;
int line = 1;
int column = 1;
static int hasSkippedBOM = 0;
static Preprocessor *currentPP = NULL;



Token getNextToken();
void skipWhitespace();
Token readIdentifier();
Token readNumber();
Token readString();
Token readChar();
Token readDir();
Token readDoubleSingleQuotedString();

void initLexer(FILE *newInput) {
    input = newInput;
    line = 1;
    column = 1;
    currentPP = NULL;
    hasSkippedBOM = 0;

    // 检查并跳过 UTF-8 BOM
    int c1 = fgetc(input);
    int c2 = fgetc(input);
    int c3 = fgetc(input);
    if (!(c1 == 0xEF && c2 == 0xBB && c3 == 0xBF)) {
        // 没有 BOM，将字符放回
        if (c3 != EOF) ungetc(c3, input);
        if (c2 != EOF) ungetc(c2, input);
        if (c1 != EOF) ungetc(c1, input);
    }
}

void setLexerPreprocessor(Preprocessor *pp) {
    currentPP = pp;
}

Token getNextToken() {
    skipWhitespace();

    int c = fgetc(input);
    column++;

    switch(c) {
        case EOF:
            return (Token){TOKEN_EOF, "", line, column};
        case '(':
            return (Token){TOKEN_LPAREN, "(", line, column};
        case ')':
            return (Token){TOKEN_RPAREN, ")", line, column};
        case '{':
            return (Token){TOKEN_LBRACE, "{", line, column};
        case '}':
            return (Token){TOKEN_RBRACE, "}", line, column};
        case '[':
            return (Token){TOKEN_LBRACKET, "[", line, column};
        case ']':
            return (Token){TOKEN_RBRACKET, "]", line, column};
        case ';':
            return (Token){TOKEN_SEMICOLON, ";", line, column};
        case ':':
            return (Token){TOKEN_COLON, ":", line, column};
        case ',':
            return (Token){TOKEN_COMMA, ",", line, column};
        case '+':
            return (Token){TOKEN_PLUS, "+", line, column};
        case '-':
            {
                int next = fgetc(input);
                column++;
                if (next == '>') {
                    return (Token){TOKEN_ARROW, "->", line, column};
                } else {
                    ungetc(next, input);
                    column--;
                    return (Token){TOKEN_MINUS, "-", line, column};
                }
            }
        case '*':
            return (Token){TOKEN_MUL, "*", line, column};
        case '/':
            {
                int next = fgetc(input);
                column++;
                if (next == '/') {
                    while (fgetc(input) != '\n' && !feof(input));
                    line++;
                    column = 1;
                    return getNextToken();
                } else if (next == '[') {
                    int depth = 1;
                    while (depth > 0 && !feof(input)) {
                        int c2 = fgetc(input);
                        column++;
                        if (c2 == '\n') {
                            line++;
                            column = 1;
                        } else if (c2 == ']') {
                            c2 = fgetc(input);
                            column++;
                            if (c2 == '/') {
                                depth--;
                            }
                        }
                    }
                    return getNextToken();
                } else {
                    ungetc(next, input);
                    column--;
                    return (Token){TOKEN_DIV, "/", line, column};
                }
            }
        case '=':
            {
                int next = fgetc(input);
                column++;
                if (next == '=') {
                    return (Token){TOKEN_EQ, "==", line, column};
                } else if (next == '>') {
                    return (Token){TOKEN_DOUBLE_ARROW, "=>", line, column};
                } else {
                    ungetc(next, input);
                    column--;
                    return (Token){TOKEN_ASSIGN, "=", line, column};
                }
            }
        case '!':
            {
                int next = fgetc(input);
                column++;
                if (next == '=') {
                    return (Token){TOKEN_NE, "!=", line, column};
                } else {
                    ungetc(next, input);
                    column--;
                    return (Token){TOKEN_NOT, "!", line, column};
                }
            }
        case '<':
            {
                int next = fgetc(input);
                column++;
                if (next == '=') {
                    return (Token){TOKEN_LE, "<=", line, column};
                } else {
                    ungetc(next, input);
                    column--;
                    return (Token){TOKEN_LT, "<", line, column};
                }
            }
        case '>':
            {
                int next = fgetc(input);
                column++;
                if (next == '=') {
                    return (Token){TOKEN_GE, ">=", line, column};
                } else {
                    ungetc(next, input);
                    column--;
                    return (Token){TOKEN_GT, ">", line, column};
                }
            }
        case '&':
            return (Token){TOKEN_AMPERSAND, "&", line, column};
        case '#':
            return (Token){TOKEN_HASH, "#", line, column};
        case '.':
            {
                int next1 = fgetc(input);
                int next2 = fgetc(input);
                if (next1 == '.' && next2 == '.') {
                    // 处理 ...
                    column += 3;
                    return (Token){TOKEN_ELLIPSIS, "...", line, column};
                } else {
                    ungetc(next2, input);
                    ungetc(next1, input);
                    column--;
                    return (Token){TOKEN_DOT, ".", line, column};
                }
            }
        case '"':
            return readString();
        case '\'':
            {
                int next = fgetc(input);
                if (next == '\'') {
                    column--;
                    return readDoubleSingleQuotedString();
                } else {
                    ungetc(next, input);
                    return readChar();
                }
            }
        default:
            if (c == 'C' || c == 'c') {
                // Check if it's a Windows path like C:\... or c:\...
                int next1 = fgetc(input);
                int next2 = fgetc(input);
                if (next1 == ':' && (next2 == '\\' || next2 == '/')) {
                    ungetc(next2, input);
                    ungetc(next1, input);
                    ungetc(c, input);
                    column -= 3;
                    return readDir();
                } else {
                    ungetc(next2, input);
                    ungetc(next1, input);
                    ungetc(c, input);
                    column -= 3;
                    return readIdentifier();
                }
            } else if (isalpha(c) || c == '_') {
                ungetc(c, input);
                column--;
                return readIdentifier();
            } else if (isdigit(c)) {
                ungetc(c, input);
                column--;
                return readNumber();
            } else {
                fprintf(stderr, "Error: Unexpected character '%c' at line %d, column %d\n", c, line, column);
                exit(1);
            }
    }
}

void skipWhitespace() {
    int c;
    while ((c = fgetc(input)) != EOF) {
        if (isspace(c)) {
            if (c == '\n') {
                line++;
                column = 1;
            } else if (c == '\r') {
                // 处理 CRLF 行结束符
                int next_c = fgetc(input);
                if (next_c == '\n') {
                    // CRLF 组合，只增加一行
                    line++;
                    column = 1;
                } else if (next_c != EOF) {
                    // 只有 CR，增加一行，并将下一个字符放回
                    ungetc(next_c, input);
                    line++;
                    column = 1;
                } else {
                    // CR 后是 EOF，只增加一行
                    line++;
                    column = 1;
                }
            } else {
                column++;
            }
        } else {
            ungetc(c, input);
            break;
        }
    }
}

Token readIdentifier() {
    Token token;
    token.line = line;
    token.column = column;

    int i = 0;
    int c;
    while ((c = fgetc(input)) != EOF && (isalnum(c) || c == '_')) {
        if (i < MAX_TOKEN_LEN - 1) {
            token.value[i++] = c;
        }
        column++;
    }
    token.value[i] = '\0';
    ungetc(c, input);

    if (strcmp(token.value, "import") == 0) {
        token.type = TOKEN_IMPORT;
    } else if (strcmp(token.value, "namespace") == 0) {
        token.type = TOKEN_NAMESPACE;
    } else if (strcmp(token.value, "eout") == 0) {
        token.type = TOKEN_OUT;
    } else if (strcmp(token.value, "ein") == 0) {
        token.type = TOKEN_IN;
    } else if (strcmp(token.value, "func") == 0) {
        token.type = TOKEN_FUNC;
    } else if (strcmp(token.value, "object") == 0) {
        token.type = TOKEN_OBJECT;
    } else if (strcmp(token.value, "return") == 0) {
        token.type = TOKEN_RETURN;
    } else if (strcmp(token.value, "for") == 0) {
        token.type = TOKEN_FOR;
    } else if (strcmp(token.value, "while") == 0) {
        token.type = TOKEN_WHILE;
    } else if (strcmp(token.value, "switch") == 0) {
        token.type = TOKEN_SWITCH;
    } else if (strcmp(token.value, "case") == 0) {
        token.type = TOKEN_CASE;
    } else if (strcmp(token.value, "default") == 0) {
        token.type = TOKEN_DEFAULT;
    } else if (strcmp(token.value, "break") == 0) {
        token.type = TOKEN_BREAK;
    } else if (strcmp(token.value, "if") == 0) {
        token.type = TOKEN_IF;
    } else if (strcmp(token.value, "else") == 0) {
        token.type = TOKEN_ELSE;
    } else if (strcmp(token.value, "endl") == 0) {
        token.type = TOKEN_ENDL;
    } else if (strcmp(token.value, "int") == 0) {
        token.type = TOKEN_INT;
    } else if (strcmp(token.value, "float") == 0) {
        token.type = TOKEN_FLOAT;
    } else if (strcmp(token.value, "double") == 0) {
        token.type = TOKEN_DOUBLE;
    } else if (strcmp(token.value, "dir") == 0) {
        token.type = TOKEN_DIR;
    } else if (strcmp(token.value, "string") == 0) {
        token.type = TOKEN_STRING_TYPE;
    } else if (strcmp(token.value, "char") == 0) {
        token.type = TOKEN_CHAR_TYPE;
    } else if (strcmp(token.value, "mem") == 0) {
        token.type = TOKEN_MEM;
    } else if (strcmp(token.value, "po") == 0) {
        token.type = TOKEN_PO;
    } else if (strcmp(token.value, "alloc") == 0) {
        token.type = TOKEN_ALLOC;
    } else if (strcmp(token.value, "free") == 0) {
        token.type = TOKEN_FREE;
    } else if (strcmp(token.value, "run") == 0) {
        token.type = TOKEN_RUN;
    } else if (strcmp(token.value, "asm") == 0) {
        token.type = TOKEN_ASM;
    } else if (strcmp(token.value, "macro") == 0) {
        token.type = TOKEN_MACRO;
    } else if (strcmp(token.value, "include") == 0) {
        token.type = TOKEN_INCLUDE;
    } else if (strcmp(token.value, "defines") == 0) {
        token.type = TOKEN_DEFINES;
    } else if (strcmp(token.value, "undef") == 0) {
        token.type = TOKEN_UNDEF;
    } else if (strcmp(token.value, "struct") == 0) {
        token.type = TOKEN_ID;
    } else if (strcmp(token.value, "enum") == 0) {
        token.type = TOKEN_ID;
    } else {
        // 检查是否是二级宏
        if (currentPP != NULL) {
            SecondaryMacro *sm = findSecondaryMacro(currentPP, token.value, -1);
            if (sm && sm->isActive) {
                token.type = TOKEN_SECONDARY_MACRO;
            } else {
                token.type = TOKEN_ID;
            }
        } else {
            token.type = TOKEN_ID;
        }
    }

    return token;
}

Token readDoubleSingleQuotedString() {
    Token token;
    token.type = TOKEN_STRING;
    token.line = line;
    token.column = column;

    int i = 0;
    int c;
    while ((c = fgetc(input)) != EOF) {
        if (c == '\r') {
            int next_c = fgetc(input);
            if (next_c == '\n') {
                line++;
                column = 0;
            } else if (next_c != EOF) {
                ungetc(next_c, input);
            }
        } else if (c == '\'') {
            int next_c = fgetc(input);
            if (next_c == '\'') {
                token.value[i] = '\0';
                return token;
            } else {
                if (i < MAX_TOKEN_LEN - 1) {
                    token.value[i++] = c;
                }
                if (next_c != EOF) {
                    if (next_c == '\n') {
                        line++;
                        column = 0;
                    } else if (next_c != '\r') {
                        column++;
                    }
                    if (i < MAX_TOKEN_LEN - 1) {
                        token.value[i++] = next_c;
                    }
                }
            }
        } else {
            if (c == '\n') {
                line++;
                column = 0;
            } else {
                column++;
            }
            if (i < MAX_TOKEN_LEN - 1) {
                token.value[i++] = c;
            }
        }
    }
    token.value[i] = '\0';
    return token;
}

Token readDir() {
    Token token;
    token.type = TOKEN_DIR;
    token.line = line;
    token.column = column;

    int i = 0;
    int c;

    while ((c = fgetc(input)) != EOF) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            // 空格分隔，路径结束
            ungetc(c, input);
            column--;
            break;
        } else if (c == ';' || c == '&' || c == '|' || c == '>' || c == '<' || c == '=' || c == '(' || c == ')' || c == '+' || c == '-') {
            // 路径结束，将字符放回去
            ungetc(c, input);
            column--;
            break;
        } else if (c == '\\') {
            // 处理反斜杠
            if (i < MAX_TOKEN_LEN - 1) {
                token.value[i++] = '\\';
                token.value[i++] = '\\'; // 转义反斜杠
            }
            column++;
        } else if (c == '/') {
            // 保留正斜杠（C语言可以处理）
            if (i < MAX_TOKEN_LEN - 1) {
                token.value[i++] = '/';
            }
            column++;
        } else if (c == '{') {
            // 检查是否是 {{ 插值表达式
            int next_c = fgetc(input);
            if (next_c == '{') {
                // 处理插值表达式 {{...}}
                if (i < MAX_TOKEN_LEN - 1) {
                    token.value[i++] = '{';
                    token.value[i++] = '{';
                }
                column += 2;

                // 读取插值表达式内容
                while ((c = fgetc(input)) != EOF) {
                    if (c == '{') {
                        // 嵌套的 {，直接添加
                        if (i < MAX_TOKEN_LEN - 1) {
                            token.value[i++] = c;
                            column++;
                        }
                    } else if (c == '}') {
                        // 检查是否是 }}
                        int next_next_c = fgetc(input);
                        if (next_next_c == '}') {
                            // 找到插值表达式的结束 }}
                            if (i < MAX_TOKEN_LEN - 1) {
                                token.value[i++] = '}';
                                token.value[i++] = '}';
                            }
                            column += 2;
                            break;
                        } else {
                            // 不是 }}，将字符放回去
                            if (i < MAX_TOKEN_LEN - 1) {
                                token.value[i++] = c;
                                token.value[i++] = next_next_c;
                                column += 2;
                            }
                        }
                    } else {
                        if (i < MAX_TOKEN_LEN - 1) {
                            token.value[i++] = c;
                        }
                        column++;
                    }
                }
            } else {
                // 不是 {{，将字符放回去
                ungetc(next_c, input);
                if (i < MAX_TOKEN_LEN - 1) {
                    token.value[i++] = c;
                }
            }
        } else {
            if (i < MAX_TOKEN_LEN - 1) {
                token.value[i++] = c;
            }
            column++;
        }
    }
    token.value[i] = '\0';

    return token;
}

Token readNumber() {
    Token token;
    token.line = line;
    token.column = column;

    int i = 0;
    int c;
    int hasDecimal = 0;
    while ((c = fgetc(input)) != EOF && (isdigit(c) || c == '.')) {
        if (c == '.') {
            hasDecimal = 1;
        }
        if (i < MAX_TOKEN_LEN - 1) {
            token.value[i++] = c;
        }
        column++;
    }
    token.value[i] = '\0';
    ungetc(c, input);
    column--;

    if (hasDecimal) {
        token.type = TOKEN_FLOAT_NUMBER;
    } else {
        token.type = TOKEN_NUMBER;
    }

    return token;
}

Token readString() {
    Token token;
    token.type = TOKEN_STRING;
    token.line = line;
    token.column = column;

    int i = 0;
    int c;
    while ((c = fgetc(input)) != EOF && c != '"') {
        column++;
        if (c == '\\') {
            int next_c = fgetc(input);
            column++;
            if (next_c == EOF) break;
            // 处理转义序列
            switch (next_c) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case '\\': c = '\\'; break;
                case '"': c = '"'; break;
                default: 
                    // 未知转义，保留原样
                    if (i < MAX_TOKEN_LEN - 1) token.value[i++] = '\\';
                    c = next_c;
            }
        }
        if (i < MAX_TOKEN_LEN - 1) {
            token.value[i++] = c;
        }
    }
    token.value[i] = '\0';
    
    return token;
}

Token readStringOld() {
    Token token;
    token.type = TOKEN_STRING;
    token.line = line;
    token.column = column;

    int i = 0;
    int c;
    column++;  // 跳过开始的 "
    while ((c = fgetc(input)) != EOF && c != '"') {
        column++;
        if (c == '\\' && (c = fgetc(input)) != EOF) {
            column++;
            if (i < MAX_TOKEN_LEN - 1) {
                token.value[i++] = '\\';
            }
            if (i < MAX_TOKEN_LEN - 1) {
                token.value[i++] = c;
            }
        } else if (c == '{') {
            // 检查是否是 {{ 插值表达式
            int next_c = fgetc(input);
            if (next_c == '{') {
                // 保留 {{ 在token中
                if (i < MAX_TOKEN_LEN - 1) {
                    token.value[i++] = '{';
                    token.value[i++] = '{';
                }
                column++;

                // 读取插值表达式内容
                while ((c = fgetc(input)) != EOF) {
                    if (c == '{') {
                        // 嵌套的 {，直接添加
                        if (i < MAX_TOKEN_LEN - 1) {
                            token.value[i++] = c;
                        }
                    } else if (c == '}') {
                        // 检查是否是 }}
                        int next_next_c = fgetc(input);
                        if (next_next_c == '}') {
                            // 找到插值表达式的结束 }}
                            if (i < MAX_TOKEN_LEN - 1) {
                                token.value[i++] = '}';
                                token.value[i++] = '}';
                            }
                            break;
                        } else {
                            // 不是 }}，将字符放回去
                            if (i < MAX_TOKEN_LEN - 1) {
                                token.value[i++] = c;
                                token.value[i++] = next_next_c;
                            }
                        }
                    } else {
                        if (i < MAX_TOKEN_LEN - 1) {
                            token.value[i++] = c;
                        }
                    }
                }
            } else {
                // 不是 {{，将字符放回去
                ungetc(next_c, input);
                if (i < MAX_TOKEN_LEN - 1) {
                    token.value[i++] = c;
                }
            }
        } else {
            if (i < MAX_TOKEN_LEN - 1) {
                token.value[i++] = c;
            }
        }
    }
    token.value[i] = '\0';
    
    // 读取并跳过结束的 "
    fgetc(input);
    column++;
    
    return token;
}

Token readChar() {
    Token token;
    token.type = TOKEN_CHAR;
    token.line = line;
    token.column = column;

    int c = fgetc(input);
    if (c == '\r') {
        c = fgetc(input);
        if (c == '\n') {
            line++;
            column = 1;
        } else if (c != EOF) {
            ungetc(c, input);
        }
    }
    column++;
    if (c == '\\' && (c = fgetc(input)) != EOF) {
        switch(c) {
            case 'n': token.value[0] = '\n'; break;
            case 't': token.value[0] = '\t'; break;
            case '\'': token.value[0] = '\''; break;
            case '\\': token.value[0] = '\\'; break;
            default: token.value[0] = c; break;
        }
        column++;
    } else {
        token.value[0] = c;
    }

    fgetc(input);
    column++;
    token.value[1] = '\0';

    return token;
}