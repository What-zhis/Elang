[![CN](https://img.shields.io/badge/语言-中文-blue)](./README_CN.md)
[![Dev Guide](https://img.shields.io/badge/Developer-Guide-blue)](./dev.md)

# E Language Compiler

An E language compiler implemented in C, supporting variable declarations, function definitions, control flow, input/output, type conversion, structs, enums, objects, pointers, memory management, source file imports, inline assembly, and a powerful two-level macro system. The compiler can generate C code or NASM assembly code.

## Table of Contents

- [E Language Compiler](#e-language-compiler)
  - [Table of Contents](#table-of-contents)
  - [1. Quick Start](#1-quick-start)
    - [1.1 Compile the Compiler](#11-compile-the-compiler)
    - [1.2 Basic Usage](#12-basic-usage)
    - [1.3 Command Line Arguments](#13-command-line-arguments)
  - [2. Language Syntax](#2-language-syntax)
    - [2.1 Comments](#21-comments)
    - [2.2 Variable Declaration](#22-variable-declaration)
    - [2.3 Data Types](#23-data-types)
    - [2.4 Function Definition](#24-function-definition)
    - [2.5 Control Flow](#25-control-flow)
    - [2.6 Input Output](#26-input-output)
    - [2.7 Type Conversion](#27-type-conversion)
    - [2.8 String Operations](#28-string-operations)
    - [2.9 Pointers and Memory Management](#29-pointers-and-memory-management)
    - [2.10 Structs and Enums](#210-structs-and-enums)
    - [2.11 Objects](#211-objects)
    - [2.12 Source File Import](#212-source-file-import)
    - [2.13 Inline Assembly](#213-inline-assembly)
    - [2.14 String Interpolation](#214-string-interpolation)
    - [2.15 File I/O](#215-file-io)
    - [2.16 Macro System](#216-macro-system)
  - [3. Operator Reference](#3-operator-reference)
    - [3.1 Arithmetic Operators](#31-arithmetic-operators)
    - [3.2 Comparison Operators](#32-comparison-operators)
    - [3.3 Logical Operators](#33-logical-operators)
    - [3.4 Assignment Operators](#34-assignment-operators)
    - [3.5 String Operators](#35-string-operators)
    - [3.6 Pointer Operators](#36-pointer-operators)
  - [4. Compiler Behavior](#4-compiler-behavior)
    - [4.1 Compilation Process](#41-compilation-process)
    - [4.2 Symbol Table Management](#42-symbol-table-management)
    - [4.3 Error Handling Mechanism](#43-error-handling-mechanism)
    - [4.4 Code Generation Strategy](#44-code-generation-strategy)
  - [5. Edge Case Handling](#5-edge-case-handling)
    - [5.1 Variable Related](#51-variable-related)
    - [5.2 Function Related](#52-function-related)
    - [5.3 Type Conversion](#53-type-conversion)
    - [5.4 Memory Management](#54-memory-management)
    - [5.5 Control Flow](#55-control-flow)
    - [5.6 String Handling](#56-string-handling)
    - [5.7 File I/O](#57-file-io)
    - [5.8 Import Mechanism](#58-import-mechanism)
  - [6. Platform Compatibility](#6-platform-compatibility)
  - [7. VSCode Extension](#7-vscode-extension)
    - [7.1 Features](#71-features)
    - [7.2 Installation](#72-installation)
    - [7.3 Packaging](#73-packaging)
  - [8. Example Programs](#8-example-programs)
    - [8.1 Hello World](#81-hello-world)
    - [8.2 Fibonacci Sequence](#82-fibonacci-sequence)
    - [8.3 Memory Management Example](#83-memory-management-example)
    - [8.4 Inline Assembly Example](#84-inline-assembly-example)
  - [9. FAQ](#9-faq)
  - [10. Feature Implementation Status](#10-feature-implementation-status)

---

## 1. Quick Start

### 1.1 Compile the Compiler

```bash
gcc -o ecompiler.exe src/main.c src/lexer.c src/parser.c src/codegen.c src/codegen_nasm.c src/preprocessor.c
```

### 1.2 Basic Usage

```bash
# Compile and run (default behavior, generates C code)
./ecompiler.exe <source.e>

# Compile and run (explicit)
./ecompiler.exe --run <source.e>

# Compile only, don't run
./ecompiler.exe --compile <source.e>

# Check errors only, don't compile or run
./ecompiler.exe --debug <source.e>

# Generate NASM assembly and compile/run
./ecompiler.exe --nasm <source.e>
```

### 1.3 Command Line Arguments

| Argument | Type | Description |
|----------|------|-------------|
| `--run` | flag | Compile and run the program (default) |
| `--compile` | flag | Compile only, don't run |
| `--debug` | flag | Check syntax errors only, don't generate code |
| `--c` | flag | Generate C code |
| `--nasm` | flag | Generate NASM assembly code |
| `--ir` | flag | Generate Intermediate Representation (IR) code |
| `--machine` | flag | Generate machine code directly (x86-64 Windows, no external tools needed) |
| `--windows` | flag | Target Windows platform (can be combined with other flags) |
| `<source.e>` | string | Required: input E language source file |

#### Argument Combinations

| Combination | Behavior |
|-------------|----------|
| `--run --c` | Generate C code, compile and run |
| `--compile --nasm` | Generate NASM assembly and compile, don't run |
| `--machine --windows` | Generate Windows x86-64 executable directly (standalone, no external tools) |
| `--debug --ir` | Check syntax errors (IR mode) |

---

## 2. Language Syntax

### 2.1 Comments

E language supports two types of comments:

```e
// Single-line comment

/* Multi-line comment
   spanning multiple lines */

/[ Another multi-line comment
   syntax ]/
```

### 2.2 Variable Declaration

Syntax: `type variable_name [= value];`

```e
// Basic variable declarations
int a = 10;
float b = 3.14;
double c = 2.718;
string d = "Hello World";
char e = 'A';
dir f = "C:\\Test\\file.txt";

// Declaration without initial value
int x;
float y;
```

#### Variable Naming Rules

- Must start with a letter or underscore
- Can only contain letters, numbers, and underscores
- Case-sensitive
- Cannot use keywords as variable names

### 2.3 Data Types

| Type | Description | Size (bytes) | C Equivalent |
|------|-------------|-------------|--------------|
| `int` | Signed integer | 4 | `int` |
| `float` | Single-precision float | 4 | `float` |
| `double` | Double-precision float | 8 | `double` |
| `string` | String | variable | `char*` |
| `char` | Single character | 1 | `char` |
| `dir` | File path | variable | `char*` |
| `po <type>` | Pointer to specified type | 8 (64-bit) | `type*` |
| `mem` | Memory block | variable | `void*` |

### 2.4 Function Definition

**Syntax:** `func [return_type] function_name(parameters) { body } [=> alias];`

```e
// Function with return type
func float addFloat(float a, float b) {
    return a + b;
}

// Function without return type (defaults to int)
func addInt(int a, int b) {
    return a + b;
}

// Function without parameters
func sayHello() {
    eout >namespace std "Hello!" & endl;
}

// Function alias
func subtract(int x, int y) {
    return x - y;
} => sub;  // Note: semicolon required here
```

#### Function Calls

```e
float result1 = addFloat(1.5, 2.5);
int result2 = addInt(10, 20);
int result3 = sub(5, 3);  // Use function alias
```

#### Parameter Rules

| Rule | Description |
|------|-------------|
| Order | Passed left to right |
| Type | Must be explicitly declared |
| Count | Unlimited |
| Naming | Follows variable naming rules |

#### Return Value Rules

| Case | Behavior |
|------|----------|
| Explicit return | Use `return` statement |
| No return statement | Returns 0 by default |
| Type mismatch | Compiler warning, implicit conversion |

### 2.5 Control Flow

#### if-else Statement

```e
if (x > 0) {
    eout >namespace std "Positive";
} else if (x < 0) {
    eout >namespace std "Negative";
} else {
    eout >namespace std "Zero";
}
```

#### while Loop

```e
int i = 0;
while (i < 10) {
    eout >namespace std string(i) & endl;
    i = i + 1;
}
```

#### for Loop

```e
int sum = 0;
for (int i = 0; i < 10; i++) {
    sum = sum + i;
}
```

#### switch-case Statement

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

**switch-case Features:**
- Supports `case` branches matching constant values
- Supports `default` branch for unmatched cases
- Each `case` needs `break` to terminate, otherwise falls through
- Values in `case` must be constants (cannot be variables)

**break Statement:**
```e
func main() {
    for (int i = 0; i < 10; i++) {
        if (i == 5) {
            break;  // Exit loop
        }
        eout >namespace std string(i) & ", ";
    }
    // Output: 0, 1, 2, 3, 4,
}
```

### 2.6 Input Output

#### Output

```e
// Output to standard output
eout >namespace std "Hello World" & endl;
eout >namespace std "Number: " & string(42) & endl;

// Output multiple values
eout >namespace std "Name: " & name & ", Age: " & string(age) & endl;
```

#### Input

```e
// Read from standard input
string name;
ein >namespace std "Enter your name: " => name;

int age;
ein >namespace std "Enter your age: " => age;
```

### 2.7 Type Conversion

E language supports explicit type conversion:

```e
// Integer to string
string(123);      // Returns "123"
string(45.67);    // Returns "45.67"

// String to integer
int("123");       // Returns 123
int("45.67");     // Returns 45 (truncates decimal)

// Integer to character
char(65);         // Returns 'A'

// String to path
dir("C:\\Path");  // Returns path string
```

### 2.8 String Operations

#### String Concatenation

Use `&` operator to concatenate strings:

```e
string greeting = "Hello" & " " & "World";
string info = "Name: " & name & ", Age: " & string(age);
```

#### String Interpolation

Use `{{expression}}` syntax for string interpolation:

```e
int x = 42;
string message = "The answer is {{string(x)}}";  // Compiles to "The answer is 42"

dir path = "test_{{string(x)}}.txt";  // Generates "test_42.txt"
```

### 2.9 Pointers and Memory Management

#### Pointer Type Declaration

```e
// Declare a pointer to integer
po int ptr;

// Declare a pointer to struct
po Point ptr2;

// Declare a memory block
mem buffer;
```

#### Memory Allocation and Deallocation

```e
// Allocate 1024 bytes
mem buffer = alloc(1024);

// Allocate memory for one int
po int ptr = alloc(int);

// Free memory
free(buffer);
free(ptr);
```

#### Pointer Operations

```e
// Dereference
ptr.value = 100;    // Equivalent to *ptr = 100 in C
int a = ptr.value;  // Equivalent to a = *ptr in C

// Address-of
int var = 10;
po int ptr = &var;   // ptr points to var

// Pointer arithmetic
po int start = &arr[0];
po int end = start + 10;  // Move 10 ints forward

// Memory block to string
mem buffer = alloc(50);
string content = string(buffer); // Treat memory as string
```

#### Memory Safety

```e
func test() {
    mem buf = alloc(10);   // buf owns memory
    mem buf2 = buf;        // Ownership transferred to buf2, buf becomes null
    free(buf2);            // Correctly freed
} // Compiles successfully, buf2 was freed
```

### 2.10 Structs and Enums

#### Enum

```e
enum Color {
    RED,      // Value is 0
    GREEN,    // Value is 1
    BLUE = 5, // Value is 5
    YELLOW    // Value is 6
};
```

#### Struct

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

#### Using Structs

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

### 2.11 Objects

E language supports objects (singleton pattern), defined with `object` keyword:

```e
object config {
    int version = 1;
    string name = "E Language";
    int enabled = 1;
}

object user {
    string username = "admin";
    int level = 10;
}
```

#### Using Objects

Objects automatically create a global instance when defined, accessible directly:

```e
func main() {
    println("config.version = " & string(config.version));
    println("config.name = " & config.name);
    
    // Modify object properties
    config.version = 2;
    user.username = "root";
}
```

Object Features:
- Singleton instance created automatically when object is defined
- Supports basic types as members
- Members can have default values
- Use `.` operator to access members

### 2.12 Source File Import

Use `import` statement to include other `.e` source files. Functions and variables from imported files can be used directly.

**Syntax:**

```e
import "filename.e";
```

**Example:**

Create `utils.e`:
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

Use in main file:
```e
import "utils.e";

func main() {
    say_hello();  // Call function from utils.e
    int result = add(10, 20);  // Use function
    eout >namespace std "Result: " & string(result) & "\n";
}
```

### 2.13 Inline Assembly

E language supports inline assembly with NASM-style syntax.

**Syntax:**

```e
asm(''
    ; Assembly code
'')[index]
```

**Features:**

- Use `''` double single-quotes to enclose assembly code (supports multiple lines)
- Virtual communication registers `xl` and `xh` for passing data between assembly and E language
- Can call E language functions and built-in functions
- Return value is an array: `[xl_value, xh_value]`

**Example:**

```e
func main() {
    // Basic assembly operation
    int result = asm(''
        mov rax, 42;
        mov rbx, 10;
        add rax, rbx;
    '');
    eout >namespace std string(result) & endl;  // Output 52
    
    // Use virtual registers
    asm(''
        mov xl, 100;   // Store to xl
        mov xh, 200;   // Store to xh
    '');
    
    int a = asm(''mov rax, xl;'');  // Read xl value
    int b = asm(''mov rax, xh;'');  // Read xh value
    eout >namespace std string(a) & ", " & string(b) & endl;  // Output 100, 200
    
    // Access return value using array index
    int xl_val = asm(''mov xl, "Hello";'')[0];  // Get xl value
    eout >namespace std xl_val & endl;  // Output Hello
}
```

**Supported Assembly Instructions:**

| Category | Instructions |
|----------|--------------|
| Data Transfer | `mov`, `push`, `pop`, `lea` |
| Arithmetic | `add`, `sub`, `mul`, `div`, `imul`, `idiv` |
| Logical | `and`, `or`, `xor`, `not` |
| Control Flow | `jmp`, `je`, `jne`, `cmp`, `call`, `ret` |

**Virtual Registers:**

| Register | Purpose | Storage Location |
|----------|---------|-----------------|
| `xl` | Virtual communication register 1 | `[rbp - 808]` |
| `xh` | Virtual communication register 2 | `[rbp - 816]` |

**Function Call Syntax:**

```e
asm(''
    ; Call external function, use _register syntax for parameters
    call string, _ah;  // Call string function with parameter in ah
    
    ; Call eout built-in function
    call eout [std] _al & _ah & endl;  // [] contains namespace
'');
```

### 2.14 String Interpolation

Use `{{expression}}` syntax in strings for interpolation:

```e
int x = 42;
dir path = "test_{{string(x)}}.txt";  // Compiles to "test_42.txt"
eout >namespace std "Hello {{string(x)}}!" & endl;  // Output "Hello 42!"
```

### 2.15 File I/O

Use `dir` type paths for file input/output:

```e
// File output
dir path = "output.txt";
eout >namespace path "Hello World!" & endl;

// File input
string content = "";
ein >namespace path "" => content;
eout >namespace std "Content: " & content & "\n";
```

### 2.16 Macro System

E language supports a powerful **two-level macro system**:
- **Level 1 macros**: Text-based substitution, similar to C preprocessor
- **Level 2 macros**: AST-based operations, supporting advanced syntax extensions

---

#### 2.16.1 Level 1 Macros

Level 1 macros use `#` prefix for pure text substitution.

##### Constant Macros

```e
#define PI 3.14159
#define MAX_COUNT 1000

func main() {
    float radius = 5.0;
    float area = PI * radius * radius;
    eout >namespace std "Area: " & string(area) & endl;
}
```

##### Function Macros

```e
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define SQUARE(x) ((x) * (x))

func main() {
    int a = MAX(10, 20);
    int b = SQUARE(5);
    eout >namespace std "Max: " & string(a) & ", Square: " & string(b) & endl;
}
```

##### Stringification Operator

Use `#x` to convert parameter to string literal:

```e
#define PRINT(x) eout >namespace std #x & " = " & string(x) & endl

func main() {
    int value = 42;
    PRINT(value);  // Output: value = 42
}
```

##### Concatenation Operator

Use `a##b` to concatenate two identifiers:

```e
#define CONCAT(a, b) a##b
#define PREFIX(name) my_##name

func main() {
    int my_value = 100;
    int CONCAT(val, ue) = 200;  // Equivalent to int value = 200;
}
```

##### Macro Scope Management

Use `#push` and `#pop` to manage macro scope:

```e
#define X 100
eout >namespace std string(X) & endl;  // Output 100

#push
#define X 200
eout >namespace std string(X) & endl;  // Output 200
#pop

eout >namespace std string(X) & endl;  // Output 100
```

##### Macro Undefine

```e
#define TEMP 123
eout >namespace std string(TEMP) & endl;

#undef TEMP
// TEMP is no longer available
```

##### Conditional Compilation

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

#### 2.16.2 Level 2 Macros

Level 2 macros use `##$` prefix to operate on Abstract Syntax Trees (AST) during compilation, enabling powerful language extensions.

##### Level 2 Macro Structure

```e
##$ macro_name: type;
    [field: value];
    [field: value];
    exec: {
        // Execution code
        return ...;
    };
```

##### Supported Macro Types

| Type | Description |
|------|-------------|
| `keyword` | Keyword macro |
| `function` | Function macro |
| `operator` | Operator macro |
| `syntax` | Syntax macro |
| `typedef` | Type definition macro |

---

##### Syntax Macro

Syntax macros use `pattern` to define matching patterns, supporting `${parameter}` syntax to capture matched content.

```e
// Define unless statement
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

**Pattern Syntax**:
- `${name}` - Capture any expression or statement
- Supports nested curly braces

---

##### Type Macro

Type macros use `fields` to define fields, enabling dynamic struct type creation.

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

##### Function Macro

Function macros use `param` to define parameter lists, supporting default parameters.

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

##### Operator Macro

Operator macros use `precedence` and `associativity` to define operator properties.

```e
// Define power operator
##$ **: operator;
    precedence: 14;
    associativity: right;
    exec: {
        return ast_create_call("pow", node->left, node->right);
    };

func main() {
    int result = 2 ** 8;  // Calculate 2 to the power of 8
    eout >namespace std "2^8 = " & string(result) & endl;
}
```

**Precedence**: Higher value means higher priority

**Associativity**:
- `left` - Left-associative
- `right` - Right-associative

---

#### 2.16.3 Level 2 Macro exec Block

The `exec` block is the core of level 2 macros, supporting the following AST manipulation functions:

| Function | Description |
|----------|-------------|
| `ast_create_if(cond, then, else)` | Create if statement |
| `ast_create_call(name, args...)` | Create function call |
| `ast_create_struct(name, fields)` | Create struct definition |
| `ast_create_not(expr)` | Create logical not expression |
| `ast_create_range_loop(start, end, step)` | Create range loop |

**Note**: Level 2 macros are only defined and collected during preprocessing; actual AST transformation occurs later in compilation.

---

#### 2.16.4 Macro Processing Flow

```
Source file (.e)
    ↓
Preprocessor (Lexer)
    ↓
Level 1 Macro Expansion (text substitution)
    ↓
Level 2 Macro Collection (parse macro definitions)
    ↓
Syntax Analysis (Parser) → AST
    ↓
Level 2 Macro Transformation (AST operations)
    ↓
Code Generation (Code Generator)
    ↓
Target Code (.c or .asm)
```

---

#### 2.16.5 Complete Macro Example

```e
// Level 1 macro example
#define PI 3.14159
#define SQUARE(x) ((x) * (x))

// Level 2 macro example
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
    // Use level 1 macro
    float radius = 5.0;
    float area = PI * SQUARE(radius);
    
    // Use type defined by level 2 macro
    vec3 pos;
    pos.x = 10.0;
    pos.y = 20.0;
    pos.z = 30.0;
    
    // Use syntax defined by level 2 macro
    unless (area > 100.0) {
        eout >namespace std "Area is small" & endl;
    }
}
```

---

## 3. Operator Reference

### 3.1 Arithmetic Operators

| Operator | Description | Precedence | Associativity |
|----------|-------------|------------|---------------|
| `+` | Addition | medium | left |
| `-` | Subtraction | medium | left |
| `*` | Multiplication | high | left |
| `/` | Division | high | left |
| `%` | Modulo | high | left |

### 3.2 Comparison Operators

| Operator | Description | Precedence | Associativity |
|----------|-------------|------------|---------------|
| `==` | Equal | low | left |
| `!=` | Not equal | low | left |
| `<` | Less than | low | left |
| `<=` | Less than or equal | low | left |
| `>` | Greater than | low | left |
| `>=` | Greater than or equal | low | left |

### 3.3 Logical Operators

| Operator | Description | Precedence | Associativity |
|----------|-------------|------------|---------------|
| `&&` | Logical AND | low | left |
| `\|\|` | Logical OR | low | left |
| `!` | Logical NOT | high | right |

### 3.4 Assignment Operators

| Operator | Description | Precedence | Associativity |
|----------|-------------|------------|---------------|
| `=` | Assignment | lowest | right |
| `=>` | Assignment (function return) | lowest | right |

### 3.5 String Operators

| Operator | Description | Precedence | Associativity |
|----------|-------------|------------|---------------|
| `&` | String concatenation | low | left |

### 3.6 Pointer Operators

| Operator | Description | Precedence | Associativity |
|----------|-------------|------------|---------------|
| `&` | Address-of | high | right |
| `.value` | Dereference | high | left |

---

## 4. Compiler Behavior

### 4.1 Compilation Process

```
Source file (.e)
    ↓
Lexical Analysis (Lexer) → Token stream
    ↓
Syntax Analysis (Parser) → AST
    ↓
Semantic Analysis (Symbol Table)
    ↓
Code Generation (Code Generator)
    ↓
    ├─→ C code (.c) → gcc → Executable
    ├─→ NASM assembly (.asm) → nasm/ld → Executable
    ├─→ Intermediate Representation (.ir)
    └─→ Machine code (.exe) → Directly generate Windows x86-64 executable (no external tools needed)
```

#### IR to Machine Code Generation Process

```
AST Node
    ↓
IR Generator → Cross-platform Intermediate Representation
    ↓
Machine Code Generator → x86-64 Instructions
    ↓
PE File Builder → DOS Header + COFF Header + Optional Header + Section Table + Section Data
    ↓
output.exe (Complete Windows executable)
```

### 4.2 Symbol Table Management

The compiler maintains a symbol table to track variable and function scopes:

| Scope Type | Description |
|------------|-------------|
| Global Scope | Variables and functions declared outside functions |
| Local Scope | Variables declared inside functions |
| Block Scope | Variables inside control flow statements (if, for, while) |

### 4.3 Error Handling Mechanism

The compiler uses an "error recovery" strategy, continuing analysis even when errors are encountered:

1. **Error Detection**: Detect errors during lexical, syntax, and semantic analysis
2. **Error Reporting**: Record error location (line, column) and error type
3. **Error Recovery**: Attempt to skip current statement and continue
4. **Error Summary**: Output all error messages at the end of compilation

### 4.4 Code Generation Strategy

#### C Code Generation

- Use `sprintf` for type conversion
- Use `printf` and `scanf` for input/output
- Use `malloc` and `free` for memory management

#### NASM Assembly Generation (x86-64)

- Use COFF format (Windows) or ELF format (Linux)
- Use stack for parameter passing
- Use System V calling convention
- Automatic stack frame management

#### Intermediate Representation (IR) Generation

- Use assembly-like cross-platform syntax
- Supports string collection and function definitions
- Supports control flow (if, while, for, switch)
- Unified instruction set (load, store, add, sub, print, etc.)

#### Machine Code Generation (x86-64 Windows)

- **Standalone implementation, no external tools required
- Directly generates complete PE file format
- Includes DOS header, COFF header, optional header, section table
- Supports .text, .data, .rdata sections
- Imports KERNEL32.dll functions (WriteFile, ExitProcess)
- Directly generates x86-64 instruction bytes
- Small file size (approximately 6.5KB)

---

## 5. Edge Case Handling

### 5.1 Variable Related

| Case | Handling |
|------|----------|
| Undeclared variable reference | Error: "Undefined symbol" |
| Duplicate variable declaration | Error: "Redefinition of symbol" |
| Uninitialized variable use | Warning: "Using uninitialized variable" |
| Variable name conflict (nested scopes) | Inner variable shadows outer variable |

### 5.2 Function Related

| Case | Handling |
|------|----------|
| Undefined function call | Error: "Undefined function" |
| Argument count mismatch | Error: "Argument count mismatch" |
| Argument type mismatch | Warning and attempt implicit conversion |
| Recursive function call | Supported (no special restrictions) |
| Duplicate function alias | Error: "Alias already defined" |

### 5.3 Type Conversion

| Case | Handling |
|------|----------|
| int → float/double | Implicit conversion, no warning |
| float/double → int | Truncate decimal, warning |
| string → int | Return 0 on parse failure, warning |
| int → string | Always succeeds |
| Invalid type conversion | Error: "Invalid type conversion" |

### 5.4 Memory Management

| Case | Handling |
|------|----------|
| Double free | Error: "Double free detected" |
| Use after free | Warning: "Use after free" |
| Null pointer dereference | Runtime error (cannot detect at compile time) |
| Memory leak | Not detected (requires external tools) |

### 5.5 Control Flow

| Case | Handling |
|------|----------|
| Infinite loop | Not detected (runtime issue) |
| Unreachable code (after return) | Warning: "Unreachable code" |
| Non-boolean condition expression | Warning and convert to boolean |
| switch without break | Normal behavior (fall-through) |
| default anywhere in switch | Supported (can be in any position) |
| Duplicate case in switch | Error: "Duplicate case" |

### 5.6 String Handling

| Case | Handling |
|------|----------|
| Empty string | Normal handling |
| Very long string | Warning: "String too long" |
| String overflow | Runtime error (depends on C library) |
| Unterminated string | Error: "Unterminated string" |

### 5.7 File I/O

| Case | Handling |
|------|----------|
| File not found | Runtime error |
| Permission denied | Runtime error |
| Invalid path format | Warning: "Invalid path format" |
| Reading very large file | Runtime error on out-of-memory |

### 5.8 Import Mechanism

| Case | Handling |
|------|----------|
| Circular import | Error: "Circular import detected" |
| Non-existent file | Error: "Cannot open imported file" |
| Duplicate import | Ignore subsequent imports |
| Syntax error in imported file | Error and terminate compilation |

---

## 6. Platform Compatibility

| Platform | C Code Generation | NASM Assembly Generation | Status |
|----------|-------------------|--------------------------|--------|
| Windows | ✅ | ✅ (COFF format) | Tested |
| Linux | ✅ | ✅ (ELF format) | Basic support |
| macOS | ✅ | ⏳ | Untested |

---

## 7. VSCode Extension

Extension located in `elhle/` directory, providing the following features:

### 7.1 Features

| Feature | Description |
|---------|-------------|
| **Syntax Highlighting** | Supports E language keywords, types, functions, strings, etc. |
| **Auto Completion** | Supports auto-completion for built-in types, functions, keywords |
| **Real-time Error Checking** | Syntax error checking via integrated compiler |
| **Code Formatting** | Basic code formatting support |
| **Bracket Matching** | Auto-match parentheses, braces, brackets |
| **Comment Support** | Single-line and multi-line comments |

### 7.2 Installation

```bash
cd elhle
npm install
npm run compile
```

### 7.3 Packaging

```bash
npm install -g vsce
vsce package
```

The generated `.vsix` file can be installed in VSCode via "Install from VSIX".

---

## 8. Example Programs

### 8.1 Hello World

```e
func main() {
    eout >namespace std "Hello, World!" & endl;
}
```

### 8.2 Fibonacci Sequence

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

### 8.3 Memory Management Example

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

### 8.4 Inline Assembly Example

```e
func main() {
    // Calculate 2^8 using assembly
    int power = asm(''
        mov rax, 2;
        mov rcx, 8;
        shl rax, cl;
    '');
    eout >namespace std "2^8 = " & string(power) & endl;
    
    // Use virtual registers
    asm(''
        mov xl, "Assembly";
        mov xh, 123;
    '');
    eout >namespace std asm(''mov rax, xl;'') & " " & string(asm(''mov rax, xh;'')) & endl;
}
```

---

## 9. FAQ

### Q: Why does my program return wrong results?
A: Check variable declaration order, stack allocation, return value handling. Use `--nasm` flag to inspect assembly code.

### Q: Why does NASM compilation fail?
A: Ensure NASM compiler is installed, check generated assembly syntax.

### Q: Why do I get errors when importing files?
A: Check file path is correct, ensure imported file has valid syntax.

### Q: Why does string concatenation fail?
A: Ensure using `&` operator, check strings are properly terminated.

### Q: How to debug assembly code?
A: Use `--nasm` flag to generate assembly, inspect `output.asm` file.

### Q: How to use inline assembly?
A: Use `asm(''assembly code'')` syntax, supports virtual registers `xl` and `xh`.

---

## 10. Feature Implementation Status

### ✅ Implemented
- Lexical and syntax analysis
- Variable declaration and assignment (int, float, double, string, char, dir)
- Function definition and call (with return types and aliases)
- Control flow (for, while, if-else, switch-case, break)
- Input/output (eout, ein)
- Type conversion (string, int, char, dir)
- String concatenation and interpolation
- Error recovery mechanism (report multiple errors)
- Symbol table (variable/function scope and type tracking)
- Memory management (AST node deallocation)
- Pointer functionality (mem, po types)
- Memory allocation and deallocation (alloc, free)
- Memory safety checks (ownership tracking, double-free detection)
- Source file import (import)
- {{}} interpolation syntax
- File I/O (via dir type paths)
- Structs and enums
- **Objects** (singleton objects with member variables and default values)
- NASM assembly code generation (x86-64)
- Inline assembly (asm function)
- VSCode extension (syntax highlighting, auto-completion, error checking, formatting)
- **Level 1 macro system** (#define, #undef, #push/#pop, #ifdef/#else/#endif, #x stringification, a##b concatenation)
- **Level 2 macro system** (##$ prefix, supports syntax/function/operator/typedef/keyword types)
- **Intermediate Representation (IR)** (cross-platform intermediate code, assembly-like syntax)
- **Machine code generation** (direct x86-64 Windows executable generation, no external tools needed)

### ⏳ Partially Implemented
- String concatenation parsing (may fail in some cases)
- Level 2 macro exec blocks (AST transformation not fully implemented)
- Machine code generator (currently supports simple programs, needs IR extension)

### ❌ Not Implemented
- More complex type system
- Classes and objects
- Exception handling
- Multi-threading support