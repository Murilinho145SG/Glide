<p align="center">
  <img src="assets/glide-horizontal.svg" alt="Glide" width="320">
</p>

**Glide gives you memory safety, real concurrency, and the speed of C, without lifetime annotations, callbacks, or a garbage collector.**

A small systems language. Small enough to learn in an afternoon.

## the problems Glide solves

### "I want safety, but `'a, 'b, 'q: 'a` makes me cry."

Borrows in Glide are function-scoped. The compiler catches dangling references, double-free, aliased mutation, and owned values trying to escape their scope. You never type a single lifetime parameter.

```glide
fn dangle() -> &int {
    let x: int = 5;
    return &x;          // rejected at compile time
}
```

### "I want heap allocation without `free()` everywhere."

Declare a `*T` from a struct literal and Glide pairs the allocation with automatic cleanup at scope exit. When you have a pile of objects that all live and die together, reach for an arena.

```glide
fn process() {
    let p: *Point = Point { x: 1, y: 2 };   // allocates
    use(p);
}                                            // freed automatically
```

```glide
let arena: *Arena = Arena::new(4096);
defer arena.free();
let n1: *Node = arena.create(Node);
let n2: *Node = arena.create(Node);
// freed together when the arena drops
```

### "I want real concurrency, not callback soup."

Channels and `spawn`. Shared-nothing by design. No `Arc<Mutex<...>>`, no async coloring, no executor to configure.

```glide
fn worker(c: chan<int>) {
    send(c, 42);
}

fn main() -> int {
    let c: chan<int> = make_chan(1);
    spawn worker(c);
    return recv(c);
}
```

### "I want errors as values, not exceptions or `Result<T, Box<dyn Error>>`."

`!T` is the result type. `?` propagates errors. That's the whole story.

```glide
fn parse(n: int) -> !int {
    if n < 0 { return err("negative"); }
    return ok(n * 2);
}

fn double(n: int) -> !int {
    let v: int = parse(n)?;
    return ok(v + 1);
}
```

### "I want generics that aren't a PhD thesis."

Monomorphized, inferred from arguments and return-type hints. No `::<T>` turbofish needed in the common case.

```glide
let v: *Vector<int> = Vector::new();
v.push(10);
v.push(20);

let m: *HashMap<int> = HashMap::new();
m.insert("answer", 42);
```

## install

Glide ships as a single archive that contains the compiler and a bundled C toolchain (Zig). No system gcc, clang, or Rust required.

**Linux / macOS:**

```bash
curl -sSf https://github.com/.../releases/latest/download/install.sh | bash
```

**Windows (PowerShell):**

```powershell
irm https://github.com/.../releases/latest/download/install.ps1 | iex
```

Or download the archive for your platform from releases and run `tools/install.{sh,ps1} --archive <path>`.

## use

```bash
glide run hello.glide
glide build hello.glide -o hello
glide build hello.glide --target=x86_64-linux-gnu     # cross-compile
glide check hello.glide
glide emit hello.glide                                 # show generated C
```

Cross-compile targets are anything Zig supports: `x86_64-linux-{gnu,musl}`, `aarch64-linux-{gnu,musl}`, `x86_64-windows-{gnu,msvc}`, `aarch64-macos`, `x86_64-macos`, `riscv64-linux-musl`, etc.

## build from source

The compiler is written in Glide. To build it from scratch you only need a C compiler:

```bash
git clone <repo> && cd glide
cc bootstrap/seed/bootstrap.c -o glide_seed -O2 -lpthread -lm     # 1. seed
bash tools/install_zig.sh                                          # 2. fetch Zig
./glide_seed build bootstrap/main.glide -o glide                   # 3. self-host
bash tools/build_release.sh                                        # 4. (optional) make a release archive
```

Examples live in `examples/`. Editor support (Zed grammar) in `zed-extension/`.

## license

MIT.
