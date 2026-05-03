# Changelog

## 0.1.0 — 2026-05-03

First release. Glide is now a self-hosted, plug-and-play systems language with no Rust or system C compiler required.

### Language

- Borrow check, function-scoped, no lifetime annotations. 7 rules: borrow in struct field, call-site aliasing, dangling return, owned escape, owned move, owned-into-`*T` param, overlapping borrows across statements.
- Auto-drop with explicit `let v* = expr` marker (block-scoped) plus the implicit `let p: *T = T { ... }` struct-lit pattern. Cleanup picks `v.free()` when the type defines one, else `free(v as *void)`.
- Errors as values: `!T` result type, `?` propagation, `ok` / `err` / `unwrap` builtins.
- Generics with monomorphization. Inference works from arguments, return-type hints, and from the first method call on a `let v = Generic::new()` binding (deferred inference).
- Concurrency: `chan<T>` and `spawn` over a per-T pthread runtime.
- Closures: anonymous `fn(args) -> ret { ... }` lifted to top-level (capture-less).
- Defer (fn-scoped, Go-like).
- Arena allocator.
- FFI: `extern fn`, `c_include`, `c_link`, opaque types.
- Slices, generics, enums + pattern match, `interface`, type aliases.

### Toolchain

- Single archive bundles everything: the `glide` binary, the `stdlib/`, and a Zig toolchain (clang + libcs + linker for cross-compile).
- Subcommands: `glide build / run / emit / check / fmt / lsp`.
- Cross-compilation via `--target=<triple>` (e.g. `x86_64-linux-gnu`, `aarch64-macos`, `x86_64-windows-gnu`).
- Output binary is a normal native exe with no Glide / Zig runtime dependency.
- `tools/install.{sh,ps1}` install the archive into `~/.local/share/glide` (Linux/macOS) or `%LOCALAPPDATA%\Programs\Glide` (Windows). No admin required.

### LSP

`glide lsp` over JSON-RPC stdio. Supports:

- Diagnostics with 18 stable codes covering type errors, all 7 borrow rules, null-safety (no null borrows, no null auto-drop), unused vars / fns / params, unnecessary `mut`, dead code after return, missing return, arena leaks, `&temporary` (dangling address), and a `large-return` performance hint.
- Hover, document symbols, goto definition, find references, document highlight, rename, completion (locals + top-level + keywords; trigger `.` and `:`).
- Document formatting.

### Editors

- **Zed**: extension under `zed-extension/` invokes `glide lsp`. Tree-sitter grammar at `glide-grammar/`.
- **VSCode**: extension under `vscode-extension/` with file-extension icon, syntax highlighting (TextMate), and LSP integration via `vscode-languageclient`.

### Bootstrap

- Compiler is ~10K LOC of Glide under `bootstrap/`.
- `bootstrap/seed/bootstrap.c` is the auto-emitted C seed (~11K LOC) for new machines: `cc bootstrap/seed/bootstrap.c -o glide_seed && ./glide_seed build bootstrap/main.glide -o glide`.

### Known limitations

- Formatter drops comments (lexer doesn't track them yet) — `glide fmt --write` is opt-in for that reason.
- LSP doesn't follow imports during analysis (single-file scope).
- No NLL — borrow lifetimes are block-scoped.
- No `move` returns — owned values can't escape their declaring fn.
- No `?T` nullable type yet — only borrows are non-null by check; `*T` remains nullable.
- Closures with capture (`move`) work only in the legacy Rust path; bootstrap supports capture-less only.
