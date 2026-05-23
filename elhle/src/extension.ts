import * as vscode from 'vscode';
import * as child_process from 'child_process';
import * as fs from 'fs';
import * as path from 'path';

const builtinTypes = ['int', 'string', 'char', 'float', 'double', 'dir', 'mem', 'po', 'bool', 'void'];
const builtinFunctions = ['string', 'int', 'char', 'dir', 'alloc', 'free', 'printf', 'scanf', 'print', 'println', 'eout', 'ein', 'sizeof', 'typeof', 'strlen', 'strcat', 'strcpy', 'strcmp'];
const builtinKeywords = ['func', 'if', 'else', 'else if', 'for', 'while', 'return', 'import', 'export', 'namespace', 'eout', 'ein', 'struct', 'enum', 'object', 'mem', 'po', 'alloc', 'free', 'asm', 'break', 'continue', 'switch', 'case', 'default', 'new', 'delete', 'true', 'false', 'null', 'void', 'this', 'super', 'as', 'from', 'typedef'];
const level1Macros = ['#define', '#undef', '#push', '#pop', '#ifdef', '#ifndef', '#else', '#endif', '#if', '#include'];
const level2MacroTypes = ['keyword', 'function', 'operator', 'syntax', 'typedef'];
const level2MacroFields = ['pattern', 'fields', 'param', 'precedence', 'associativity', 'exec'];
const astFunctions = ['ast_create_if', 'ast_create_call', 'ast_create_struct', 'ast_create_not', 'ast_create_range_loop'];

class ECompletionItemProvider implements vscode.CompletionItemProvider {
    provideCompletionItems(
        document: vscode.TextDocument,
        position: vscode.Position,
        token: vscode.CancellationToken,
        context: vscode.CompletionContext
    ): vscode.ProviderResult<vscode.CompletionItem[] | vscode.CompletionList> {
        const completions: vscode.CompletionItem[] = [];
        const linePrefix = document.lineAt(position).text.substr(0, position.character);

        const isLevel1MacroContext = linePrefix.trim().startsWith('#');
        const isLevel2MacroContext = linePrefix.trim().startsWith('##$') || this.isInsideLevel2Macro(document, position);
        const isExecBlock = this.isInsideExecBlock(document, position);

        if (isExecBlock) {
            astFunctions.forEach(func => {
                const item = new vscode.CompletionItem(func, vscode.CompletionItemKind.Function);
                item.detail = 'AST creation function';
                item.documentation = 'Level 2 macro AST manipulation function';
                completions.push(item);
            });
            return completions;
        }

        if (isLevel1MacroContext) {
            level1Macros.forEach(macro => {
                const item = new vscode.CompletionItem(macro, vscode.CompletionItemKind.Keyword);
                item.detail = 'Preprocessor directive';
                completions.push(item);
            });
            return completions;
        }

        if (isLevel2MacroContext) {
            level2MacroTypes.forEach(type => {
                const item = new vscode.CompletionItem(type, vscode.CompletionItemKind.TypeParameter);
                item.detail = 'Macro type';
                completions.push(item);
            });

            level2MacroFields.forEach(field => {
                const item = new vscode.CompletionItem(field, vscode.CompletionItemKind.Property);
                item.detail = 'Macro property';
                if (field === 'pattern') {
                    item.insertText = `${field}: "";`;
                } else if (field === 'exec') {
                    item.insertText = `${field}: {\n    \n};`;
                } else if (field === 'fields' || field === 'param') {
                    item.insertText = `${field}: [];`;
                } else {
                    item.insertText = `${field}: `;
                }
                completions.push(item);
            });
            return completions;
        }

        if (linePrefix.endsWith('#')) {
            level1Macros.forEach(macro => {
                const item = new vscode.CompletionItem(macro, vscode.CompletionItemKind.Keyword);
                item.detail = 'Preprocessor directive';
                completions.push(item);
            });
        }

        builtinTypes.forEach(type => {
            const item = new vscode.CompletionItem(type, vscode.CompletionItemKind.Keyword);
            item.detail = 'Built-in type';
            completions.push(item);
        });

        builtinFunctions.forEach(func => {
            const item = new vscode.CompletionItem(func, vscode.CompletionItemKind.Function);
            item.detail = 'Built-in function';
            item.insertText = `${func}()`;
            item.range = new vscode.Range(position, position);
            completions.push(item);
        });

        builtinKeywords.forEach(keyword => {
            const item = new vscode.CompletionItem(keyword, vscode.CompletionItemKind.Keyword);
            item.detail = 'Keyword';
            completions.push(item);
        });

        return completions;
    }

