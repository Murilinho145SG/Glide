# Glide language reference

Snapshot of what the language supports today.

## Types

| Form | Example | Notes |
| --- | --- | --- |
| `int`     | `42`         | C `int` |
| `float`   | `3.14`       | C `double` |
| `bool`    | `true`       | C `bool` (`stdbool.h`) |
| `char`    | `'a'`        | C `char` |
| `string`  | `"hi"`       | C `const char*` |
| `void`    |              | only as fn return / `*void` |
| `*T`      | `*int`       | owned heap pointer |
| `&T`      | `&int`       | shared (immutable) borrow |
| `&mut T`  | `&mut int`   | exclusive mutable borrow |
| `chan<T>` | `chan<int>`  | channel; created with `make_chan(N)` |
| `fn(...) -> T` | `fn(int, int) -> int` | function pointer |
| `<Struct>`| `Point`      | user-defined |

`null` is the typed null pointer; assignable to any `*T`.

### Memory model

| Form | Allocates? | Auto-frees? | Can be moved/returned? |
| --- | --- | --- | --- |
| `T` (value) | stack | scope-local | yes (copy or move) |
| `*T` from `T { ... }` | heap (auto) | yes (auto-drop) | **no** in v1 |
| `*T` from `arena_alloc()` etc | allocator-managed | no | yes (raw) |
| `&T` / `&mut T` | no | no (view) | only pass-through of param |

The pattern `let p: *T = T { ... };` triggers heap allocation and auto-drop at scope end. Other `*T` (from arena, function returns, casts) are raw pointers without auto-drop.

### Borrow rules (v1)

- `&T` and `&mut T` exist only as parameters or local variables; **not allowed in struct fields**.
- Cannot return a borrow of a local variable (escape rule).
- Within a single call, cannot borrow the same variable as both `&` and `&mut`, or as `&mut` twice.
- `*T` decays to `&T` or `&mut T` automatically at call sites.

### Type aliases

```glide
type UserId int;
type Callback fn(int);
type Handler fn(*Request) -> *Response;
pub type ServerHandler fn(*Request) -> *Response;
```

## Literals

```glide
123                 // int
0x1F  0b1010  0o17  // hex / bin / oct
1_000_000           // underscores allowed
3.14   1.5e10       // float
"hello\n"           // string with escapes
'a'                 // char
true   false        // bool
null                // null pointer

[1, 2, 3]           // array literal -> *T (C compound literal)
Point { x: 1, y: 2 }    // struct literal
new Point { x: 1, y: 2 }   // heap struct -> *Point
```

String / char escapes: `\n \t \r \\ \" \' \0 \xNN`.

## Declarations

```glide
let x: int = 5;                 // typed, immutable
let mut y: int = 5;             // mutable
let x = 5;                      // inferred from value
let x: int;                     // uninitialized (type required)
const PI: float = 3.14;

fn name(a: int, b: int) -> int {
    return a + b;
}

fn no_return(p: *int) {
    *p = 0;
}

struct Point {
    x: int,
    y: int,
}

interface Greet {
    fn hi(self: string) -> string;
}

impl Greet for string {
    fn hi(self: string) -> string {
        return "hello, ".concat(self);
    }
}

import "std/color";
```

## Statements

```glide
expr;                           // expression statement
{ ... }                         // block

if cond { ... }
if cond { ... } else { ... }
if cond { ... } else if cond { ... } else { ... }

while cond { ... }

for let i: int = 0; i < n; i++ {
    ...
}

break;
continue;

return;
return value;

spawn worker(arg1, arg2);
```

`if`/`while`/`for` conditions don't take parentheses; the body is always braced.

## Expressions

| Category | Operators |
| --- | --- |
| Arithmetic | `+ - * / %` |
| Comparison | `== != < <= > >=` |
| Logical    | `&& \|\| !` |
| Bitwise    | `& \| ^ ~ << >>` |
| Assignment | `= += -= *= /= %= &= \|= ^= <<= >>=` |
| Postfix    | `++ --` |
| Unary      | `- ! ~ * & &mut` |
| Member     | `obj.field` (auto-deref on `*Struct`) |
| Index      | `arr[i]` |
| Call       | `f(args)` |
| Cast       | `expr as Type` |
| Sizeof     | `sizeof(Type)` |

