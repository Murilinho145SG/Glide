# Developing the Glide compiler

This document is for people working on the compiler itself. If you just want to use Glide, see `README.md`.

## the bootstrap loop

The compiler is written in Glide. To break the chicken-and-egg, `bootstrap/seed/bootstrap.c` is an auto-emitted C version that any C compiler can build.

```bash
# 1. Build the seed binary (any cc — gcc, clang, MinGW, ...)
cc bootstrap/seed/bootstrap.c -o glide_seed -O2 -lpthread -lm

# 2. Fetch the bundled Zig toolchain (used as the C backend)
bash tools/install_zig.sh

# 3. Use the seed to build the real compiler
./glide_seed build bootstrap/main.glide -o glide

# 4. (optional) verify self-host
./glide build bootstrap/main.glide -o glide_gen2
./glide_gen2 build bootstrap/main.glide -o glide_gen3   # should match
```

Once `glide` works, you don't need `glide_seed` again unless you change a runtime helper that the seed itself uses (rare).

## project structure

```
bootstrap/
  main.glide        driver: arg parsing, build/run/emit/check/fmt/lsp
  lexer.glide       byte stream -> tokens
  parser.glide      tokens -> AST
  ast.glide         Stmt / Expr / Type definitions
  typer.glide       type + borrow checker (collects diagnostics)
  codegen.glide     AST -> C source (also emits the runtime)
  fmt.glide         AST -> canonical Glide source
  lsp.glide         JSON-RPC server (uses everything above)
  json.glide        minimal JSON parser + emitter for the LSP

  seed/bootstrap.c  auto-emitted C version of the compiler

stdlib/
  vector.glide      Vector<T>
  hashmap.glide     HashMap<V> with string keys

tools/
  install_zig.{sh,ps1}    download a Zig release into runtime/zig/
  install.{sh,ps1}        install a Glide release archive
  build_release.sh        package glide+stdlib+Zig into a tarball/zip
  gen_icons.py            rasterize the logo SVG to PNGs

zed-extension/             Zed editor support (LSP wiring + tree-sitter)
vscode-extension/          VSCode extension (LSP wiring + tmLanguage)
glide-grammar/             tree-sitter grammar (used by Zed)

assets/                    logos and icons
```

## edit-build cycle

When you change anything under `bootstrap/`:

```bash
./glide build bootstrap/main.glide -o glide_new
mv glide_new glide          # replace the running compiler
./glide check some_test.glide
```

If the change touches `emit_stdlib_runtime` in `codegen.glide` AND your change requires the new helpers to be present in the seed-built binary, you also need to patch `bootstrap/seed/bootstrap.c` manually (add both the C function bodies and their corresponding `printf` lines that re-emit them on the next compile). This is the only chicken-and-egg case; other changes propagate naturally.

After a meaningful change, regenerate the seed so new contributors don't carry your patches:

```bash
./glide emit bootstrap/main.glide > bootstrap/seed/bootstrap.c
```

Verify the new seed compiles cleanly:

```bash
cc bootstrap/seed/bootstrap.c -o /tmp/seed_check -O2 -lpthread -lm
```

## testing

There's no test runner yet. The verification flow is:

```bash
# Self-host (3 generations should produce identical compilers)
./glide build bootstrap/main.glide -o gen2
./gen2 build bootstrap/main.glide -o gen3

# Smoke test
echo 'fn main() -> int { return 42; }' > /tmp/h.glide
./glide run /tmp/h.glide                    # exit 42

# LSP smoke test
echo '{...}' | ./glide lsp                   # see ad-hoc Python tests
```

Adding proper tests is on the roadmap.

## working on the LSP

Run `glide lsp` and pipe LSP requests via stdin. The protocol uses `Content-Length: N\r\n\r\n<payload>` framing. Easiest to drive from a small Python script that builds the right JSON-RPC envelopes.

For interactive testing, install the Zed or VSCode extension, point it at your in-development `glide` binary (PATH or `glide.path` setting), and check the server's `--trace` channel.

## working on the formatter

`bootstrap/fmt.glide` walks the AST and emits canonical text. The formatter can run on any `.glide` file:

```bash
./glide fmt bootstrap/codegen.glide          # to stdout
./glide fmt foo.glide --write                # rewrite in place
```

Round-trip stability is the primary invariant: `format(format(X)) == format(X)`. Add a test by passing a representative file through twice and `diff`-ing.

## releases

```bash
# Bump VERSION in tools/install*.sh, tools/build_release.sh, CHANGELOG.md, etc.
# Tag the commit
git tag v0.X.Y

# Build for each target platform
bash tools/build_release.sh                              # host
bash tools/build_release.sh --target=x86_64-linux        # cross
bash tools/build_release.sh --target=aarch64-macos       # cross

# Push and publish
git push origin main && git push origin v0.X.Y
gh release create v0.X.Y dist/glide-*.{zip,tar.gz} \
    tools/install.sh tools/install.ps1 \
    --title "Glide v0.X.Y" --notes-file CHANGELOG.md
```

The install scripts default to GitHub's `releases/latest/download/` so users get the latest tag automatically.

## known constraints

- The bootstrap parser doesn't track end-position info per node, so the LSP and the formatter can only point at the start of a token.
- The lexer drops comments. The formatter therefore drops them too. Adding a comment-aware lexer + formatter is the next iteration.
- LSP analysis is single-file (no cross-file resolution beyond what the typer already does for `pub` symbols).
- No `?T` nullable type yet — only `&T` borrows are non-null by typer enforcement. `*T` may be null at runtime.
