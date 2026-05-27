# Compiler Developer Guide

This document describes the behaviors and characteristics that a qualified compiler should possess, helping developers understand the core responsibilities of a compiler.

## Table of Contents

- [Compiler Developer Guide](#compiler-developer-guide)
  - [Table of Contents](#table-of-contents)
  - [1. Core Responsibilities of a Compiler](#1-core-responsibilities-of-a-compiler)
  - [2. Lexer Behavior Requirements](#2-lexer-behavior-requirements)
    - [2.1 Token Classification](#21-token-classification)
    - [2.2 Whitespace and Comment Handling](#22-whitespace-and-comment-handling)
    - [2.3 Error Handling](#23-error-handling)
  - [3. Parser Behavior Requirements](#3-parser-behavior-requirements)
    - [3.1 Syntax Rule Validation](#31-syntax-rule-validation)
    - [3.2 AST Construction](#32-ast-construction)
    - [3.3 Error Recovery](#33-error-recovery)
  - [4. Semantic Analyzer Behavior Requirements](#4-semantic-analyzer-behavior-requirements)
    - [4.1 Symbol Table Management](#41-symbol-table-management)
    - [4.2 Type Checking](#42-type-checking)
    - [4.3 Scope Management](#43-scope-management)
  - [5. Code Generator Behavior Requirements](#5-code-generator-behavior-requirements)
    - [5.1 Target Code Generation](#51-target-code-generation)
    - [5.2 Optimization](#52-optimization)
    - [5.3 Platform Adaptation](#53-platform-adaptation)
  - [6. Preprocessor Behavior Requirements](#6-preprocessor-behavior-requirements)
    - [6.1 Macro Processing](#61-macro-processing)
    - [6.2 File Import](#62-file-import)
    - [6.3 Conditional Compilation](#63-conditional-compilation)
  - [7. Error Handling System](#7-error-handling-system)
    - [7.1 Error Detection](#71-error-detection)
    - [7.2 Error Reporting](#72-error-reporting)
    - [7.3 User Experience](#73-user-experience)
  - [8. Compiler Performance Requirements](#8-compiler-performance-requirements)
    - [8.1 Compilation Speed](#81-compilation-speed)
    - [8.2 Memory Usage](#82-memory-usage)
    - [8.3 Extensibility](#83-extensibility)
  - [9. Compiler Compatibility Requirements](#9-compiler-compatibility-requirements)
    - [9.1 Language Specification Compatibility](#91-language-specification-compatibility)
    - [9.2 Platform Compatibility](#92-platform-compatibility)
    - [9.3 Toolchain Compatibility](#93-toolchain-compatibility)

---

## 1. Core Responsibilities of a Compiler

A qualified compiler should be able to:

1. **Correctly parse** source code that conforms to the language specification
2. **Accurately report** syntax and semantic errors in the source code
3. **Generate correct** target code (assembly, machine code, or other languages)
4. **Optimize generated** code for better execution efficiency
5. **Provide good** user experience (clear error messages, reasonable compilation time)

---

## 2. Lexer Behavior Requirements

### 2.1 Token Classification

The lexer should be able to identify and classify the following types of tokens:

| Token Type | Description | Examples |
|-----------|-------------|----------|
| **Keywords** | Language reserved words | `func`, `if`, `else`, `return` |
| **Identifiers** | Variable names, function names, etc. | `myVar`, `calculateSum` |
| **Literals** | Constant values | `42`, `3.14`, `"hello"`, `'A'` |
| **Operators** | Arithmetic, comparison, logical operators | `+`, `-`, `*`, `/`, `==`, `&&` |
| **Delimiters** | Parentheses, semicolons, commas, etc. | `(`, `)`, `{`, `}`, `;`, `,` |

### 2.2 Whitespace and Comment Handling

- **Whitespace characters**: Spaces, tabs, and newlines should be properly ignored
- **Single-line comments**: Content from the comment marker to the end of line should be ignored
- **Multi-line comments**: Comments spanning multiple lines should be handled correctly, including nested comments

### 2.3 Error Handling

The lexer should be able to:
- Identify illegal characters
- Report unclosed string literals
- Report invalid number formats
- Locate the position where the error occurred (line and column numbers)

---

## 3. Parser Behavior Requirements

### 3.1 Syntax Rule Validation

The parser should validate that the source code conforms to the language's syntax rules:

- **Expression syntax**: Correctly handle operator precedence and associativity
- **Statement syntax**: Support control flow statements like if-else, while, for, switch
- **Declaration syntax**: Support variable declarations, function declarations, struct declarations, etc.
- **Function calls**: Correctly parse function argument lists

### 3.2 AST Construction

The parser should construct a correct Abstract Syntax Tree:

- **Node types**: Each syntactic structure corresponds to a node type
- **Node relationships**: Correctly express parent-child and sibling relationships between nodes
- **Node properties**: Include necessary semantic information (e.g., identifier names, literal values)

### 3.3 Error Recovery

The parser should have error recovery capabilities:

- When encountering a syntax error, attempt to skip the current statement and continue parsing
- Report multiple errors instead of stopping at the first error
- Provide accurate error positions and descriptions

---

## 4. Semantic Analyzer Behavior Requirements

### 4.1 Symbol Table Management

The semantic analyzer should maintain a symbol table:

- **Symbol registration**: Register variables, functions, and other symbols upon declaration
- **Symbol lookup**: Look up symbol definitions when used
- **Symbol conflict detection**: Detect conflicts like duplicate declarations

### 4.2 Type Checking

The semantic analyzer should perform type checking:

- **Type matching**: Verify that expression types are compatible
- **Implicit conversion**: Support reasonable implicit type conversions
- **Type error reporting**: Report type mismatch errors

### 4.3 Scope Management

The semantic analyzer should manage variable scopes:

- **Scope creation**: Create new scopes when entering functions, loops, and conditional statements
- **Scope destruction**: Destroy scopes when exiting
- **Scope lookup**: Search for symbols from the current scope upward

---

## 5. Code Generator Behavior Requirements

### 5.1 Target Code Generation

The code generator should be able to:

- **Generate correct target code**: Ensure generated code is semantically equivalent to source code
- **Handle all language features**: Support variables, functions, control flow, pointers, etc.
- **Generate linkable code**: Follow the target platform's calling conventions

### 5.2 Optimization

The code generator should perform reasonable optimizations:

- **Constant folding**: Evaluate constant expressions at compile time
- **Dead code elimination**: Remove unreachable code
- **Common subexpression elimination**: Avoid redundant calculations
- **Register allocation**: Optimize register usage

### 5.3 Platform Adaptation

The code generator should support multiple target platforms:

- **Instruction set adaptation**: Support x86-64, ARM, and other instruction sets
- **System call adaptation**: Adapt to system calls of different operating systems
- **ABI adaptation**: Follow the target platform's Application Binary Interface

---

## 6. Preprocessor Behavior Requirements

### 6.1 Macro Processing

The preprocessor should support macro definition and expansion:

- **Constant macros**: Simple text substitution
- **Function macros**: Parameterized macro expansion
- **Macro scoping**: Support macro scope management

### 6.2 File Import

The preprocessor should support file imports:

- **Import parsing**: Parse import statements
- **Path resolution**: Correctly resolve paths of imported files
- **Circular import detection**: Detect and report circular imports

### 6.3 Conditional Compilation

The preprocessor should support conditional compilation:

- **Conditional directives**: Support #if, #ifdef, #ifndef, #else, #endif
- **Condition evaluation**: Correctly evaluate conditional expressions
- **Nested conditions**: Support nested conditional compilation

---

## 7. Error Handling System

### 7.1 Error Detection

The compiler should detect errors at various stages:

- **Lexical errors**: Illegal characters, unclosed strings, etc.
- **Syntax errors**: Structures that do not conform to syntax rules
- **Semantic errors**: Type mismatches, undefined symbols, etc.
- **Code generation errors**: Cases where valid code cannot be generated

### 7.2 Error Reporting

The compiler should provide clear error reporting:

- **Location information**: Report the file name, line number, and column number where the error occurred
- **Error type**: Clearly indicate the error type (syntax error, type error, etc.)
- **Error description**: Provide a clear description of the error cause
- **Fix suggestions**: Provide possible fix suggestions

### 7.3 User Experience

The compiler should provide a good user experience:

- **Color output**: Use colors to distinguish errors and warnings
- **Context information**: Show code surrounding the error
- **Error codes**: Assign unique codes to each error type

---

## 8. Compiler Performance Requirements

### 8.1 Compilation Speed

The compiler should have reasonable compilation speed:

- **Linear time complexity**: Compilation time should be linear with code size
- **Incremental compilation**: Support recompiling only modified files
- **Parallel compilation**: Support parallel compilation of multiple files

### 8.2 Memory Usage

The compiler should use memory reasonably:

- **Memory management**: Release memory that is no longer needed in a timely manner
- **Large file support**: Handle large source files
- **Memory constraints**: Fail gracefully when memory is insufficient

### 8.3 Extensibility

The compiler should have good extensibility:

- **Modular design**: Each stage is independent
- **Plugin system**: Support extending functionality through plugins
- **Configuration options**: Provide rich configuration options

---

## 9. Compiler Compatibility Requirements

### 9.1 Language Specification Compatibility

The compiler should strictly comply with the language specification:

- **Syntax compatibility**: Support all syntax defined by the language specification
- **Semantic compatibility**: Ensure semantics match the language specification
- **Backward compatibility**: Compatible with older versions of source code

### 9.2 Platform Compatibility

The compiler should support multiple platforms:

- **Operating system compatibility**: Support Windows, Linux, macOS, etc.
- **Architecture compatibility**: Support x86-64, ARM, RISC-V, etc.
- **Toolchain compatibility**: Compatible with mainstream build toolchains

### 9.3 Toolchain Compatibility

The compiler should integrate with other tools:

- **Debugger support**: Generate debugging information (DWARF, PDB, etc.)
- **Editor support**: Provide LSP (Language Server Protocol) support
- **Build system support**: Integrate with Make, CMake, Ninja, etc.

---

*This document describes best practices and behavioral specifications for compiler development*
