# Glide language reference

Targeted at someone who already knows another systems language (Rust, C, Go, Zig). For a learning-oriented walkthrough see `TUTORIAL.md`.

## lexical structure

### keywords

```
let const mut fn struct enum impl interface type extern pub move
if else while for return break continue match defer spawn import
true false null as
```

`chan` is a type constructor, not a keyword in expression position.

### operators

```
+  -  *  /  %               arithmetic
== != <  <= >  >=           comparison
&& || !                     logical
&  |  ^  ~  << >>           bitwise
=  += -= *= /=              assignment / compound
&  &mut                     borrow / mutable borrow
*                           dereference / pointer-type prefix / auto-drop suffix
->                          fn return type
=>                          match arm
::                          path
?                           result propagation (postfix)
```

### literals

```
42      0xFF    0b1010    1_000_000        integers
3.14    2.5e10                              floats
"text"  "esc\n"                             strings
'a'     '\n'    '\xFF'                      chars
true    false   null                        keywords
```

### comments

```glide
// line
/* block */
```

The lexer currently discards comments; the formatter doesn't preserve them.

## types

| Type            | Meaning                                                   |
| --------------- | --------------------------------------------------------- |
| `int` `uint`    | platform `int` (typically 32-bit) and unsigned variant     |
| `i8`–`i64`, `u8`–`u64` | fixed-width integers                                |
| `usize` `isize` | pointer-sized integers                                    |
| `f32` `f64` `float` | floating point                                       |
| `bool` `char` `string` | primitives. `string` is `const char*`             |
| `*T`            | pointer; non-owning unless declared with auto-drop         |
| `&T`            | shared borrow (non-null, no lifetime annotation)           |
| `&mut T`        | exclusive mutable borrow (non-null)                        |
| `[]T`           | slice: `{ data: *T, len: usize }`                         |
| `T<U, V>`       | generic instantiation                                      |
| `fn(A, B) -> R` | function pointer                                           |
| `!T`            | result: success carries `T`, error carries a `string`      |
| `chan<T>`       | typed channel                                              |

## statements

```glide
let name[: T] = expr;             // immutable
let mut name[: T] = expr;         // mutable
let name*[: T] = expr;            // owned + auto-drop at scope exit

const NAME[: T] = expr;           // file-scope or block-scope constant

return [expr];

expr;                             // expression statement (calls, assignments)

if cond { ... } else { ... }
while cond { ... }
for init; cond; step { ... }
{ ... }                           // block
match scrutinee { Variant(b) => { ... } _ => { ... } }

defer expr;                       // run at fn end / before return (LIFO)
spawn fn_call(args);              // run fn on a new thread

import "path/to/file.glide";
```

`break` and `continue` work in `while` and `for` loops.

## declarations

```glide
[pub] fn name[<T1, T2>](p1: T1, p2: T2) -> R { ... }
[pub] fn name(args) -> R;                                  // extern (no body)

[pub] struct Name[<T>] {
    field1: Type1,
    field2: Type2,
}

[pub] enum Name {
    Variant1,
    Variant2(int, string),
}

impl[<T>] Name[<T>] {
    fn method(self: *Name<T>, arg: int) -> int { ... }
}

extern fn libc_function(args) -> ret;
extern type OpaqueHandle;
extern type Tm = "struct tm";          // alias an existing C type
c_include "<stdio.h>";
c_link "m";                            // link against -lm
```

`pub` makes the symbol importable from other Glide files. Top-level visibility defaults to private.

## memory model

### stack values

Plain primitives and `let p: T = T { ... }` (no `*`) live on the stack and are copied on assignment / return-by-value.

### owned heap (`*T` with auto-drop)

Two equivalent patterns:

```glide
let p: *Point = Point { x: 1, y: 2 };   // implicit: type annotation + struct lit
let p* = Point { x: 1, y: 2 };          // explicit marker
let v* = Vector::new();                  // works for any pointer-returning expr
```

The compiler emits `free(p as *void)` (or `p.free()` if the type defines a `free` method) at the end of the enclosing block. Cleanup is scope-based: an auto-drop binding inside a `for` body fires once per iteration.

### raw heap (`*T`)

Pointers that aren't auto-drop are raw:

```glide
let p: *Point = some_call();    // up to you to free
let q: *Point = new Point { x: 1, y: 2 };   // explicit `new`, raw
```

You're responsible for matching `free(p as *void)` or letting them leak intentionally (e.g. the program owns them until exit).

### arenas

```glide
let arena: *Arena = Arena::new(4096);
defer arena.free();

let p: *Point = arena.create(Point);            // sugar for arena.alloc(sizeof(Point)) as *Point
let buf: *int = arena.create(int, 100);         // sugar for an array
let raw: *void = arena.alloc(bytes);            // raw escape hatch
```

