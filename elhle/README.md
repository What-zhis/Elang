# E Language Support for VS Code

This extension provides language support for the E programming language, including syntax highlighting, auto-completion, error checking, and code formatting.

## Features

- **Syntax Highlighting**: Highlights keywords, types, functions, strings, numbers, and operators in E code.
- **Auto-completion**: Provides suggestions for built-in types, functions, and keywords.
- **Error Checking**: Integrates with the E compiler to provide real-time error diagnostics.
- **Code Formatting**: Formats E code with proper indentation and spacing.

## Installation

1. Open VS Code.
2. Go to Extensions (Ctrl+Shift+X).
3. Search for "E Language Support".
4. Click Install.
5. Reload VS Code to activate the extension.

## Usage

### Syntax Highlighting
The extension automatically highlights E code in files with the `.e` extension.

### Auto-completion
Start typing in an E file, and the extension will provide suggestions for:
- Built-in types: `int`, `string`, `char`, `float`, `double`, `dir`
- Built-in functions: `string()`, `int()`, `char()`, `dir()`
- Keywords: `func`, `if`, `else`, `for`, `while`, `return`, `import`, `namespace`, `eout`, `ein`

### Error Checking
The extension runs the E compiler in the background to check for syntax errors and displays diagnostics in the editor.

### Code Formatting
To format an E file:
1. Open the file.
2. Press Shift+Alt+F or right-click and select "Format Document".

## Configuration

The extension uses the following default configuration:

- **Tab Size**: 4 spaces
- **Indentation**: Spaces

## Examples

### Basic E Code

```e
import <stdio>;

int x = 10;
float pi = 3.14;

func float addFloat(float a, float b) {
    return a + b;
}

func int main() {
    eout >namespace std "Hello World!" & endl;
    return 0;
}
```

### Type Conversion

```e
int num = 42;
string str = string(num);
int converted = int(str);
char ch = char(65);
dir path = C:\Test\file.txt;
```

### Control Flow

```e
func int main() {
    int i = 0;
    for(i = 0; i < 5; i++) {
        eout >namespace std "i = " & string(i) & endl;
    }

    if (i > 3) {
        eout >namespace std "i is greater than 3" & endl;
    } else {
        eout >namespace std "i is 3 or less" & endl;
    }

    return 0;
}
```

## Requirements

- VS Code 1.60.0 or later
- E compiler (elang.exe) in the same directory as the extension or in the project root

## Known Issues

- The extension may not detect all errors in complex E code.
- Auto-completion may not work for user-defined functions and variables.
- dir type path interpolation (`C:\{{var}}\file.txt`) is not yet implemented.

## Building from Source

```bash
# Install dependencies
npm install

# Compile TypeScript
npm run compile

# Package the extension
npm install -g vsce
vsce package
```

## Contributing

Contributions are welcome! Please feel free to submit a pull request or open an issue.

## License

MIT