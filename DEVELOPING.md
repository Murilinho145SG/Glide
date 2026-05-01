# Developing Glide

## Module map

| Path | What it does |
| --- | --- |
| `src/lexer/` | tokens, keywords, operators |
| `src/parser/` | Pratt expression parser, statements |
| `src/ast/` | AST types |
| `src/typer/` | type checker, symbol index, UFCS lookup, stdlib registration |
| `src/codegen/` | C transpiler, C runtime helpers (`STDLIB_C`) |
| `src/fmt/` | pretty-printer |
| `src/lsp.rs` | language server (hover, goto, completion, formatting, auto-import) |
| `src/main.rs` | driver: `build` / `run` / `emit` / `fmt` / `lsp`, recursive import resolution |
| `std/` | stdlib written in Glide |
| `glide-grammar/` | tree-sitter grammar (regenerate `src/parser.c` after editing `grammar.js`) |
| `zed-extension/` | Zed dev extension (LSP launcher + language config + queries) |

## Build / install / test

```bash
cargo build
cargo install --path . --force         # update PATH binary; close Zed first
glide run examples/<file>.glide
glide fmt examples/<file>.glide
glide emit examples/<file>.glide       # print generated C
glide lsp                              # start language server on stdio
```

## Faster iteration

```bash
cargo install cargo-watch
cargo watch -x "run --quiet -- run examples/array.glide"
```

Re-runs on every save.

For just type-checking without invoking gcc, run `cargo run --quiet -- emit <file>` and discard stdout.

## Recipes

### Add a keyword

1. `src/lexer/keywords.rs`: add enum variant + entry in `to_lexeme` + entry in `from_str`.
2. `src/parser/mod.rs`: handle it in the right spot (usually `parse_stmt_kind` or `parse_prefix`).
3. `src/ast/mod.rs`: add the AST variant if the keyword introduces a new construct.
4. `src/typer/mod.rs`: add a check arm; pre-register if it declares something.
5. `src/codegen/mod.rs`: emit C for the new node.
6. `src/fmt/mod.rs`: pretty-print the new node.
7. `glide-grammar/grammar.js`: add the grammar rule.
8. `zed-extension/languages/glide/highlights.scm` and `glide-grammar/queries/highlights.scm`: add to the keyword list.
9. `cd glide-grammar && tree-sitter generate`.
10. Commit, then bump `commit` SHA in `zed-extension/extension.toml`.

### Add a Rust-side builtin (printf-like)

`src/typer/mod.rs::register_builtins` — append an entry. If it maps to libc, codegen needs no changes.

### Add a Rust-side stdlib helper for a primitive method

1. `src/typer/mod.rs::stdlib_signatures` — append `(name, params, ret)`. The name controls UFCS dispatch: `int_foo` enables `n.foo()` on int.
2. `src/codegen/mod.rs::STDLIB_C` — add the `__glide_<name>` C function definition.

### Add a stdlib module in Glide

1. Create `std/<name>.glide`.
2. Import in user code with `import "std/<name>";`.
3. To make a function callable as `obj.method(args)`, name it `<typemangle>_<method>`:
   - `int.foo` → `fn int_foo(self: int, ...)`
   - `string.foo` → `fn string_foo(self: string, ...)`
   - `*Point.foo` → `fn Point_ptr_foo(self: *Point, ...)`
   - `*int.foo` → `fn int_ptr_foo(self: *int, ...)`

The LSP auto-discovers any `.glide` file under `std/` in any ancestor directory, so completion offers methods even before the user adds the import.

### Add an interface and impl

```glide
interface Greet {
    fn hi(self: string) -> string;
}

impl Greet for string {
    fn hi(self: string) -> string {
        return "hello, ".concat(self);
    }
}
```

The `impl` registers `string_hi` under the hood. UFCS makes `"world".hi()` resolve to it.

### Regenerate the tree-sitter parser

```bash
cd glide-grammar
tree-sitter generate
cd ..
git add glide-grammar
git commit -m "grammar: ..."
git rev-parse HEAD                     # paste into zed-extension/extension.toml
git add zed-extension/extension.toml
git commit -m "chore(zed): re-pin grammar"
```

In Zed: `zed: rebuild dev extension`.

## How the LSP loads context

On every keystroke (`did_change`):

1. Parse the current file.
2. Walk ancestor directories looking for a `std/` folder.
3. For every `.glide` file under that `std/`, parse and pre-register its top-level decls in the `Typer` (with `module = "std/foo"` and `file = path`).
4. Follow the file's own `import` statements and pre-register those too.
5. Type-check the current file with all those decls visible.
6. If parsing fails, keep the previous good `SymbolIndex` so completion / hover still work mid-edit.

This is what lets `"x".blue()` complete even when `import "std/color"` isn't yet typed; the auto-import edit is attached to the completion item and applied when the user accepts.

## Common gotchas

- `cargo install` fails with "permission denied" on Windows: Zed has `glide.exe` locked. Close Zed first.
- Zed colors stale: regenerate grammar, re-pin SHA, `zed: rebuild dev extension`.
- Method lookup misses: the function name has to match the mangled prefix exactly. `Point_ptr_translate` vs `Point_translate` matters; check `Type::mangle()` in `src/types/mod.rs`.
- Format / parse round-trip should be idempotent. `cargo run -- fmt <file>` twice should produce the same output. Failures usually mean the formatter is missing a case for a new AST node.
