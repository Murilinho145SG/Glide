# Glide tutorial

Goes from zero to real programs. Each section's code runs as-is with `glide run <file>.glide`.

For the full list of features and what's missing, see `LANGUAGE.md`. For working on the compiler itself, see `DEVELOPING.md`.

---

## 1. Hello world

```glide
fn main() -> int {
    printf("hello\n");
    return 0;
}
```

Every program needs `fn main`. Returns int (process exit code). `printf` is the libc one, format strings work the same way.

---

## 2. Variables

```glide
fn main() -> int {
    let n: int = 42;
    let pi: float = 3.14;
    let name: string = "alelo";
    let ok: bool = true;
    let c: char = 'a';

    let inferred = 100;        // type comes from value (int here)
    const MAX: int = 1_000_000; // const is immutable

    printf("%d %f %s\n", n, pi, name);
    return 0;
}
```

`let` is mutable, `const` isn't. Underscores in numbers are just visual.

---

## 3. Functions

```glide
fn add(a: int, b: int) -> int {
    return a + b;
}

fn greet(name: string) {
    printf("hi, %s\n", name);
}

fn main() -> int {
    let s: int = add(3, 4);
    printf("%d\n", s);
    greet("world");
    return 0;
}
```

`-> Type` for the return type. Omit it if the function returns nothing.

---

## 4. If / while / for

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

No parentheses around conditions. Body always braced.

---

## 5. Pointers and memory

Glide doesn't have a garbage collector. You allocate with `malloc` and free with `free`.

```glide
fn use_pointer(p: *int) {
    *p += 100;          // dereference and assign
}

fn main() -> int {
    let x: int = 5;
    use_pointer(&x);    // pass address of x
    printf("%d\n", x);  // 105

    let p: *int = malloc(sizeof(int)) as *int;
    *p = 42;
    printf("%d\n", *p);
    free(p as *void);

    return 0;
}
```

`*T` is "pointer to T". `&x` takes the address. `*p` reads/writes through it.
`sizeof` gives byte size. `as` is a cast.

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
    printf("(%d, %d)\n", h->x, h->y);
    free(h as *void);

    return 0;
}
```

`new T { ... }` is `malloc + struct init` in one go. Returns `*T`.

`a.x` works on values; `a->x` works on pointers. Glide also auto-derefs `.` on pointers, so `h.x` works too.

---

## 7. Methods (UFCS)

Any free function whose first parameter is `T` can be called as a method on a `T`:

```glide
struct Point {
    x: int,
    y: int,
}

fn Point_distance_sq(a: Point, b: Point) -> int {
    let dx: int = a.x - b.x;
    let dy: int = a.y - b.y;
    return dx * dx + dy * dy;
}

fn main() -> int {
    let p: Point = Point { x: 0, y: 0 };
    let q: Point = Point { x: 3, y: 4 };
    printf("%d\n", p.distance_sq(q));   // 25
    return 0;
}
```

The naming convention `<TypeName>_<method>` is what enables `p.distance_sq(...)`. Same for primitives:

```glide
let s: string = 42.to_string();         // calls int_to_string
printf("len = %d\n", s.len());          // calls string_len
let upper: string = "hi".concat(" world");  // calls string_concat
```

You can chain: `c.is_alpha().to_string()`, `name.concat(" :)").len()`.

---

## 8. Interfaces

Interfaces are like Rust traits. They declare methods. Then you `impl` for any type, including native ones.

```glide
interface Greet {
    fn shout(self: string) -> string;
}

impl Greet for string {
    fn shout(self: string) -> string {
        return self.concat("!!!");
    }
}

fn main() -> int {
    printf("%s\n", "hello".shout());   // hello!!!
    return 0;
}
```

`impl Color for string` in `std/color.glide` is what gives you `"x".blue()`, `"y".red()`, etc.

---

## 9. Arrays

`[1, 2, 3]` is an array literal. Type is `*T`. Glide doesn't track length, so you carry it yourself.

```glide
import "std/io";

fn main() -> int {
    let nums: *int = [10, 20, 30, 40, 50];
    println(nums.to_string(5));   // [10, 20, 30, 40, 50]

    let sum: int = 0;
    for let i: int = 0; i < 5; i++ {
        sum += nums[i];
    }
    println(sum.to_string());     // 150

    return 0;
}
```

`std/io` has print/println and array-to-string helpers.

---

## 10. Concurrency

Glide has Go-style channels and a `spawn` keyword that runs a function in a thread. Channels are typed and bounded.

```glide
fn worker(out: chan<int>) {
    send(out, 42);
    close(out);
}

fn main() -> int {
    let c: chan<int> = make_chan(1);
    spawn worker(c);

    let v: int = recv(c);
    printf("got %d\n", v);

    return 0;
}
```

`make_chan(N)` makes a channel with capacity `N`. `send`/`recv`/`close` are the operations. Channels block when full / empty. `close` wakes everyone.

`make_chan` only works directly inside `let c: chan<T> = make_chan(...)` (with the explicit type annotation).

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

For the LSP completion, methods from any file under `std/` show up automatically and offer to auto-import on accept.

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
    if t->done {
        mark = "[x]".green();
    }
    println(mark.concat(" ").concat(t->name));
}

fn main() -> int {
    let a: Task = Task { name: "write tutorial",      done: true };
    let b: Task = Task { name: "add generics",        done: false };
    let c: Task = Task { name: "build a real project", done: false };

    print_task(&a);
    print_task(&b);
    print_task(&c);

    return 0;
}
```

Save this as `examples/todo.glide` and run with `glide run examples/todo.glide`.

---

## What to learn next

The compiler-side things you'll bump into when growing your own programs:

- **String building is awkward** because there's no string interpolation and no formatter beyond `printf`. You'll lean on `concat` chains and `to_string`. A real `format!`-equivalent in stdlib is the obvious next stdlib win.
- **Arrays don't track length.** Wrapping `*T + len` in a struct gives you a real "list". Add `push`/`get`/`len` methods on it. Once you have that, real programs feel much better.
- **No generics yet**, so the list above is per-element-type (`IntList`, `StringList`, ...). Generics is the biggest language feature you don't have. Adding it is a real project.
- **No errors / Result type.** Today fns either return a value or you crash. Pattern match would help.
- **No closures**, so you can't pass a function to a fn that takes a behavior. Indirect calls would unlock callbacks and higher-order stuff.

`LANGUAGE.md` has the full list of what's missing. `DEVELOPING.md` shows how to add new things to the compiler itself when you decide what to tackle.

Best way to actually learn the language: write something small in it that you'd otherwise write in Go or Rust. A wc clone, a tiny key-value store, a maze generator. You'll discover exactly what hurts and what's pleasant, and your priorities will sort themselves out.