## Method calls (UFCS)

`obj.method(args)` rewrites to a free function call. Lookup order:

1. `__glide_<typemangle>_<method>(obj, args)` (compiler builtin / stdlib)
2. `<typemangle>_<method>(obj, args)` (user-defined or `impl` method)
3. Stripped pointer mangle: `<basename>_<method>(obj, args)`
4. Plain `<method>(obj, args)` if first param type matches

`Type::mangle()`:
- `int`, `string`, ... → same name
- `*Point` → `Point_ptr`
- `chan<int>` → `chan_int`

So `fn int_to_string(n: int) -> string` makes `1.to_string()` work. `fn Point_ptr_translate(p: *Point, ...)` makes `(&p).translate(...)` work.

## Builtins (always available)

```
printf(fmt: string, ...) -> int
scanf(fmt: string, ...) -> int
puts(s: string) -> int
malloc(size: int) -> *void
calloc(n: int, size: int) -> *void
free(p: *void)
strlen(s: string) -> int
strcmp(a: string, b: string) -> int
```

All variadic / unchecked at the boundary (matches libc).

## Concurrency

```glide
let c: chan<int> = make_chan(4);    // capacity 4 ring buffer

spawn worker(c);                    // pthread_create + detach

send(c, 42);
let v: int = recv(c);
close(c);
```

Channels are bounded, blocking on full/empty, broadcast on close. Compiled with `-lpthread` automatically.

`make_chan(N)` only valid as the initializer of a `let` / `const` with an explicit `chan<T>` annotation.

## Primitive methods (stdlib)

Available everywhere, no import needed:

```
int   .to_string()    -> string
int   .to_float()     -> float
int   .abs()          -> int

float .to_string()    -> string
float .to_int()        -> int
float .floor()        -> float
float .ceil()         -> float

string.len()          -> int
string.eq(b: string)  -> bool
string.at(i: int)     -> char
string.concat(b)      -> string

bool  .to_string()    -> string

char  .to_int()       -> int
char  .is_digit()     -> bool
char  .is_alpha()     -> bool
```

## Stdlib modules (in Glide)

### `std/io`

```
print(s: string)
println(s: string)

int_ptr_to_string(arr: *int, n: int) -> string
string_ptr_to_string(arr: *string, n: int) -> string
float_ptr_to_string(arr: *float, n: int) -> string
```

UFCS exposes the array printers as `arr.to_string(n)`.

### `std/color`

```
interface Color {
    black / red / green / yellow / blue / magenta / cyan / white
    bold / dim / underline
}
impl Color for string { ... }   // ANSI escapes

wrap_ansi(code: string, s: string) -> string
```

So `"hi".blue().bold()` produces a colored string.

## Toolchain

```
glide build <file> [-o name]    # compile to native exe (gcc)
glide run <file>                # build to tempfile and run
glide emit <file>                # print generated C
glide fmt <file> [--write]      # pretty-print
glide lsp                       # language server on stdio
```

Imports are resolved recursively from the file being compiled. The driver looks for `std/` in any ancestor of the source file, so `import "std/color";` works from anywhere in the project.

## Editor support

Zed extension at `zed-extension/`:
- syntax highlighting via tree-sitter (`glide-grammar/`)
- LSP launcher
- LSP features: diagnostics, hover, goto-definition, completion (with auto-import for stdlib methods), document formatting

Format-on-save settings example:

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

## What's not (yet) supported

For honesty:

- No generics
- No `[T; N]` fixed-array type syntax (use `*T` + length)
- No length tracking on arrays (you pass `n` everywhere)
- No `match`
- No closures / function pointers / indirect calls
- No `import` aliasing (`import "std/io" as io`)
- No multi-file user modules outside `std/` (works but no namespacing)
- No GC, no ownership/borrow rules: you call `malloc`/`free` yourself
- `as` cast is unchecked
- No macros
- No string interpolation
