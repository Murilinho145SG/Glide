# Glide tutorial

From zero to a real program. Each section's code runs as-is with `glide run <file>.glide`.

For the full list of features and what's missing, see `LANGUAGE.md`. For working on the compiler itself, see `DEVELOPING.md`.

---

## 1. Hello world

```glide
fn main() -> int {
    printf("hello\n");
    return 0;
}
```

Every program needs `fn main`. Returns int (process exit code). `printf` is the libc one, format strings work the same.

---

## 2. Variables

```glide
fn main() -> int {
    let n: int = 42;
    let pi: float = 3.14;
    let name: string = "Glide";
    let ok: bool = true;
    let c: char = 'a';

    let inferred = 100;          // type comes from the value (int)
    const MAX: int = 1_000_000;  // const is immutable

    printf("%d %f %s\n", n, pi, name);
    return 0;
}
```

`let` is mutable, `const` isn't. Underscores in numbers are visual only.

---

## 3. Functions

```glide
fn add(a: int, b: int) -> int {
    return a + b;
}

fn greet(name: string) {
    printf("hello, %s\n", name);
}

fn main() -> int {
    let s: int = add(3, 4);
    printf("%d\n", s);
    greet("world");
    return 0;
}
```

`-> Type` for the return. Omit it when the function returns nothing.

---

## 4. if / while / for

```glide
fn main() -> int {
    let n: int = 7;

    if n > 10 {
        printf("big\n");
    } else if n > 5 {
        printf("medium\n");
    } else {
        printf("small\n");
    }

    let i: int = 0;
    while i < 3 {
        printf("while %d\n", i);
        i++;
    }

    for let j: int = 0; j < 3; j++ {
        printf("for %d\n", j);
    }

    return 0;
}
```

No parentheses around conditions. Body is always braced.

---

## 5. Pointers and memory

Glide has no garbage collector. You allocate with `malloc` and free with `free`.

```glide
fn add_offset(p: *int) {
    *p += 100;
}

fn main() -> int {
    let x: int = 5;
    add_offset(&x);
    printf("%d\n", x);  // 105

    let p: *int = malloc(sizeof(int)) as *int;
    *p = 42;
    printf("%d\n", *p);
    free(p as *void);

    return 0;
}
```

`*T` is "pointer to T". `&x` takes the address. `*p` reads/writes through it.
`sizeof` returns the size in bytes. `as` is a cast.

---

## 6. Structs

```glide
struct Point {
    x: int,
    y: int,
}

fn main() -> int {
    let p: Point = Point { x: 3, y: 4 };
    printf("(%d, %d)\n", p.x, p.y);

    let h: *Point = new Point { x: 10, y: 20 };  // heap-allocated
    printf("(%d, %d)\n", h.x, h.y);
    free(h as *void);

    return 0;
}
```

`new T { ... }` is `malloc + struct init` in one expression. Returns `*T`.

`.` is the only field access. It works on values and auto-derefs on pointers, so `h.x` works whether `h` is `Point` or `*Point`.

---

## 7. Methods (UFCS and impl)

Any free function whose first parameter is `T` can be called as a method on a `T`. Three styles:

**Style 1 — free function with name convention:**

```glide
fn Point_distance(a: Point, b: Point) -> int {
    let dx: int = a.x - b.x;
    let dy: int = a.y - b.y;
    return dx * dx + dy * dy;
}

let p: Point = Point { x: 0, y: 0 };
let q: Point = Point { x: 3, y: 4 };
printf("%d\n", p.distance(q));   // 25
```

**Style 2 — `impl Type` block:**

```glide
impl Point {
    fn distance(self: Point, other: Point) -> int {
        let dx: int = self.x - other.x;
        let dy: int = self.y - other.y;
        return dx * dx + dy * dy;
    }

    fn translate(self: *Point, dx: int, dy: int) {
        self.x += dx;
        self.y += dy;
    }

    // no self: associated function
    fn origin() -> Point {
        return Point { x: 0, y: 0 };
    }
}

let p: Point = Point::origin();
p.distance(Point::origin());
```

**Style 3 — interface + impl** (trait-like):

