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

## get started

```bash
cargo install --path .
glide run hello.glide
```

Examples in `examples/`. Editor support in `zed-extension/`.

## license

MIT.
