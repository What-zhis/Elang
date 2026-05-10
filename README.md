# E语言编译器

一个使用C语言实现的E语言编译器，支持变量声明、函数定义、控制流、输入输出、类型转换、结构体、枚举、指针、内存管理、源文件导入、内联汇编和强大的两级宏系统等功能。编译器可以生成C代码或NASM汇编代码。

## 目录

- [E语言编译器](#e语言编译器)
  - [目录](#目录)
  - [1. 快速开始](#1-快速开始)
    - [1.1 编译编译器](#11-编译编译器)
    - [1.2 基本用法](#12-基本用法)
    - [1.3 命令行参数](#13-命令行参数)
  - [2. 语言语法](#2-语言语法)
    - [2.1 注释](#21-注释)
    - [2.2 变量声明](#22-变量声明)
    - [2.3 数据类型](#23-数据类型)
    - [2.4 函数定义](#24-函数定义)
    - [2.5 控制流](#25-控制流)
    - [2.6 输入输出](#26-输入输出)
    - [2.7 类型转换](#27-类型转换)
    - [2.8 字符串操作](#28-字符串操作)
    - [2.9 指针和内存管理](#29-指针和内存管理)
    - [2.10 结构体和枚举](#210-结构体和枚举)
    - [2.11 源文件导入](#211-源文件导入)
    - [2.12 内联汇编](#212-内联汇编)
    - [2.13 字符串插值](#213-字符串插值)
    - [2.14 文件I/O](#214-file-io)
    - [2.15 宏系统](#215-宏系统)
  - [3. 操作符参考](#3-操作符参考)
    - [3.1 算术操作符](#31-算术操作符)
    - [3.2 比较操作符](#32-比较操作符)
    - [3.3 逻辑操作符](#33-逻辑操作符)
    - [3.4 赋值操作符](#34-赋值操作符)
    - [3.5 字符串操作符](#35-字符串操作符)
    - [3.6 指针操作符](#36-指针操作符)
  - [4. 编译器行为](#4-编译器行为)
    - [4.1 编译流程](#41-编译流程)
    - [4.2 符号表管理](#42-符号表管理)
    - [4.3 错误处理机制](#43-错误处理机制)
    - [4.4 代码生成策略](#44-代码生成策略)
  - [5. 边缘情况处理](#5-边缘情况处理)
    - [5.1 变量相关](#51-变量相关)
    - [5.2 函数相关](#52-函数相关)
    - [5.3 类型转换](#53-类型转换)
    - [5.4 内存管理](#54-内存管理)
    - [5.5 控制流](#55-控制流)
    - [5.6 字符串处理](#56-字符串处理)
    - [5.7 文件I/O](#57-file-io)
    - [5.8 导入机制](#58-导入机制)
  - [6. 平台兼容性](#6-平台兼容性)
  - [7. VSCode插件](#7-vscode插件)
    - [7.1 功能特性](#71-功能特性)
    - [7.2 安装方法](#72-安装方法)
    - [7.3 打包发布](#73-打包发布)
  - [8. 示例程序](#8-示例程序)
    - [8.1 Hello World](#81-hello-world)
    - [8.2 斐波那契数列](#82-斐波那契数列)
    - [8.3 内存管理示例](#83-内存管理示例)
    - [8.4 内联汇编示例](#84-内联汇编示例)
  - [9. 常见问题](#9-常见问题)
  - [10. 特性实现状态](#10-特性实现状态)

---

## 1. 快速开始

### 1.1 编译编译器

```bash
gcc -o ecompiler.exe src/main.c src/lexer.c src/parser.c src/codegen.c src/codegen_nasm.c src/preprocessor.c
```

### 1.2 基本用法

```bash
# 编译并运行（默认行为，生成C代码）
./ecompiler.exe <源文件.e>

# 编译并运行（显式指定）
./ecompiler.exe --run <源文件.e>

# 仅编译，不运行
./ecompiler.exe --compile <源文件.e>

# 仅检查错误，不编译或运行
./ecompiler.exe --debug <源文件.e>

# 生成NASM汇编代码并编译运行
./ecompiler.exe --nasm <源文件.e>
```

### 1.3 命令行参数

| 参数 | 类型 | 描述 |
|------|------|------|
| `--run` | 标志 | 编译并运行程序（默认行为） |
| `--compile` | 标志 | 仅编译，不运行 |
| `--debug` | 标志 | 仅检查语法错误，不生成代码 |
| `--c` | 标志 | 生成C代码 |
| `--nasm` | 标志 | 生成NASM汇编代码 |
| `--ir` | 标志 | 生成中间表示（IR）代码 |
| `--machine` | 标志 | 直接生成机器码（x86-64 Windows，不需要外部工具） |
| `--windows` | 标志 | 指定目标平台为Windows（可与其他参数组合使用） |
| `<源文件.e>` | 字符串 | 必需参数，指定输入的E语言源文件 |

#### 参数组合

| 组合示例 | 行为 |
|---------|------|
| `--run --c` | 生成C代码，编译并运行 |
| `--compile --nasm` | 生成NASM汇编并编译，但不运行 |
| `--machine --windows` | 直接生成Windows x86-64可执行文件（独立，不需要外部工具） |
| `--debug --ir` | 检查语法错误（IR模式） |

---

## 2. 语言语法

### 2.1 注释

E语言支持两种注释方式：

```e
// 单行注释

/* 多行注释
   可以跨越多行 */

/[ 另一种多行注释
   语法 ]/
```

### 2.2 变量声明

变量声明语法：`类型 变量名 [= 值];`

```e
// 基本变量声明
int a = 10;
float b = 3.14;
double c = 2.718;
string d = "Hello World";
char e = 'A';
dir f = "C:\\Test\\file.txt";

// 无初始值声明
int x;
float y;
```

#### 变量命名规则

- 变量名必须以字母或下划线开头
- 变量名只能包含字母、数字和下划线
- 变量名区分大小写
- 不能使用关键字作为变量名

### 2.3 数据类型

| 类型 | 描述 | 大小（字节） | C等价类型 |
|------|------|------------|-----------|
| `int` | 有符号整数 | 4 | `int` |
| `float` | 单精度浮点数 | 4 | `float` |
| `double` | 双精度浮点数 | 8 | `double` |
| `string` | 字符串 | 可变 | `char*` |
| `char` | 单个字符 | 1 | `char` |
| `dir` | 文件路径 | 可变 | `char*` |
| `po <type>` | 指向指定类型的指针 | 8 (64位) | `type*` |
| `mem` | 内存块 | 可变 | `void*` |

### 2.4 函数定义

**语法：** `func [返回类型] 函数名(参数列表) { 函数体 } [=> 别名];`

```e
// 带返回类型的函数
func float addFloat(float a, float b) {
    return a + b;
}

// 无返回类型（默认为int）
func addInt(int a, int b) {
    return a + b;
}

// 无参数函数
func sayHello() {
    eout >namespace std "Hello!" & endl;
}

// 函数别名
func subtract(int x, int y) {
    return x - y;
} => sub;  // 注意：这里要加分号
```

#### 函数调用

```e
float result1 = addFloat(1.5, 2.5);
int result2 = addInt(10, 20);
int result3 = sub(5, 3);  // 使用函数别名
```

#### 参数规则

| 规则 | 说明 |
|------|------|
| 参数顺序 | 从左到右依次传递 |
| 参数类型 | 必须显式声明 |
| 参数数量 | 无限制 |
| 参数命名 | 遵循变量命名规则 |

#### 返回值规则

| 情况 | 行为 |
|------|------|
| 显式返回 | 使用 `return` 语句 |
| 无return语句 | 默认返回0 |
| 返回类型不匹配 | 编译器警告，隐式类型转换 |

### 2.5 控制流

#### if-else 语句

```e
if (x > 0) {
    eout >namespace std "Positive";
} else if (x < 0) {
    eout >namespace std "Negative";
} else {
    eout >namespace std "Zero";
}
```

#### while 循环

```e
int i = 0;
while (i < 10) {
    eout >namespace std string(i) & endl;
    i = i + 1;
}
```

#### for 循环

```e
int sum = 0;
for (int i = 0; i < 10; i++) {
    sum = sum + i;
}
```

#### switch-case 语句

```e
func main() {
    int x = 1;
    switch (x) {
        case 1:
            eout >namespace std "Case 1 matched!";
            break;
        case 2:
            eout >namespace std "Case 2 matched!";
            break;
        default:
            eout >namespace std "Default case!";
    }
    eout >namespace std "Done!";
}
```

**switch-case特性：**
- 支持 `case` 分支匹配常量值
- 支持 `default` 分支处理未匹配的情况
- 每个 `case` 需要用 `break` 终止，否则会执行下一个分支（穿透）
- `case` 中的值必须是常量（不能是变量）

**break语句：**
```e
func main() {
    for (int i = 0; i < 10; i++) {
        if (i == 5) {
            break;  // 退出循环
        }
        eout >namespace std string(i) & ", ";
    }
    // 输出: 0, 1, 2, 3, 4,
}
```

### 2.6 输入输出

#### 输出

```e
// 输出到标准输出
eout >namespace std "Hello World" & endl;
eout >namespace std "Number: " & string(42) & endl;

// 输出多个值
eout >namespace std "Name: " & name & ", Age: " & string(age) & endl;
```

#### 输入

```e
// 从标准输入读取
string name;
ein >namespace std "Enter your name: " => name;

int age;
ein >namespace std "Enter your age: " => age;
```

### 2.7 类型转换

E语言支持显式类型转换：

```e
// 整数转字符串
string(123);      // 返回 "123"
string(45.67);    // 返回 "45.67"

// 字符串转整数
int("123");       // 返回 123
int("45.67");     // 返回 45（截断小数）

// 整数转字符
char(65);         // 返回 'A'

// 字符串转路径
dir("C:\\Path");  // 返回路径字符串
```

### 2.8 字符串操作

#### 字符串拼接

使用 `&` 操作符拼接字符串：

```e
string greeting = "Hello" & " " & "World";
string info = "Name: " & name & ", Age: " & string(age);
```

#### 字符串插值

使用 `{{表达式}}` 语法在字符串中进行插值：

```e
int x = 42;
string message = "The answer is {{string(x)}}";  // 编译时生成 "The answer is 42"

dir path = "test_{{string(x)}}.txt";  // 生成 "test_42.txt"
```

### 2.9 指针和内存管理

#### 指针类型声明

```e
// 声明一个指向整数的指针
po int ptr;

// 声明一个指向结构体的指针
po Point ptr2;

// 声明一个内存块
mem buffer;
```

#### 内存分配与释放

```e
// 分配1024字节内存
mem buffer = alloc(1024);

// 分配一个 int 大小的内存，并将指针指向它
po int ptr = alloc(int);

// 释放内存
free(buffer);
free(ptr);
```

#### 指针操作

```e
// 解引用
ptr.value = 100;    // 相当于 C 语言的 *ptr = 100
int a = ptr.value;  // 相当于 C 语言的 a = *ptr

// 取地址
int var = 10;
po int ptr = &var;   // ptr 指向 var

// 指针运算
po int start = &arr[0];
po int end = start + 10;  // 向后移动10个int

// 内存块转字符串
mem buffer = alloc(50);
string content = string(buffer); // 将内存内容视为字符串
```

#### 内存安全

```e
func test() {
    mem buf = alloc(10);   // buf 拥有内存
    mem buf2 = buf;        // 所有权转移给 buf2, buf 自动变为 null
    free(buf2);            // 正确释放
} // 编译成功，因为 buf2 已经被释放了
```

### 2.10 结构体和枚举

#### 枚举

```e
enum Color {
    RED,      // 值为 0
    GREEN,    // 值为 1
    BLUE = 5, // 值为 5
    YELLOW    // 值为 6
};
```

#### 结构体

```e
struct Point {
    int x;
    int y;
};

struct Rectangle {
    Point topLeft;
    Point bottomRight;
};
```

#### 使用结构体

```e
Point p1;
p1.x = 10;
p1.y = 20;

Rectangle rect;
rect.topLeft.x = 0;
rect.topLeft.y = 0;
rect.bottomRight.x = 100;
rect.bottomRight.y = 200;
```

### 2.11 源文件导入（import）

使用 `import` 语句可以引入其他 `.e` 源文件。被导入文件中的函数和变量可以在当前文件中直接使用。

**语法：**

```e
import "filename.e";
```

**示例：**

创建 `utils.e` 文件：
```e
// utils.e
int global_var = 100;

func say_hello() {
    eout >namespace std "Hello from utils.e!" & "\n";
}

func add(int a, int b) {
    return a + b;
}
```

在主文件中使用：
```e
import "utils.e";

func main() {
    say_hello();  // 调用 utils.e 中的函数
    int result = add(10, 20);  // 使用函数
    eout >namespace std "Result: " & string(result) & "\n";
}
```

### 2.12 内联汇编

E语言支持内联汇编，可以直接编写NASM风格的汇编代码。

**语法：**

```e
asm(''
    ; 汇编代码
'')[索引]
```

**特性：**

- 使用 `''` 双单引号包围汇编代码（支持多行）
- 虚拟通讯寄存器 `xl` 和 `xh` 用于在汇编和E语言之间传递数据
- 可以调用E语言函数和内置函数
- 返回值是一个数组：`[xl的值, xh的值]`

**示例：**

```e
func main() {
    // 基本汇编操作
    int result = asm(''
        mov rax, 42;
        mov rbx, 10;
        add rax, rbx;
    '');
    eout >namespace std string(result) & endl;  // 输出 52
    
    // 使用虚拟寄存器
    asm(''
        mov xl, 100;   // 存储到 xl
        mov xh, 200;   // 存储到 xh
    '');
    
    int a = asm(''mov rax, xl;'');  // 读取 xl 的值
    int b = asm(''mov rax, xh;'');  // 读取 xh 的值
    eout >namespace std string(a) & ", " & string(b) & endl;  // 输出 100, 200
    
    // 使用数组索引访问返回值
    int xl_val = asm(''mov xl, "Hello";'')[0];  // 获取 xl 的值
    eout >namespace std xl_val & endl;  // 输出 Hello
}
```

**汇编指令支持：**

| 类别 | 指令 |
|------|------|
| 数据传输 | `mov`, `push`, `pop`, `lea` |
| 算术运算 | `add`, `sub`, `mul`, `div`, `imul`, `idiv` |
| 逻辑运算 | `and`, `or`, `xor`, `not` |
| 控制流 | `jmp`, `je`, `jne`, `cmp`, `call`, `ret` |

**虚拟寄存器：**

| 寄存器 | 用途 | 存储位置 |
|--------|------|----------|
| `xl` | 虚拟通讯寄存器1 | `[rbp - 808]` |
| `xh` | 虚拟通讯寄存器2 | `[rbp - 816]` |

**函数调用语法：**

```e
asm(''
    ; 调用外部函数，参数使用 _寄存器 语法
    call string, _ah;  // 调用 string 函数，参数是寄存器 ah
    
    ; 调用 eout 内置函数
    call eout [std] _al & _ah & endl;  // []内是命名空间
'');
```

### 2.13 字符串插值

字符串中可以使用 `{{expression}}` 语法进行插值计算：

```e
int x = 42;
dir path = "test_{{string(x)}}.txt";  // 编译时生成 "test_42.txt"
eout >namespace std "Hello {{string(x)}}!" & endl;  // 输出 "Hello 42!"
```

### 2.14 File I/O

使用 `dir` 类型的路径可以实现文件输入输出：

```e
// 文件输出
dir path = "output.txt";
eout >namespace path "Hello World!" & endl;

// 文件输入
string content = "";
ein >namespace path "" => content;
eout >namespace std "Content: " & content & "\n";
```

### 2.15 宏系统

E语言支持强大的**两级宏系统**：
- **一级宏**：基于文本替换，类似C语言的预处理器
- **二级宏**：基于AST操作，支持高级语法扩展

---

#### 2.15.1 一级宏（Level 1）

一级宏使用 `#` 前缀，进行纯文本替换。

##### 常量宏

```e
#define PI 3.14159
#define MAX_COUNT 1000

func main() {
    float radius = 5.0;
    float area = PI * radius * radius;
    eout >namespace std "Area: " & string(area) & endl;
}
```

##### 函数宏

```e
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define SQUARE(x) ((x) * (x))

func main() {
    int a = MAX(10, 20);
    int b = SQUARE(5);
    eout >namespace std "Max: " & string(a) & ", Square: " & string(b) & endl;
}
```

##### 字符串化操作符

使用 `#x` 将参数转换为字符串字面量：

```e
#define PRINT(x) eout >namespace std #x & " = " & string(x) & endl

func main() {
    int value = 42;
    PRINT(value);  // 输出: value = 42
}
```

##### 拼接操作符

使用 `a##b` 拼接两个标识符：

```e
#define CONCAT(a, b) a##b
#define PREFIX(name) my_##name

func main() {
    int my_value = 100;
    int CONCAT(val, ue) = 200;  // 等价于 int value = 200;
}
```

##### 宏作用域管理

使用 `#push` 和 `#pop` 管理宏作用域：

```e
#define X 100
eout >namespace std string(X) & endl;  // 输出 100

#push
#define X 200
eout >namespace std string(X) & endl;  // 输出 200
#pop

eout >namespace std string(X) & endl;  // 输出 100
```

##### 宏取消定义

```e
#define TEMP 123
eout >namespace std string(TEMP) & endl;

#undef TEMP
// TEMP 已不再可用
```

##### 条件编译

```e
#define DEBUG 1

#if DEBUG
    eout >namespace std "Debug mode enabled" & endl;
#else
    eout >namespace std "Release mode" & endl;
#endif

#define FEATURE_A 1

#ifdef FEATURE_A
    eout >namespace std "Feature A enabled" & endl;
#endif

#ifndef FEATURE_B
    eout >namespace std "Feature B not defined" & endl;
#endif
```

---

#### 2.15.2 二级宏（Level 2）

二级宏使用 `##$` 前缀，在编译时操作抽象语法树（AST），支持强大的语言扩展能力。

##### 二级宏的基本结构

```e
##$ 宏名: 类型;
    [字段: 值];
    [字段: 值];
    exec: {
        // 执行代码
        return ...;
    };
```

##### 支持的宏类型

| 类型 | 描述 |
|------|------|
| `keyword` | 关键字宏 |
| `function` | 函数宏 |
| `operator` | 运算符宏 |
| `syntax` | 语法宏 |
| `typedef` | 类型定义宏 |

---

##### 语法宏（syntax）

语法宏使用 `pattern` 定义匹配模式，支持 `${参数}` 语法捕获匹配内容。

```e
// 定义 unless 语句
##$ unless: syntax;
    pattern: "unless (${cond}) { ${body} }";
    exec: {
        return ast_create_if(ast_create_not(cond), body, NULL);
    };

func main() {
    int x = 0;
    unless (x > 0) {
        eout >namespace std "x is not positive" & endl;
    }
}
```

**pattern 语法**：
- `${name}` - 捕获任意表达式或语句
- 支持任意嵌套的花括号

---

##### 类型宏（typedef）

类型宏使用 `fields` 定义字段，可以动态创建结构体类型。

```e
##$ vec3: typedef;
    fields: [x: float, y: float, z: float];
    exec: {
        return ast_create_struct("Vec3", fields);
    };

func main() {
    vec3 position;
    position.x = 1.0;
    position.y = 2.0;
    position.z = 3.0;
}
```

---

##### 函数宏（function）

函数宏使用 `param` 定义参数列表，支持默认参数。

```e
##$ range: function;
    param: [start, end, step = 1];
    exec: {
        return ast_create_range_loop(start, end, step);
    };

func main() {
    int sum = 0;
    range (int i = 0; i < 10; i++) {
        sum += i;
    }
}
```

---

##### 运算符宏（operator）

运算符宏使用 `precedence` 和 `associativity` 定义运算符特性。

```e
// 定义幂运算符
##$ **: operator;
    precedence: 14;
    associativity: right;
    exec: {
        return ast_create_call("pow", node->left, node->right);
    };

func main() {
    int result = 2 ** 8;  // 计算 2 的 8 次方
    eout >namespace std "2^8 = " & string(result) & endl;
}
```

**precedence 优先级**：数值越大优先级越高

**associativity 结合性**：
- `left` - 左结合
- `right` - 右结合

---

#### 2.15.3 二级宏的 exec 块

`exec` 块是二级宏的核心，支持以下 AST 操作函数：

| 函数 | 描述 |
|------|------|
| `ast_create_if(cond, then, else)` | 创建 if 语句 |
| `ast_create_call(name, args...)` | 创建函数调用 |
| `ast_create_struct(name, fields)` | 创建结构体定义 |
| `ast_create_not(expr)` | 创建逻辑非表达式 |
| `ast_create_range_loop(start, end, step)` | 创建范围循环 |

**注意**：二级宏在预处理器阶段仅被定义和收集，实际的 AST 转换在编译后期执行。

---

#### 2.15.4 宏的处理流程

```
源文件 (.e)
    ↓
预处理器 (Lexer)
    ↓
一级宏展开 (文本替换)
    ↓
二级宏收集 (解析宏定义)
    ↓
语法分析 (Parser) → AST
    ↓
二级宏转换 (AST操作)
    ↓
代码生成 (Code Generator)
    ↓
目标代码 (.c 或 .asm)
```

---

#### 2.15.5 完整的宏示例

```e
// 一级宏示例
#define PI 3.14159
#define SQUARE(x) ((x) * (x))

// 二级宏示例
##$ unless: syntax;
    pattern: "unless (${cond}) { ${body} }";
    exec: {
        return ast_create_if(ast_create_not(cond), body, NULL);
    };

##$ vec3: typedef;
    fields: [x: float, y: float, z: float];
    exec: {
        return ast_create_struct("Vec3", fields);
    };

func main() {
    // 使用一级宏
    float radius = 5.0;
    float area = PI * SQUARE(radius);
    
    // 使用二级宏定义的类型
    vec3 pos;
    pos.x = 10.0;
    pos.y = 20.0;
    pos.z = 30.0;
    
    // 使用二级宏定义的语法
    unless (area > 100.0) {
        eout >namespace std "Area is small" & endl;
    }
}
```

---

## 3. 操作符参考

### 3.1 算术操作符

| 操作符 | 描述 | 优先级 | 结合性 |
|--------|------|--------|--------|
| `+` | 加法 | 中 | 左 |
| `-` | 减法 | 中 | 左 |
| `*` | 乘法 | 高 | 左 |
| `/` | 除法 | 高 | 左 |
| `%` | 取模 | 高 | 左 |

### 3.2 比较操作符

| 操作符 | 描述 | 优先级 | 结合性 |
|--------|------|--------|--------|
| `==` | 等于 | 低 | 左 |
| `!=` | 不等于 | 低 | 左 |
| `<` | 小于 | 低 | 左 |
| `<=` | 小于等于 | 低 | 左 |
| `>` | 大于 | 低 | 左 |
| `>=` | 大于等于 | 低 | 左 |

### 3.3 逻辑操作符

| 操作符 | 描述 | 优先级 | 结合性 |
|--------|------|--------|--------|
| `&&` | 逻辑与 | 低 | 左 |
| `\|\|` | 逻辑或 | 低 | 左 |
| `!` | 逻辑非 | 高 | 右 |

### 3.4 赋值操作符

| 操作符 | 描述 | 优先级 | 结合性 |
|--------|------|--------|--------|
| `=` | 赋值 | 最低 | 右 |
| `=>` | 赋值（函数返回） | 最低 | 右 |

### 3.5 字符串操作符

| 操作符 | 描述 | 优先级 | 结合性 |
|--------|------|--------|--------|
| `&` | 字符串拼接 | 低 | 左 |

### 3.6 指针操作符

| 操作符 | 描述 | 优先级 | 结合性 |
|--------|------|--------|--------|
| `&` | 取地址 | 高 | 右 |
| `.value` | 解引用 | 高 | 左 |

---

## 4. 编译器行为

### 4.1 编译流程

```
源文件 (.e)
    ↓
词法分析 (Lexer) → Token流
    ↓
语法分析 (Parser) → AST
    ↓
语义分析 (Symbol Table)
    ↓
代码生成 (Code Generator)
    ↓
    ├─→ C代码 (.c) → gcc → 可执行文件
    ├─→ NASM汇编 (.asm) → nasm/ld → 可执行文件
    ├─→ 中间表示 (.ir)
    └─→ 机器码 (.exe) → 直接生成Windows x86-64可执行文件（不需要外部工具）
```

#### IR到机器码的生成流程

```
AST节点
    ↓
IR生成器 → 跨平台中间表示
    ↓
机器码生成器 → x86-64指令
    ↓
PE文件构建器 → DOS头 + COFF头 + 可选头 + 节表 + 节数据
    ↓
output.exe (完整的Windows可执行文件)
```

### 4.2 符号表管理

编译器维护一个符号表来跟踪变量和函数的作用域：

| 作用域类型 | 描述 |
|-----------|------|
| 全局作用域 | 函数外部声明的变量和函数 |
| 局部作用域 | 函数内部声明的变量 |
| 块作用域 | 控制流语句（if、for、while）内部的变量 |

### 4.3 错误处理机制

编译器采用"错误恢复"策略，即使遇到错误也会继续分析：

1. **错误检测**：在词法、语法、语义分析阶段检测错误
2. **错误报告**：记录错误位置（行号、列号）和错误类型
3. **错误恢复**：尝试跳过当前语句继续分析
4. **错误汇总**：编译结束时输出所有错误信息

### 4.4 代码生成策略

#### C代码生成

- 使用 `sprintf` 实现类型转换
- 使用 `printf` 和 `scanf` 实现输入输出
- 使用 `malloc` 和 `free` 实现内存管理

#### NASM汇编生成（x86-64）

- 使用COFF格式（Windows）或ELF格式（Linux）
- 使用栈传递参数
- 使用系统V调用约定
- 自动管理栈帧

#### 中间表示（IR）生成

- 使用类汇编的跨平台语法
- 支持字符串收集和函数定义
- 支持控制流（if、while、for、switch）
- 统一的指令集（load、store、add、sub、print等）

#### 机器码生成（x86-64 Windows）

- **独立实现，不依赖外部工具
- 直接生成完整的PE文件格式
- 包含DOS头、COFF头、可选头、节表
- 支持.text、.data、.rdata节
- 导入表导入KERNEL32.dll函数（WriteFile、ExitProcess）
- 直接生成x86-64指令码
- 文件体积小（约6.5KB）

---

## 5. 边缘情况处理

### 5.1 变量相关

| 情况 | 处理方式 |
|------|----------|
| 未声明变量引用 | 报错："Undefined symbol" |
| 重复变量声明 | 报错："Redefinition of symbol" |
| 未初始化变量使用 | 警告："Using uninitialized variable" |
| 变量名冲突（作用域嵌套） | 内层变量遮蔽外层变量 |

### 5.2 函数相关

| 情况 | 处理方式 |
|------|----------|
| 未定义函数调用 | 报错："Undefined function" |
| 参数数量不匹配 | 报错："Argument count mismatch" |
| 参数类型不匹配 | 警告并尝试隐式转换 |
| 递归函数调用 | 支持（无特殊限制） |
| 函数别名重复 | 报错："Alias already defined" |

### 5.3 类型转换

| 情况 | 处理方式 |
|------|----------|
| int → float/double | 隐式转换，无警告 |
| float/double → int | 截断小数部分，警告 |
| string → int | 解析失败返回0，警告 |
| int → string | 始终成功 |
| 非法类型转换 | 报错："Invalid type conversion" |

### 5.4 内存管理

| 情况 | 处理方式 |
|------|----------|
| 重复释放 | 报错："Double free detected" |
| 使用已释放内存 | 警告："Use after free" |
| 空指针解引用 | 运行时错误（无法在编译时检测） |
| 内存泄漏 | 不检测（需要外部工具） |

### 5.5 控制流

| 情况 | 处理方式 |
|------|----------|
| 无限循环 | 不检测（运行时问题） |
| 死代码（return后语句） | 警告："Unreachable code" |
| 条件表达式非布尔类型 | 警告并转换为布尔值 |
| switch语句中无break | 正常行为（case穿透） |
| switch语句中default位置任意 | 支持（default可以在任何位置） |
| switch语句中case重复 | 报错："Duplicate case" |

### 5.6 字符串处理

| 情况 | 处理方式 |
|------|----------|
| 空字符串 | 正常处理 |
| 超长字符串 | 警告："String too long" |
| 字符串溢出 | 运行时错误（依赖C库） |
| 未闭合字符串 | 报错："Unterminated string" |

### 5.7 File I/O

| 情况 | 处理方式 |
|------|----------|
| 文件不存在 | 运行时错误 |
| 权限不足 | 运行时错误 |
| 路径格式错误 | 警告："Invalid path format" |
| 超大文件读取 | 内存不足时运行时错误 |

### 5.8 导入机制

| 情况 | 处理方式 |
|------|----------|
| 循环导入 | 报错："Circular import detected" |
| 不存在的文件 | 报错："Cannot open imported file" |
| 重复导入 | 忽略后续导入 |
| 导入文件语法错误 | 报错并终止编译 |

---

## 6. 平台兼容性

| 平台 | C代码生成 | NASM汇编生成 | 状态 |
|------|----------|-------------|------|
| Windows | ✅ | ✅ (COFF格式) | 测试通过 |
| Linux | ✅ | ✅ (ELF格式) | 基本支持 |
| macOS | ✅ | ⏳ | 未测试 |

---

## 7. VSCode插件

插件位于 `elhle/` 目录，提供以下功能：

### 7.1 功能特性

| 功能 | 描述 |
|------|------|
| **语法高亮** | 支持E语言关键字、类型、函数、字符串等 |
| **自动补全** | 支持内置类型、函数、关键字的自动补全 |
| **实时错误检查** | 通过集成编译器进行语法错误检查 |
| **代码格式化** | 支持基本的代码格式化 |
| **括号匹配** | 自动匹配括号、花括号、方括号 |
| **注释支持** | 支持单行和多行注释 |

### 7.2 安装方法

```bash
cd elhle
npm install
npm run compile
```

### 7.3 打包发布

```bash
npm install -g vsce
vsce package
```

生成的 `.vsix` 文件可以在 VSCode 中通过"从 VSIX 安装"安装。

---

## 8. 示例程序

### 8.1 Hello World

```e
func main() {
    eout >namespace std "Hello, World!" & endl;
}
```

### 8.2 斐波那契数列

```e
func fibonacci(int n) {
    if (n <= 1) {
        return n;
    }
    return fibonacci(n - 1) + fibonacci(n - 2);
}

func main() {
    int result = fibonacci(10);
    eout >namespace std "Fibonacci(10) = " & string(result) & endl;
}
```

### 8.3 内存管理示例

```e
func main() {
    mem buffer = alloc(100);
    po int ptr = alloc(int);
    ptr.value = 42;
    eout >namespace std "Value: " & string(ptr.value) & endl;
    free(ptr);
    free(buffer);
}
```

### 8.4 内联汇编示例

```e
func main() {
    // 使用汇编计算 2^8
    int power = asm(''
        mov rax, 2;
        mov rcx, 8;
        shl rax, cl;
    '');
    eout >namespace std "2^8 = " & string(power) & endl;
    
    // 使用虚拟寄存器
    asm(''
        mov xl, "Assembly";
        mov xh, 123;
    '');
    eout >namespace std asm(''mov rax, xl;'') & " " & string(asm(''mov rax, xh;'')) & endl;
}
```

---

## 9. 常见问题

### Q: 为什么生成的程序返回错误结果？
A: 检查变量声明顺序、栈空间分配、返回值处理。使用 `--nasm` 参数查看汇编代码。

### Q: 为什么NASM编译失败？
A: 确保安装了NASM编译器，并检查生成的汇编代码语法。

### Q: 为什么导入文件时出错？
A: 检查文件路径是否正确，确保被导入文件语法正确。

### Q: 为什么字符串拼接出错？
A: 确保使用 `&` 操作符，检查字符串是否正确闭合。

### Q: 如何调试汇编代码？
A: 使用 `--nasm` 参数生成汇编代码，检查 `output.asm` 文件。

### Q: 如何使用内联汇编？
A: 使用 `asm(''汇编代码'')` 语法，支持虚拟寄存器 `xl` 和 `xh`。

---

## 10. 特性实现状态

### ✅ 已实现
- 词法分析和语法分析
- 变量声明和赋值（int, float, double, string, char, dir）
- 函数定义和调用（支持返回类型和别名）
- 控制流（for, while, if-else, switch-case, break）
- 输入输出（eout, ein）
- 类型转换（string, int, char, dir）
- 字符串拼接和插值
- 错误恢复机制（报告多个错误）
- 符号表（变量/函数作用域和类型跟踪）
- 内存管理（AST节点释放）
- 指针功能（mem, po类型）
- 内存分配与释放（alloc, free）
- 内存安全检查（所有权跟踪、重复释放检测）
- 源文件导入（import）
- {{}} 插值语法
- 文件I/O（通过dir类型路径）
- 结构体和枚举
- NASM汇编代码生成（x86-64）
- 内联汇编（asm函数）
- VSCode插件（语法高亮、自动补全、错误检查、格式化）
- **一级宏系统**（#define, #undef, #push/#pop, #ifdef/#else/#endif, #x字符串化, a##b拼接）
- **二级宏系统**（##$ 前缀，支持 syntax/function/operator/typedef/keyword 五种类型）
- **中间表示（IR）**（跨平台中间代码，类汇编语法）
- **机器码生成**（直接生成x86-64 Windows可执行文件，不需要外部工具）

### ⏳ 部分实现
- 字符串拼接的解析（某些情况下可能出错）
- 二级宏的 exec 块（AST转换未完全实现）
- 机器码生成器（目前仅支持简单程序，需要扩展IR支持）

### ❌ 未实现
- 更复杂的类型系统
- 类和对象
- 异常处理
- 多线程支持