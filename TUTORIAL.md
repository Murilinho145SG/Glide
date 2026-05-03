# Glide tutorial

Walks you from a fresh install through writing, building, and running Glide programs. Assumes you've installed Glide via the one-liner from the README.

```bash
glide --help
```

If that prints the usage, you're set.

## hello world

Save this as `hello.glide`:

```glide
fn main() -> int {
    println!("hello, glide");
    return 0;
}
```

Run it:

```bash
glide run hello.glide
```

`glide run` compiles to a temp executable, runs it, then deletes it. To keep the binary, use `glide build hello.glide -o hello`.

## values and bindings

Glide has a small set of primitive types: `int`, `uint`, `i32`, `i64`, `u32`, `u64`, `usize`, `isize`, `f32`, `f64`, `float`, `bool`, `char`, `string`.

```glide
let x: int = 42;            // immutable
let mut y: int = 0;         // mutable
y = x + 1;
let pi: float = 3.14;
let ok: bool = true;
let name: string = "glide";
```

Type annotations are optional when the compiler can infer:

```glide
let v = Vector::new();      // T inferred from later v.push(...)
v.push(10);                 // T = int
```

## functions

```glide
fn add(a: int, b: int) -> int {
    return a + b;
}

fn greet(name: string) {
    println!("hello,", name);
}
```

`fn` declarations stand alone or live inside `impl` blocks for methods.

## structs and methods

```glide
struct Point {
    x: int,
    y: int,
}

impl Point {
    fn distance(self: *Point, other: *Point) -> int {
        let dx: int = self.x - other.x;
        let dy: int = self.y - other.y;
        return dx * dx + dy * dy;
    }
}
```

Call a method as `p.distance(q)`. The compiler auto-borrows where needed.

## memory

Three categories of values you actually allocate:

**stack** — value types, automatic:

```glide
let p: Point = Point { x: 1, y: 2 };   // lives on the stack
```

**owned heap with auto-drop** — your binding owns it; freed at scope exit:

```glide
fn process() {
    let v* = Vector::new();    // malloc'd
    v.push(10);
    v.push(20);
}                              // automatic v.free() here
```

The `*` after the binding name is the explicit "this is owned, clean up at scope end" marker. Works for any expression that returns a pointer.

**arena** — group of allocations with a shared lifetime:

```glide
let arena: *Arena = Arena::new(4096);
defer arena.free();

let p: *Point = arena.create(Point);
let q: *Point = arena.create(Point);
// freed together when arena.free() runs
```

## borrows

`&T` and `&mut T` are non-owning views. They can't be null and can't outlive the function.

```glide
fn touch(p: &Point) -> int {
    return p.x + p.y;
}

fn main() -> int {
    let p: Point = Point { x: 3, y: 4 };
    return touch(&p);
}
```

The compiler enforces:

- A value can have many `&T` viewers OR one `&mut T` viewer, not both
- A borrow can't be passed back as a function return when its source was a local
- Two arguments to the same call can't alias the same variable if any is `&mut`

You never write lifetime annotations.

## errors as values

`!T` is a result type. `?` propagates errors. `ok` and `err` build them.

```glide
fn parse_pos(n: int) -> !int {
    if n < 0 { return err("negative"); }
    return ok(n * 2);
}

fn pipeline(n: int) -> !int {
    let v: int = parse_pos(n)?;     // if err, return err from pipeline
    return ok(v + 1);
}

fn main() -> int {
    let r: !int = pipeline(5);
    return unwrap(r);                // 11
}
```

## generics

Type parameters use angle brackets. Inference works from arguments and from later uses of the binding.

```glide
fn first<T>(v: *Vector<T>) -> T {
    return v.get(0);
}

let v = Vector::new();           // T deferred
v.push(42);                      // T = int
let n: int = first(v);           // 42
```

The stdlib ships `Vector<T>` and `HashMap<V>`.

## concurrency

`chan<T>` is a typed channel. `spawn` runs a function on a new thread.

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

Shared-nothing by design. No locks, no async coloring.

## next steps

- Read `LANGUAGE.md` for the full reference
- Browse `examples/` for working programs covering each feature
- Try `glide check foo.glide` to see the compiler's diagnostics
- Wire up the LSP (`glide lsp`) in your editor for completion, hover, and goto