    private isInsideLevel2Macro(document: vscode.TextDocument, position: vscode.Position): boolean {
        const text = document.getText();
        const offset = document.offsetAt(position);
        
        for (let i = offset; i >= 0; i--) {
            if (i >= 2 && text.substr(i, 3) === '##$') {
                for (let j = i + 3; j < offset; j++) {
                    if (text[j] === '#' && text[j + 1] !== '#') {
                        return false;
                    }
                }
                return true;
            }
            if (text[i] === '\n') {
                let hasHash = false;
                for (let k = i - 1; k >= 0 && text[k] !== '\n'; k--) {
                    if (text[k] === '#') {
                        hasHash = true;
                        break;
                    }
                }
                if (!hasHash) {
                    return false;
                }
            }
        }
        return false;
    }

    private isInsideExecBlock(document: vscode.TextDocument, position: vscode.Position): boolean {
        const text = document.getText();
        const offset = document.offsetAt(position);
        
        let braceCount = 0;
        let insideExec = false;
        
        for (let i = offset; i >= 0; i--) {
            if (i >= 4 && text.substr(i, 5) === 'exec:') {
                insideExec = true;
                for (let j = i + 5; j < offset; j++) {
                    if (text[j] === '{') {
                        break;
                    }
                }
                break;
            }
        }
        
        if (!insideExec) {
            return false;
        }
        
        for (let i = offset; i >= 0; i--) {
            if (text[i] === '}') {
                braceCount++;
            } else if (text[i] === '{') {
                braceCount--;
                if (braceCount < 0) {
                    break;
                }
            }
        }
        
        return braceCount >= 0;
    }
}

class EDocumentFormattingEditProvider implements vscode.DocumentFormattingEditProvider {
    provideDocumentFormattingEdits(
        document: vscode.TextDocument,
        options: vscode.FormattingOptions,
        token: vscode.CancellationToken
    ): vscode.ProviderResult<vscode.TextEdit[]> {
        const edits: vscode.TextEdit[] = [];
        const text = document.getText();
        const lines = text.split('\n');
        const formattedLines: string[] = [];
        let indentLevel = 0;
        const indentSize = options.tabSize || 4;
        let insideLevel2Macro = false;
        let insideExecBlock = false;
        let execBlockBraceCount = 0;

        for (let i = 0; i < lines.length; i++) {
            const line = lines[i];
            const trimmedLine = line.trim();

            if (trimmedLine.startsWith('##$')) {
                insideLevel2Macro = true;
                indentLevel = 0;
            }

            if (trimmedLine.includes('exec:')) {
                const braceIndex = trimmedLine.indexOf('{');
                if (braceIndex !== -1) {
                    insideExecBlock = true;
                    execBlockBraceCount = 1;
                }
            }

            let shouldDeindent = trimmedLine.startsWith('}') || trimmedLine.startsWith('else');
            
            if (insideExecBlock) {
                for (const char of trimmedLine) {
                    if (char === '}') {
                        execBlockBraceCount--;
                        if (execBlockBraceCount === 0) {
                            insideExecBlock = false;
                            indentLevel = Math.max(1, indentLevel - 1);
                        }
                    } else if (char === '{') {
                        execBlockBraceCount++;
                    }
                }
                shouldDeindent = false;
            }

            if (shouldDeindent) {
                indentLevel = Math.max(0, indentLevel - 1);
            }

            let currentIndentLevel = indentLevel;
            
            if (insideLevel2Macro && !trimmedLine.startsWith('##$')) {
                currentIndentLevel = 1;
            }

            if (insideExecBlock) {
                currentIndentLevel = indentLevel + 1;
            }

            const indent = ' '.repeat(currentIndentLevel * indentSize);
            formattedLines.push(indent + trimmedLine);

            let shouldIndent = trimmedLine.endsWith('{');
            
            if (insideLevel2Macro && trimmedLine.endsWith(':') && !trimmedLine.includes('exec:')) {
                shouldIndent = true;
            }

            if (shouldIndent) {
                indentLevel++;
            }

            if (insideLevel2Macro && i + 1 < lines.length) {
                const nextLine = lines[i + 1].trim();
                if (nextLine.length > 0 && !nextLine.startsWith('#') && !nextLine.endsWith(';')) {
                    insideLevel2Macro = false;
                    indentLevel = Math.max(0, indentLevel - 1);
                }
            }
        }

        const formattedText = formattedLines.join('\n');
        edits.push(
            vscode.TextEdit.replace(
                new vscode.Range(0, 0, document.lineCount, 0),
                formattedText
            )
        );

        return edits;
    }
}

class EDiagnosticProvider {
    private diagnosticCollection: vscode.DiagnosticCollection;
    private config: vscode.WorkspaceConfiguration;

    constructor() {
        this.diagnosticCollection = vscode.languages.createDiagnosticCollection('e');
        this.config = vscode.workspace.getConfiguration('elhle');
    }