```glide
interface Show {
    fn show(self: Point);
}

impl Show for Point {
    fn show(self: Point) {
        printf("(%d, %d)\n", self.x, self.y);
    }
}
```

Built-in primitives already come with stdlib methods:

```glide
let s: string = 42.to_string();
printf("len = %d\n", s.len());
let upper: string = "hello".concat(" world");
```

Chaining works: `c.is_alpha().to_string()`, `name.concat(" ok").len()`.

---

## 8. Arrays

`[1, 2, 3]` is an array literal. Type is `*T`. Glide doesn't track length, so you carry it yourself.

```glide
import "std/io";

fn main() -> int {
    let nums: *int = [10, 20, 30, 40, 50];
    println!(nums.to_string(5));   // [10, 20, 30, 40, 50]

    let sum: int = 0;
    for let i: int = 0; i < 5; i++ {
        sum += nums[i];
    }
    println!(sum.to_string());     // 150

    return 0;
}
```

---

## 9. println! and print! macros

Take any number of args of any type. The compiler picks the right format spec for each.

```glide
let name: string = "Maria";
let age: int = 30;

println!(name, age, 3.14, true);          // Maria 30 3.14 true
println!("age squared:", age * age);
```

`println!` adds `\n`. `print!` doesn't.

---

## 10. Concurrency

Glide has typed channels (Go-style) and a `spawn` keyword that runs a function in a thread.

```glide
fn worker(out: chan<int>) {
    out.send(42);
    out.close();
}

fn main() -> int {
    let c: chan<int> = make_chan(1);
    spawn worker(c);

    let v: int = c.recv();
    printf("got %d\n", v);

    return 0;
}
```

`make_chan(N)` makes a channel with capacity `N`. `send` / `recv` / `close` are the operations; can be called as methods (`c.send(x)`) or as free functions (`send(c, x)`). Channels block on full / empty. `close` wakes everyone.

`make_chan` only works as the initializer of a `let` or `const` with an explicit `chan<T>` annotation.

---

## 11. Imports and the stdlib

The compiler looks for a `std/` directory in any ancestor of your source file. `import "std/<name>";` loads `std/<name>.glide`.

You can also import your own files:

```glide
// helpers.glide
fn double(n: int) -> int {
    return n * 2;
}
```

```glide
// main.glide
import "helpers";

fn main() -> int {
    printf("%d\n", double(21));
    return 0;
}
```

In the LSP, methods from any file under `std/` show up automatically and offer to auto-import on accept.

---

## 12. Putting it together

A small command-line program:

```glide
import "std/io";
import "std/color";

struct Task {
    name: string,
    done: bool,
}

fn print_task(t: *Task) {
    let mark: string = "[ ]";
    if t.done {
        mark = "[x]".green();
    }
    println!(mark.concat(" ").concat(t.name));
}

fn main() -> int {
    let a: Task = Task { name: "write tutorial", done: true  };
    let b: Task = Task { name: "add generics",   done: false };
    let c: Task = Task { name: "build a real project", done: false };

    print_task(&a);
    print_task(&b);
    print_task(&c);

    return 0;
}
```

Save as `examples/todo.glide`, run with `glide run examples/todo.glide`.

---

## What to study next

Practical limits you'll hit, which point at what to add:

- **Building strings is awkward** because there's no interpolation and `printf` is the only formatter. You end up with `concat` chains plus `to_string`. A `format!` in stdlib is the obvious next quick win.
- **Arrays don't track length.** Wrapping `*T + len` in a struct gives you a real "list". Add `push`/`get`/`len` methods. Once that exists, real programs feel much better.
- **No generics yet**, so that list above is per element type (`IntList`, `StringList`, ...). Generics is the biggest language feature still missing. Adding it is a real project.
- **No Result/Option or pattern matching.** Today functions either return a value or crash. Pattern matching would help.
- **No closures**, so you can't pass behavior as an argument. Indirect calls would unlock callbacks and higher-order functions.

`LANGUAGE.md` has the full gap list. `DEVELOPING.md` shows how to add things to the compiler when you decide what to tackle.

The best way to actually learn the language is to write something small in Glide that you'd otherwise write in another language. A simple `wc`, an in-memory KV store, a password generator. You'll discover exactly what hurts and what's pleasant, and your priorities will sort themselves out.
