# Glide for Visual Studio Code

Syntax highlighting + Language Server Protocol integration for the Glide programming language.

## Features

- Diagnostics: parse errors, type errors, borrow checker, null safety, unused vars, dead code, missing return, arena leaks, large-return performance hints
- Hover: function signatures, struct/enum declarations, built-in types
- Goto definition
- Find references
- Document highlight
- Document symbols (outline)
- Completion: locals, top-level decls, keywords
- Rename refactor

## Requirements

The `glide` binary must be on your PATH (or set `glide.path` in settings).

## Build

```bash
cd vscode-extension
npm install
npm run compile
```

To launch a development host:

1. Open this folder in VSCode
2. Press `F5`
3. A new VSCode window opens with the extension loaded
4. Open any `.glide` file

To produce a `.vsix` for installation:

```bash
npm install -g @vscode/vsce
vsce package
code --install-extension glide-0.1.0.vsix
```

## Settings

| Key                   | Default | Description                                    |
| --------------------- | ------- | ---------------------------------------------- |
| `glide.path`          | `glide` | Path to the `glide` executable.                |
| `glide.trace.server`  | `off`   | LSP message tracing (`off` / `messages` / `verbose`). |
