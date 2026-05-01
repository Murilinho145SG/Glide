# Zed extension for Glide

Registers the Glide language, launches `glide lsp`, and ships a Tree-sitter
grammar.

LSP features: diagnostics, hover, goto-definition, completion (keywords,
globals, struct fields after `.`), document formatting.

## Install

1. Put `glide` on `PATH`:

   ```bash
   cargo install --path ..
   ```

2. Generate the Tree-sitter parser (one-time):

   ```bash
   cargo install tree-sitter-cli
   cd grammars/glide
   tree-sitter generate
   ```

3. In Zed, command palette: `zed: install dev extension`, pick this directory.

## Updating the grammar

After editing `grammar.js` or `queries/highlights.scm`:

```bash
cd zed-extension/grammars/glide
tree-sitter generate
cd ../../..
git add zed-extension/grammars/glide
git commit -m "grammar: ..."
git rev-parse HEAD
```

Paste the SHA into `commit = "..."` in `extension.toml`, then
`zed: rebuild dev extension` in Zed.

## Updating the compiler

```bash
cargo install --path .. --force
```

Reopen the workspace so Zed re-spawns the LSP.

## Format-on-save

Add to your Zed `settings.json`:

```json
{
  "languages": {
    "Glide": {
      "format_on_save": "on",
      "formatter": "language_server"
    }
  }
}
```