Use arenas when you have a bag of allocations sharing a lifetime (parser nodes, request-scoped data). Free is `O(1)` regardless of count.

### borrows

`&T` and `&mut T` are non-null views with function-scoped lifetimes. The borrow checker enforces:

| Code                    | Description                                                    |
| ----------------------- | -------------------------------------------------------------- |
| `borrow-in-field`       | `&T` / `&mut T` can't appear in a struct field (use `*T`)      |
| `borrow-alias-in-call`  | `f(&x, &mut x)` and `f(&mut x, &mut x)` are rejected           |
| `dangling-return`       | `return &local` is rejected; pass-through of borrow params OK   |
| `owned-escape`          | `return owned_p` is rejected (auto-drop would free it)         |
| `owned-move`            | `let q = owned_p` is rejected (double-free risk)               |
| `owned-into-ptr-param`  | `f(owned_p)` where `f` takes `*T` by value is rejected         |
| `overlap-borrow`        | conflicting `&` / `&mut` in the same scope is rejected          |

You never write lifetime annotations.

## errors as values

```glide
fn parse(s: string) -> !int {
    if s.eq("") { return err("empty"); }
    return ok(42);
}

fn pipeline(s: string) -> !int {
    let n: int = parse(s)?;     // propagate err if any
    return ok(n + 1);
}
```

`?` only works inside a function whose return type is `!U` for some `U`. Result conversion happens at the propagation site: an `!T` with err `e` becomes `!U` with err `e`.

`unwrap(r)` returns the inner value or a zero-initialized fallback if `r` is err. Use it at boundaries where you've already checked the error.

## concurrency

```glide
fn worker(c: chan<int>) {
    send(c, 42);
    close(c);
}

fn main() -> int {
    let c: chan<int> = make_chan(4);     // buffered, capacity 4
    spawn worker(c);

    let v: int = recv(c);                // blocks until value or closed
    close(c);                            // sender already closed; idempotent
    return v;
}
```

Channels are bounded, blocking, MPMC. `spawn` takes a direct function call and starts a new thread. Each spawned function runs to completion (no early termination from outside).

`make_chan(cap)` requires `cap >= 1`. Sending on a closed channel is a no-op. Receiving from a closed empty channel returns a zero-initialized `T`.

## generics

Function and struct type parameters use angle brackets; instantiation is monomorphized.

```glide
struct Vec<T> { data: *T, len: int, cap: int }

impl<T> Vec<T> {
    fn new() -> *Vec<T> { ... }
    fn push(self: *Vec<T>, x: T) { ... }
}

fn first<T>(v: *Vec<T>) -> T { return v.data[0]; }
```

Inference fires from:

- An explicit return-type hint at the call site (`let v: *Vec<int> = Vec::new()`)
- Argument types passed in (`first(v)` infers `T` from `v`)
- Method calls on a not-yet-bound `let v = Generic::new()` — the first call that uses a type parameter resolves it

There's no turbofish syntax; if inference fails you must annotate the let.

## interfaces

```glide
interface Drawable {
    fn draw(self: *T);
}

impl Drawable for Circle {
    fn draw(self: *Circle) { ... }
}

fn render(items: *Drawable) { ... }    // dynamic dispatch
```

Used sparingly; most polymorphism in Glide goes through generics.

## FFI

```glide
extern fn printf(fmt: string, ...) -> int;
extern fn time(t: *long) -> long;
extern type FILE;
extern type Tm = "struct tm";
c_include "<time.h>";
c_link "m";

fn now() -> long {
    let t: *long = null;
    return time(t);
}
```

Variadic functions accept `...` as the last "parameter". `extern type Foo;` declares an opaque struct; `extern type Foo = "..."` aliases an existing C type. `c_include` injects an `#include` into the generated C; `c_link` adds `-l<name>` to the linker.

## memory layout and ABI

Glide compiles to C99 + pthread. Structs follow the C ABI: same layout as the equivalent C struct. Generic struct instantiations are mangled with their type arguments (e.g. `Vector<int>` becomes `Vector__int`). Function pointers fit in a `void*` slot and cast at call sites.

## what's intentionally not in the language

- **Lifetimes / generic over lifetimes** — borrows are function-scoped only
- **Async / await** — `chan` + `spawn` cover concurrency; no executor
- **Trait bounds on generics** — errors surface at monomorphization
- **Macros beyond `println!` / `print!` / `format!`** — no user-defined macros
- **Reflection** — none
- **Garbage collection** — explicitly excluded
- **Nullable type (`?T`)** — pointers are nullable; borrows are non-null. `?T` is on the roadmap.
