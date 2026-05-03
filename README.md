<p align="center">
  <img src="assets/glide-horizontal.svg" alt="Glide" width="320">
</p>

A small statically typed systems language. Pointers, no GC, Go-style channels.
The compiler is written in Rust and transpiles to C.

```glide
struct Point {
    x: int,
    y: int,
}

fn translate(p: *Point, dx: int, dy: int) {
    p->x += dx;
    p->y += dy;
}

fn worker(c: chan<int>) {
    send(c, 42);
}

fn main() -> int {
    let p: *Point = new Point { x: 3, y: 4 };
    translate(p, 10, 20);
    printf("(%d, %d)\n", p->x, p->y);
    free(p);

    let c: chan<int> = make_chan(1);
    spawn worker(c);
    printf("got %d\n", recv(c));
    return 0;
}
```

## Build

Requires Rust and a C compiler (`gcc` or `clang`) on `PATH`.

```bash
cargo install --path .
```

## Use

```bash
glide build hello.glide -o hello
./hello

glide run hello.glide          # build to a tempfile and run
glide emit hello.glide         # print the generated C
glide fmt hello.glide          # pretty-print to stdout
glide fmt hello.glide --write  # rewrite the file in place
glide lsp                      # start the LSP server on stdio
```

Programs that use `chan` / `spawn` get linked with `-lpthread` automatically.

## Editor

The `zed-extension/` directory has a Zed extension that wires the LSP and a
Tree-sitter grammar. See `zed-extension/README.md`.

## Examples

`examples/*.glide` covers each language feature end-to-end.

## License

MIT.