    public updateDiagnostics(document: vscode.TextDocument): void {
        const diagnostics: vscode.Diagnostic[] = [];
        const filePath = document.uri.fsPath;
        const tempOutputPath = path.join(path.dirname(filePath), '_output.c');

        this.config = vscode.workspace.getConfiguration('elhle');
        const compilerPath = this.config.get<string>('compilerPath', '');
        const compilerArgs = this.config.get<string[]>('compilerArgs', ['--debug']);
        const enableLinting = this.config.get<boolean>('enableLinting', true);

        if (!enableLinting) {
            this.diagnosticCollection.set(document.uri, []);
            return;
        }

        if (!compilerPath || compilerPath.trim() === '') {
            const warningMsg = 'E compiler path not configured. Please set "elhle.compilerPath" in settings.';
            diagnostics.push({
                range: new vscode.Range(0, 0, 0, 0),
                message: warningMsg,
                severity: vscode.DiagnosticSeverity.Warning,
                source: 'E Language Plugin'
            });
            this.diagnosticCollection.set(document.uri, diagnostics);
            return;
        }

        try {
            const args = [...compilerArgs, `"${filePath}"`];
            const result = child_process.execSync(`"${compilerPath}" ${args.join(' ')}`, {
                cwd: path.dirname(filePath),
                encoding: 'utf8',
                timeout: 10000
            });

            if (fs.existsSync(tempOutputPath)) {
                try {
                    child_process.execSync(`gcc -o _output.exe _output.c`, {
                        cwd: path.dirname(filePath),
                        encoding: 'utf8',
                        timeout: 10000
                    });
                } catch (error: any) {
                    const errorOutput = error.stdout + error.stderr;
                    const errorLines = errorOutput.split('\n');
                    
                    errorLines.forEach((line: string) => {
                        const match = line.match(/_output\.c:(\d+):(\d+):\s*(error|warning):\s*(.*)/);
                        if (match) {
                            const lineNumber = parseInt(match[1]) - 1;
                            const column = parseInt(match[2]) - 1;
                            const severity = match[3] === 'error' ? vscode.DiagnosticSeverity.Error : vscode.DiagnosticSeverity.Warning;
                            const message = match[4];

                            diagnostics.push({
                                range: new vscode.Range(lineNumber, column, lineNumber, column + 1),
                                message: message,
                                severity: severity,
                                source: 'GCC'
                            });
                        }
                    });
                }

                if (fs.existsSync(tempOutputPath)) {
                    fs.unlinkSync(tempOutputPath);
                }
                if (fs.existsSync(path.join(path.dirname(filePath), '_output.exe'))) {
                    fs.unlinkSync(path.join(path.dirname(filePath), '_output.exe'));
                }
            }
        } catch (error: any) {
            const errorOutput = error.stdout + error.stderr;
            const errorLines = errorOutput.split('\n');
            
            errorLines.forEach((line: string) => {
                const match = line.match(/(.*):(\d+):(\d+):\s*(error|warning):\s*(.*)/);
                if (match) {
                    const lineNumber = parseInt(match[2]) - 1;
                    const column = parseInt(match[3]) - 1;
                    const severity = match[4] === 'error' ? vscode.DiagnosticSeverity.Error : vscode.DiagnosticSeverity.Warning;
                    const message = match[5];

                    diagnostics.push({
                        range: new vscode.Range(lineNumber, column, lineNumber, column + 1),
                        message: message,
                        severity: severity,
                        source: 'E Compiler'
                    });
                }
            });
        }

        this.diagnosticCollection.set(document.uri, diagnostics);
    }

    public dispose(): void {
        this.diagnosticCollection.dispose();
    }
}

export function activate(context: vscode.ExtensionContext) {
    console.log('E Language Support activated');

    const completionDisposable = vscode.languages.registerCompletionItemProvider(
        'e',
        new ECompletionItemProvider(),
        ' ',
        '.',
        '('
    );

    const formattingDisposable = vscode.languages.registerDocumentFormattingEditProvider(
        'e',
        new EDocumentFormattingEditProvider()
    );

    const diagnosticProvider = new EDiagnosticProvider();

    const changeDisposable = vscode.workspace.onDidChangeTextDocument(event => {
        if (event.document.languageId === 'e') {
            diagnosticProvider.updateDiagnostics(event.document);
        }
    });

    const openDisposable = vscode.workspace.onDidOpenTextDocument(document => {
        if (document.languageId === 'e') {
            diagnosticProvider.updateDiagnostics(document);
        }
    });

    const configChangeDisposable = vscode.workspace.onDidChangeConfiguration(event => {
        if (event.affectsConfiguration('elhle')) {
            vscode.workspace.textDocuments.forEach(document => {
                if (document.languageId === 'e') {
                    diagnosticProvider.updateDiagnostics(document);
                }
            });
        }
    });

    context.subscriptions.push(
        completionDisposable,
        formattingDisposable,
        changeDisposable,
        openDisposable,
        configChangeDisposable,
        diagnosticProvider
    );
}

export function deactivate() {
    console.log('E Language Support deactivated');
}
