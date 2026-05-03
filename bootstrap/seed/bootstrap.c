#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// ---- glide runtime ----
static int __glide_string_len(const char* s) { return (int)strlen(s); }
static bool __glide_string_eq(const char* a, const char* b) { return strcmp(a, b) == 0; }
static char __glide_string_at(const char* s, int i) { return s[i]; }
static const char* __glide_string_concat(const char* a, const char* b) {
    size_t la = strlen(a), lb = strlen(b);
    char* out = (char*)malloc(la + lb + 1);
    memcpy(out, a, la); memcpy(out + la, b, lb); out[la + lb] = 0;
    return out;
}
static const char* __glide_string_substring(const char* s, int start, int end) {
    int n = (int)strlen(s);
    if (start < 0) start = 0;
    if (end > n) end = n;
    if (start > end) start = end;
    int len = end - start;
    char* out = (char*)malloc((size_t)len + 1);
    memcpy(out, s + start, (size_t)len); out[len] = 0;
    return out;
}
static int __glide_char_to_int(char c) { return (int)(unsigned char)c; }
static bool __glide_char_is_digit(char c) { return c >= '0' && c <= '9'; }
static bool __glide_char_is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
static int __glide_int_abs(int n) { return n < 0 ? -n : n; }
static const char* __glide_int_to_string(int n) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d", n);
    char* out = (char*)malloc((size_t)len + 1);
    memcpy(out, buf, (size_t)len + 1);
    return out;
}
static const char* __glide_bool_to_string(bool b) { return b ? "true" : "false"; }
#include <stdarg.h>
static const char* __glide_format(const char* fmt, ...) {
    va_list ap1, ap2;
    va_start(ap1, fmt);
    va_copy(ap2, ap1);
    int n = vsnprintf(NULL, 0, fmt, ap1);
    va_end(ap1);
    char* out = (char*)malloc((size_t)n + 1);
    vsnprintf(out, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return out;
}
static int __glide_argc_g = 0;
static char** __glide_argv_g = NULL;
static void __glide_args_init(int argc, char** argv) { __glide_argc_g = argc; __glide_argv_g = argv; }
static int args_count(void) { return __glide_argc_g; }
static const char* args_at(int i) {
    if (i < 0 || i >= __glide_argc_g) return "";
    return __glide_argv_g[i];
}
static const char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, f); fclose(f); buf[got] = 0;
    return buf;
}
static bool write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "wb"); if (!f) return false;
    size_t n = strlen(content);
    size_t wrote = fwrite(content, 1, n, f); fclose(f);
    return wrote == n;
}
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define __glide_dup _dup
#define __glide_dup2 _dup2
#define __glide_close _close
#define __glide_fileno _fileno
#else
#include <unistd.h>
#define __glide_dup dup
#define __glide_dup2 dup2
#define __glide_close close
#define __glide_fileno fileno
#endif
static int __glide_redirect_to(const char* path) {
    fflush(stdout);
    int saved = __glide_dup(1);
    if (saved < 0) return -1;
    FILE* f = fopen(path, "w");
    if (!f) { __glide_close(saved); return -1; }
    __glide_dup2(__glide_fileno(f), 1);
    fclose(f);
    return saved;
}
static void __glide_restore_stdout(int saved) {
    fflush(stdout);
    if (saved >= 0) { __glide_dup2(saved, 1); __glide_close(saved); }
}
static int __glide_shell(const char* cmd) { return system(cmd); }
static const char* __glide_getenv(const char* name) { const char* v = getenv(name); return v ? v : ""; }
static bool __glide_file_exists(const char* path) {
    FILE* f = fopen(path, "rb"); if (!f) return false; fclose(f); return true;
}
#ifdef _WIN32
#include <fcntl.h>
static void __glide_set_binary_io(void) {
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
}
#else
static void __glide_set_binary_io(void) {}
#endif
static const char* __glide_read_line(void) {
    static char buf[8192];
    if (!fgets(buf, sizeof(buf), stdin)) { buf[0] = 0; return buf; }
    return buf;
}
static const char* __glide_read_bytes(int n) {
    if (n <= 0) return "";
    char* buf = (char*)malloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, stdin);
    buf[got] = 0;
    return buf;
}
static void __glide_write_str(const char* s) {
    fputs(s, stdout);
}
static void __glide_write_bytes(const char* s, int n) {
    if (n > 0) fwrite(s, 1, (size_t)n, stdout);
}
static void __glide_flush_stdout(void) { fflush(stdout); }
static void __glide_log(const char* s) {
    fputs(s, stderr); fputc('\n', stderr); fflush(stderr);
}
static int __glide_is_windows(void) {
#ifdef _WIN32
    return 1;
#else
    return 0;
#endif
}
#ifdef _WIN32
static const char* __glide_exe_path(void) {
    static char buf[1024];
    DWORD n = GetModuleFileNameA(NULL, buf, sizeof(buf));
    if (n == 0 || n >= sizeof(buf)) buf[0] = 0;
    return buf;
}
static const char* __glide_exe_dir(void) {
    static char buf[1024];
    DWORD n = GetModuleFileNameA(NULL, buf, sizeof(buf));
    if (n == 0 || n >= sizeof(buf)) { buf[0] = 0; return buf; }
    for (DWORD i = n; i > 0; i--) { if (buf[i-1] == '\\' || buf[i-1] == '/') { buf[i-1] = 0; return buf; } }
    return "";
}
#else
static const char* __glide_exe_path(void) {
    static char buf[1024];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) { buf[0] = 0; return buf; }
    buf[n] = 0;
    return buf;
}
static const char* __glide_exe_dir(void) {
    static char buf[1024];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) { buf[0] = 0; return buf; }
    buf[n] = 0;
    for (ssize_t i = n; i > 0; i--) { if (buf[i-1] == '/') { buf[i-1] = 0; return buf; } }
    return "";
}
#endif
typedef struct Arena { unsigned char* head; int cap; int used; } Arena;
static Arena* Arena_new(int cap) {
    Arena* a = (Arena*)malloc(sizeof(Arena));
    a->head = (unsigned char*)malloc((size_t)cap);
    a->cap = cap; a->used = 0;
    return a;
}
static void* Arena_alloc(Arena* a, int size) {
    int aligned = (size + 7) & ~7;
    if (a->used + aligned > a->cap) { fprintf(stderr, "arena oom\n"); exit(1); }
    void* p = (void*)(a->head + a->used);
    a->used += aligned;
    return p;
}
static void Arena_free(Arena* a) { free(a->head); free(a); }
static int Arena_used(Arena* a) { return a->used; }
static void Arena_reset(Arena* a) { a->used = 0; }

typedef struct  Vector__Type   Vector__Type ;
typedef struct  Vector__Expr   Vector__Expr ;
typedef struct  Vector__string   Vector__string ;
typedef struct  Vector__Param   Vector__Param ;
typedef struct  Vector__Stmt   Vector__Stmt ;
typedef struct  Vector__Field   Vector__Field ;
typedef struct  Vector__EnumVariant   Vector__EnumVariant ;
typedef struct  Vector__MatchArm   Vector__MatchArm ;
typedef struct  HashMap__FnSig   HashMap__FnSig ;
typedef struct  HashMap__bool   HashMap__bool ;
typedef struct  HashMap__Type   HashMap__Type ;
typedef struct  Vector__BorrowEvent   Vector__BorrowEvent ;
typedef struct  Vector__DiagEntry   Vector__DiagEntry ;
typedef struct  Vector__bool   Vector__bool ;
typedef struct  HashMap__Stmt   HashMap__Stmt ;
typedef struct  Vector__FnMonoEntry   Vector__FnMonoEntry ;
typedef struct  Vector__AnonFn   Vector__AnonFn ;
typedef struct  Vector__JsonValue   Vector__JsonValue ;
typedef struct  HashMap__LspDoc   HashMap__LspDoc ;
typedef struct  Vector__UseSite   Vector__UseSite ;
typedef struct  Token   Token ;
typedef struct  Lexer   Lexer ;
typedef struct  Type   Type ;
typedef struct  Expr   Expr ;
typedef struct  Param   Param ;
typedef struct  Field   Field ;
typedef struct  EnumVariant   EnumVariant ;
typedef struct  MatchArm   MatchArm ;
typedef struct  Stmt   Stmt ;
typedef struct  StructLitField   StructLitField ;
typedef struct  Parser   Parser ;
typedef struct  FnSig   FnSig ;
typedef struct  BorrowEvent   BorrowEvent ;
typedef struct  DiagEntry   DiagEntry ;
typedef struct  Typer   Typer ;
typedef struct  FnMonoEntry   FnMonoEntry ;
typedef struct  AnonFn   AnonFn ;
typedef struct  CG   CG ;
typedef struct  JsonValue   JsonValue ;
typedef struct  JsonParser   JsonParser ;
typedef struct  LspDoc   LspDoc ;
typedef struct  LspState   LspState ;
typedef struct  UseSite   UseSite ;

struct  Vector__Type  {
     Type*   data;
     int   len;
     int   cap;
};

struct  Vector__Expr  {
     Expr*   data;
     int   len;
     int   cap;
};

struct  Vector__string  {
     const char**   data;
     int   len;
     int   cap;
};

struct  Vector__Param  {
     Param*   data;
     int   len;
     int   cap;
};

struct  Vector__Stmt  {
     Stmt*   data;
     int   len;
     int   cap;
};

struct  Vector__Field  {
     Field*   data;
     int   len;
     int   cap;
};

struct  Vector__EnumVariant  {
     EnumVariant*   data;
     int   len;
     int   cap;
};

struct  Vector__MatchArm  {
     MatchArm*   data;
     int   len;
     int   cap;
};

struct  HashMap__FnSig  {
     const char**   keys;
     FnSig*   values;
     bool*   occupied;
     int   len;
     int   cap;
};

struct  HashMap__bool  {
     const char**   keys;
     bool*   values;
     bool*   occupied;
     int   len;
     int   cap;
};

struct  HashMap__Type  {
     const char**   keys;
     Type*   values;
     bool*   occupied;
     int   len;
     int   cap;
};

struct  Vector__BorrowEvent  {
     BorrowEvent*   data;
     int   len;
     int   cap;
};

struct  Vector__DiagEntry  {
     DiagEntry*   data;
     int   len;
     int   cap;
};

struct  Vector__bool  {
     bool*   data;
     int   len;
     int   cap;
};

struct  HashMap__Stmt  {
     const char**   keys;
     Stmt*   values;
     bool*   occupied;
     int   len;
     int   cap;
};

struct  Vector__FnMonoEntry  {
     FnMonoEntry*   data;
     int   len;
     int   cap;
};

struct  Vector__AnonFn  {
     AnonFn*   data;
     int   len;
     int   cap;
};

struct  Vector__JsonValue  {
     JsonValue*   data;
     int   len;
     int   cap;
};

struct  HashMap__LspDoc  {
     const char**   keys;
     LspDoc*   values;
     bool*   occupied;
     int   len;
     int   cap;
};

struct  Vector__UseSite  {
     UseSite*   data;
     int   len;
     int   cap;
};

typedef struct  Token  {
     int   kind;
     const char*   lexeme;
     int   line;
     int   column;
}  Token ;

typedef struct  Lexer  {
     const char*   src;
     int   src_len;
     int   pos;
     int   line;
     int   column;
}  Lexer ;

typedef struct  Type  {
     int   kind;
     const char*   name;
     Type*   inner;
     Vector__Type*   args;
     Type*   fnptr_ret;
}  Type ;

typedef struct  Expr  {
     int   kind;
     int   line;
     int   column;
     int   int_val;
     const char*   str_val;
     bool   bool_val;
     int   op_code;
     Expr*   lhs;
     Expr*   rhs;
     Expr*   operand;
     Vector__Expr*   args;
     const char*   field;
     Type*   cast_to;
     Vector__string*   field_names;
     Vector__Param*   fn_expr_params;
     Vector__Stmt*   fn_expr_body;
}  Expr ;

typedef struct  Param  {
     const char*   name;
     Type*   ty;
}  Param ;

typedef struct  Field  {
     const char*   name;
     Type*   ty;
}  Field ;

typedef struct  EnumVariant  {
     const char*   name;
     Vector__Type*   fields;
}  EnumVariant ;

typedef struct  MatchArm  {
     const char*   variant;
     Vector__string*   bindings;
     Vector__Stmt*   body;
}  MatchArm ;

typedef struct  Stmt  {
     int   kind;
     int   line;
     int   column;
     bool   is_pub;
     const char*   origin;
     const char*   name;
     Type*   let_ty;
     Expr*   let_value;
     bool   is_mut;
     bool   is_auto_owned;
     Expr*   expr_value;
     Expr*   cond;
     Vector__Stmt*   then_body;
     Vector__Stmt*   else_body;
     Stmt*   for_init;
     Expr*   for_step;
     Vector__Param*   fn_params;
     Type*   fn_ret_ty;
     Vector__Stmt*   fn_body;
     Vector__Field*   struct_fields;
     Type*   impl_target;
     Vector__Stmt*   impl_methods;
     Vector__string*   type_params;
     const char*   import_path;
     Vector__EnumVariant*   enum_variants;
     Expr*   scrutinee;
     Vector__MatchArm*   arms;
}  Stmt ;

typedef struct  StructLitField  {
     const char*   name;
     Expr*   value;
}  StructLitField ;

typedef struct  Parser  {
     Lexer*   lex;
     Token   current;
     Token   peek;
     int   error_count;
     bool   no_struct_lit;
}  Parser ;

typedef struct  FnSig  {
     const char*   name;
     Vector__Param*   params;
     Type*   ret_type;
}  FnSig ;

typedef struct  BorrowEvent  {
     const char*   source;
     bool   is_mut;
}  BorrowEvent ;

typedef struct  DiagEntry  {
     int   line;
     int   col;
     int   end_line;
     int   end_col;
     int   severity;
     const char*   code;
     const char*   message;
     const char*   origin;
}  DiagEntry ;

typedef struct  Typer  {
     HashMap__FnSig*   fns;
     HashMap__bool*   structs;
     HashMap__Type*   scope;
     HashMap__Type*   module_scope;
     HashMap__bool*   owned_locals;
     Vector__BorrowEvent*   borrows;
     Type*   current_ret;
     const char*   current_origin;
     int   error_count;
     Vector__DiagEntry*   diagnostics;
}  Typer ;

typedef struct  FnMonoEntry  {
     const char*   name;
     Vector__Type*   args;
}  FnMonoEntry ;

typedef struct  AnonFn  {
     const char*   name;
     Vector__Param*   params;
     Type*   ret_ty;
     Vector__Stmt*   body;
}  AnonFn ;

typedef struct  CG  {
     HashMap__Type*   scope;
     HashMap__Stmt*   generic_structs;
     HashMap__Stmt*   generic_fns;
     HashMap__Stmt*   structs;
     HashMap__Stmt*   enums;
     HashMap__Stmt*   fns;
     HashMap__bool*   emitted_monos;
     Vector__FnMonoEntry*   fn_mono_queue;
     Vector__Expr*   defer_stack;
     Vector__Expr*   auto_drop_stack;
     Vector__AnonFn*   anon_fns;
     int   anon_count;
     Vector__Type*   chan_types;
     int   spawn_count;
     bool   uses_pthread;
     Vector__Type*   result_types;
     Type*   current_ret_ty;
}  CG ;

typedef struct  JsonValue  {
     int   kind;
     bool   b;
     int   i;
     const char*   s;
     Vector__JsonValue*   arr;
     Vector__string*   obj_keys;
     Vector__JsonValue*   obj_vals;
}  JsonValue ;

typedef struct  JsonParser  {
     const char*   src;
     int   pos;
     int   n;
}  JsonParser ;

typedef struct  LspDoc  {
     const char*   uri;
     const char*   text;
     int   version;
     Vector__Stmt*   stmts;
}  LspDoc ;

typedef struct  LspState  {
     HashMap__LspDoc*   docs;
     bool   shutdown_requested;
}  LspState ;

typedef struct  UseSite  {
     int   line;
     int   col;
}  UseSite ;

#define  TOK_EOF  0
#define  TOK_IDENT  1
#define  TOK_INT  2
#define  TOK_FLOAT  3
#define  TOK_STRING  4
#define  TOK_CHAR  5
#define  TOK_KEYWORD  6
#define  TOK_OP  7
#define  TOK_ERROR  8
#define  TY_NAMED  0
#define  TY_POINTER  1
#define  TY_BORROW  2
#define  TY_BORROW_MUT  3
#define  TY_SLICE  4
#define  TY_GENERIC  5
#define  TY_FNPTR  6
#define  TY_RESULT  7
#define  EX_INT  0
#define  EX_FLOAT  1
#define  EX_STRING  2
#define  EX_BOOL  3
#define  EX_NULL  4
#define  EX_IDENT  5
#define  EX_BINARY  6
#define  EX_UNARY  7
#define  EX_CALL  8
#define  EX_ASSIGN  9
#define  EX_MEMBER  10
#define  EX_INDEX  11
#define  EX_CAST  12
#define  EX_CHAR  13
#define  EX_MACRO  14
#define  EX_PATH  15
#define  EX_POSTINC  16
#define  EX_POSTDEC  17
#define  EX_STRUCT_LIT  18
#define  EX_NEW  19
#define  EX_SIZEOF  20
#define  EX_FNEXPR  21
#define  OP_ADD  1
#define  OP_SUB  2
#define  OP_MUL  3
#define  OP_DIV  4
#define  OP_MOD  5
#define  OP_EQ  6
#define  OP_NE  7
#define  OP_LT  8
#define  OP_LE  9
#define  OP_GT  10
#define  OP_GE  11
#define  OP_AND  12
#define  OP_OR  13
#define  OP_ASSIGN  14
#define  OP_PLUS_ASSIGN  15
#define  OP_MINUS_ASSIGN  16
#define  OP_STAR_ASSIGN  17
#define  OP_SLASH_ASSIGN  18
#define  OP_BIT_AND  19
#define  OP_BIT_OR  20
#define  OP_BIT_XOR  21
#define  OP_SHL  22
#define  OP_SHR  23
#define  UN_NEG  1
#define  UN_NOT  2
#define  UN_DEREF  3
#define  UN_ADDR  4
#define  UN_ADDR_MUT  5
#define  UN_BIT_NOT  6
#define  UN_TRY  7
#define  ST_LET  0
#define  ST_RETURN  1
#define  ST_EXPR  2
#define  ST_IF  3
#define  ST_WHILE  4
#define  ST_BLOCK  5
#define  ST_FN  6
#define  ST_BREAK  7
#define  ST_CONTINUE  8
#define  ST_STRUCT  9
#define  ST_IMPL  10
#define  ST_CONST  11
#define  ST_FOR  12
#define  ST_DEFER  13
#define  ST_IMPORT  14
#define  ST_ENUM  15
#define  ST_MATCH  16
#define  ST_SPAWN  17
#define  JSON_NULL  0
#define  JSON_BOOL  1
#define  JSON_INT  2
#define  JSON_STRING  3
#define  JSON_ARRAY  4
#define  JSON_OBJECT  5
static const const char*   FMT_INDENT = "    ";
#define  MODE_EMIT  0
#define  MODE_BUILD  1
#define  MODE_RUN  2
#define  MODE_CHECK  3
#define  MODE_FMT  4

bool   is_keyword (const char*   s);
const char*   token_kind_name (int   k);
Type*   ty_fnptr (Vector__Type*   params, Type*   ret);
Type*   ty_generic (const char*   name, Vector__Type*   args);
Type*   ty_named (const char*   name);
Type*   ty_result (Type*   inner);
Type*   ty_pointer (Type*   inner);
Expr*   expr_int (int   n, int   line, int   col);
Expr*   expr_string (const char*   s, int   line, int   col);
Expr*   expr_ident (const char*   name, int   line, int   col);
Expr*   expr_bool (bool   b, int   line, int   col);
Expr*   expr_binary (int   op, Expr*   lhs, Expr*   rhs);
Expr*   expr_call (Expr*   callee, Vector__Expr*   args);
Stmt*   stmt_let (const char*   name, Type*   ty, Expr*   value, bool   is_mut, int   line, int   col);
Stmt*   stmt_return (Expr*   v, int   line, int   col);
Stmt*   stmt_expr (Expr*   e);
Stmt*   stmt_fn (const char*   name, Vector__Param*   params, Type*   ret_ty, Vector__Stmt*   body, int   line, int   col);
Stmt*   stmt_struct (const char*   name, Vector__Field*   fields, int   line, int   col);
Stmt*   stmt_impl (Type*   target, Vector__Stmt*   methods, int   line, int   col);
Stmt*   stmt_if (Expr*   cond, Vector__Stmt*   then_body, Vector__Stmt*   else_body, int   line, int   col);
Stmt*   stmt_while (Expr*   cond, Vector__Stmt*   body, int   line, int   col);
Stmt*   stmt_for (Stmt*   init, Expr*   cond, Expr*   step, Vector__Stmt*   body, int   line, int   col);
Stmt*   stmt_break (int   line, int   col);
Stmt*   stmt_continue (int   line, int   col);
Stmt*   stmt_const (const char*   name, Type*   ty, Expr*   value, int   line, int   col);
Stmt*   stmt_import (const char*   path, int   line, int   col);
Stmt*   stmt_enum (const char*   name, Vector__EnumVariant*   variants, int   line, int   col);
Stmt*   stmt_spawn (Expr*   call, int   line, int   col);
Stmt*   stmt_match (Expr*   scrutinee, Vector__MatchArm*   arms, int   line, int   col);
Expr*   expr_unary (int   op, Expr*   operand, int   line, int   col);
Expr*   expr_assign (int   op, Expr*   lhs, Expr*   rhs);
Expr*   expr_member (Expr*   obj, const char*   field);
Expr*   expr_index (Expr*   obj, Expr*   idx);
Expr*   expr_cast (Expr*   inner, Type*   target);
Expr*   expr_path (const char*   ty, const char*   member, int   line, int   col);
Expr*   expr_macro (const char*   name, Vector__Expr*   args, int   line, int   col);
Expr*   expr_char (char   c, int   line, int   col);
Expr*   expr_null (int   line, int   col);
Expr*   expr_postinc (Expr*   inner);
Expr*   expr_fnexpr (Vector__Param*   params, Type*   ret_ty, Vector__Stmt*   body, int   line, int   col);
Expr*   expr_postdec (Expr*   inner);
Expr*   expr_struct_lit (const char*   type_name, Vector__string*   names, Vector__Expr*   values, int   line, int   col);
Vector__Stmt*   parse_program (Parser*   p);
Stmt*   parse_top_stmt (Parser*   p);
Stmt*   parse_fn (Parser*   p);
Vector__string*   parse_type_params (Parser*   p);
void   skip_type_params (Parser*   p);
Stmt*   parse_struct (Parser*   p);
Stmt*   parse_enum (Parser*   p);
Stmt*   parse_impl (Parser*   p);
Stmt*   parse_import (Parser*   p);
Vector__Stmt*   parse_block (Parser*   p);
Stmt*   parse_stmt (Parser*   p);
Expr*   parse_maybe_assign (Parser*   p, Expr*   lhs);
Stmt*   parse_let (Parser*   p);
Stmt*   parse_const (Parser*   p);
Stmt*   parse_return (Parser*   p);
Stmt*   parse_match (Parser*   p);
Stmt*   parse_if (Parser*   p);
Stmt*   parse_while (Parser*   p);
Stmt*   parse_for (Parser*   p);
Type*   parse_type (Parser*   p);
Expr*   parse_expr (Parser*   p, int   min_bp);
int   peek_binop_bp (Parser*   p);
Expr*   parse_prefix (Parser*   p);
int   parse_int_lexeme (const char*   s);
bool   starts_uppercase (const char*   s);
char   decode_char_inner (const char*   s);
bool   type_eq (Type*   a, Type*   b);
const char*   type_to_string (Type*   t);
bool   is_int_type (Type*   t);
bool   is_float_type (Type*   t);
bool   is_bool_type (Type*   t);
bool   types_compat (Type*   want, Type*   got);
void   pre_register (Typer*   t, Vector__Stmt*   program);
int   count_borrows (Typer*   t, const char*   name, bool   want_mut);
void   record_borrow (Typer*   t, const char*   source, bool   is_mut, int   line, int   col);
int   enter_borrow_scope (Typer*   t);
void   exit_borrow_scope (Typer*   t, int   saved);
void   check_call_aliasing (Typer*   t, Expr*   call);
void   check_program (Typer*   t, Vector__Stmt*   program);
void   check_top (Typer*   t, Stmt*   s);
void   check_fn (Typer*   t, Stmt*   s);
void   check_stmt (Typer*   t, Stmt*   s);
void   check_let_or_const (Typer*   t, Stmt*   s);
void   check_return (Typer*   t, Stmt*   s);
Type*   infer_expr (Typer*   t, Expr*   e);
const char*   format_spec_for (Type*   t);
Type*   stdlib_method_ret (const char*   ty_name, const char*   method);
void   lift_anons_in_expr (CG*   g, Expr*   e);
void   lift_anons_in_stmt (CG*   g, Stmt*   s);
void   collect_result_in_type (CG*   g, Type*   t);
void   collect_result_in_expr (CG*   g, Expr*   e);
void   collect_result_in_stmt (CG*   g, Stmt*   s);
void   emit_result_runtime (CG*   g);
void   collect_chan_in_type (CG*   g, Type*   t);
void   collect_chan_in_expr (CG*   g, Expr*   e);
void   collect_chan_in_stmt (CG*   g, Stmt*   s);
void   emit_chan_runtime (CG*   g);
void   pre_emit_spawn_stubs_in_stmt (CG*   g, Stmt*   s);
void   emit_spawn_stub (CG*   g, Expr*   call, int   id);
const char*   int_to_str (int   n);
const char*   digit_str (int   d);
bool   is_stdlib_primitive (const char*   name);
void   emit_stdlib_runtime (void);
Type*   strip_ptr (Type*   t);
Type*   infer_for_codegen (CG*   g, Expr*   e);
const char*   type_to_c (Type*   t);
const char*   fnptr_cast_str (Type*   t);
const char*   mangle_generic (const char*   name, Vector__Type*   args);
const char*   mangle_type (Type*   t);
bool   try_mono_call (CG*   g, Expr*   call, Type*   ret_hint);
void   prescan_expr (CG*   g, Expr*   e, Type*   ret_hint);
void   prescan_stmt (CG*   g, Stmt*   s);
void   emit_mono_forward_decl (CG*   g, const char*   mangled, Stmt*   template, Vector__Type*   args);
void   unify_for_inference (Type*   pat, Type*   actual, Vector__string*   params, HashMap__Type*   bindings);
Type*   subst_type (Type*   t, Vector__string*   params, Vector__Type*   args);
Expr*   subst_expr (Expr*   e, Vector__string*   params, Vector__Type*   args, HashMap__Stmt*   gs);
Stmt*   subst_stmt (Stmt*   s, Vector__string*   params, Vector__Type*   args, HashMap__Stmt*   gs);
const char*   op_to_c (int   op);
const char*   unop_to_c (int   op);
void   ind (int   n);
void   emit_expr (CG*   g, Expr*   e);
void   emit_stmt (CG*   g, Stmt*   s, int   depth);
void   emit_fn_signature (Stmt*   s);
Type*   try_constrain_from_expr (CG*   g, Expr*   e, const char*   var, const char*   gen_name, Vector__string*   tparams);
Type*   try_constrain_from_stmt (CG*   g, Stmt*   s, const char*   var, const char*   gen_name, Vector__string*   tparams);
void   populate_scope_from_stmt (CG*   g, Stmt*   s);
Type*   try_resolve_open_let (CG*   g, Stmt*   s, Vector__Stmt*   body, int   start);
void   infer_let_types (CG*   g, Vector__Stmt*   body);
void   infer_let_types_in_program (CG*   g, Vector__Stmt*   program);
void   emit_fn (CG*   g, Stmt*   s);
void   emit_impl (CG*   g, Stmt*   s);
void   emit_struct (Stmt*   s);
void   emit_enum (Stmt*   s);
void   emit_program (Vector__Stmt*   program);
void   emit_top_const (CG*   g, Stmt*   s);
void   emit_struct_mono (CG*   g, Type*   t);
void   collect_generic_uses_in_type (Type*   t, Vector__Type*   out);
void   collect_generic_uses_in_stmt (Stmt*   s, Vector__Type*   out);
JsonValue*   json_null (void);
JsonValue*   json_bool (bool   b);
JsonValue*   json_int (int   n);
JsonValue*   json_string (const char*   s);
JsonValue*   json_array (void);
JsonValue*   json_object (void);
void   json_arr_push (JsonValue*   arr, JsonValue*   v);
void   json_obj_set (JsonValue*   obj, const char*   key, JsonValue*   v);
JsonValue*   json_get (JsonValue*   v, const char*   key);
JsonValue*   json_at (JsonValue*   v, int   idx);
const char*   json_as_string (JsonValue*   v);
int   json_as_int (JsonValue*   v);
bool   json_as_bool (JsonValue*   v);
bool   json_is_null (JsonValue*   v);
int   jp_peek (JsonParser*   p);
int   jp_advance (JsonParser*   p);
void   jp_skip_ws (JsonParser*   p);
JsonValue*   jp_value (JsonParser*   p);
JsonValue*   jp_object (JsonParser*   p);
JsonValue*   jp_array (JsonParser*   p);
JsonValue*   jp_string_value (JsonParser*   p);
const char*   jp_unescape (const char*   raw);
JsonValue*   jp_bool (JsonParser*   p);
JsonValue*   jp_null (JsonParser*   p);
JsonValue*   jp_number (JsonParser*   p);
JsonValue*   json_parse (const char*   src);
const char*   json_escape (const char*   s);
const char*   json_emit (JsonValue*   v);
const char*   fmt_indent (int   depth);
const char*   fmt_type (Type*   t);
const char*   fmt_char_lit (int   n);
const char*   fmt_binop (int   op);
const char*   fmt_unop (int   op);
const char*   fmt_expr (Expr*   e);
const char*   fmt_struct_lit (Expr*   e, const char*   prefix);
const char*   fmt_stmt_inline (Stmt*   s);
const char*   fmt_let_const_inline (Stmt*   s);
const char*   fmt_stmt (Stmt*   s, int   depth);
const char*   fmt_for_init (Stmt*   s);
const char*   fmt_type_params (Stmt*   s);
const char*   fmt_fn (Stmt*   s, int   depth);
const char*   fmt_struct (Stmt*   s, int   depth);
const char*   fmt_enum (Stmt*   s, int   depth);
const char*   fmt_impl (Stmt*   s, int   depth);
const char*   fmt_match (Stmt*   s, int   depth);
const char*   fmt_program (Vector__Stmt*   stmts);
bool   needs_blank_line (int   prev, int   curr);
const char*   __glide_read_line (void);
const char*   __glide_read_bytes (int   n);
void   __glide_write_str (const char*   s);
void   __glide_flush_stdout (void);
void   __glide_log (const char*   s);
void   __glide_set_binary_io (void);
LspState*   lsp_state_new (void);
const char*   lsp_read_message (void);
int   parse_int_str (const char*   s);
void   lsp_send (const char*   payload);
void   lsp_send_response (JsonValue*   id, JsonValue*   result);
void   lsp_send_notification (const char*   method, JsonValue*   params);
void   handle_initialize (JsonValue*   req);
void   handle_shutdown (JsonValue*   req, LspState*   state);
JsonValue*   diag_to_json (DiagEntry*   d);
const char*   uri_to_path (const char*   uri);
const char*   find_stdlib_root (void);
void   run_analysis_and_publish (const char*   uri, const char*   text, LspState*   state);
void   handle_did_open (JsonValue*   req, LspState*   state);
void   handle_did_change (JsonValue*   req, LspState*   state);
void   handle_did_close (JsonValue*   req, LspState*   state);
void   run_extra_analyses (Typer*   t, Vector__Stmt*   stmts);
const char*   type_to_string_pretty (Type*   t);
const char*   fn_signature (Stmt*   s);
Stmt*   find_top_decl (Vector__Stmt*   stmts, const char*   name);
const char*   word_at (const char*   text, int   line0, int   col0);
void   handle_hover (JsonValue*   req, LspState*   state);
int   symbol_kind_for (Stmt*   s);
JsonValue*   position_to_json (int   line1, int   col1);
JsonValue*   range_for_stmt (Stmt*   s);
void   handle_document_symbol (JsonValue*   req, LspState*   state);
void   handle_definition (JsonValue*   req, LspState*   state);
int   ci_kind_for (Stmt*   s);
Stmt*   fn_containing (Vector__Stmt*   stmts, int   line0);
void   collect_locals (Vector__Stmt*   body, int   before_line, Vector__Stmt*   out);
void   collect_locals_stmt (Stmt*   s, int   before_line, Vector__Stmt*   out);
JsonValue*   completion_item (const char*   label, int   kind, const char*   detail);
void   handle_completion (JsonValue*   req, LspState*   state);
void   collect_uses_in_expr (Expr*   e, const char*   name, Vector__UseSite*   out);
void   collect_uses_in_stmt (Stmt*   s, const char*   name, Vector__UseSite*   out);
JsonValue*   use_to_range (UseSite*   u, int   name_len);
void   handle_references (JsonValue*   req, LspState*   state);
void   handle_document_highlight (JsonValue*   req, LspState*   state);
void   handle_rename (JsonValue*   req, LspState*   state);
int   count_lines (const char*   s);
void   handle_formatting (JsonValue*   req, LspState*   state);
void   analysis_unused_vars (Typer*   t, Vector__Stmt*   program);
void   check_unused_in_body (Typer*   t, Vector__Stmt*   body);
bool   stmt_uses_name (Stmt*   s, const char*   name);
bool   expr_uses_name (Expr*   e, const char*   name);
void   analysis_arena_not_freed (Typer*   t, Vector__Stmt*   program);
void   check_arena_in_body (Typer*   t, Vector__Stmt*   body);
bool   arena_is_freed_in_body (const char*   name, Vector__Stmt*   body, int   start);
bool   stmt_calls_method (Stmt*   s, const char*   var, const char*   method);
bool   expr_calls_method (Expr*   e, const char*   var, const char*   method);
void   analysis_addr_of_temporary (Typer*   t, Vector__Stmt*   program);
void   check_addr_temp_body (Typer*   t, Vector__Stmt*   body);
void   analysis_dead_code (Typer*   t, Vector__Stmt*   program);
void   check_dead_code_body (Typer*   t, Vector__Stmt*   body);
void   analysis_unused_fn (Typer*   t, Vector__Stmt*   program);
void   collect_calls_body (Vector__Stmt*   body, HashMap__bool*   out);
void   collect_calls_stmt (Stmt*   s, HashMap__bool*   out);
void   collect_calls_expr (Expr*   e, HashMap__bool*   out);
void   analysis_unnecessary_mut (Typer*   t, Vector__Stmt*   program);
void   check_mut_body (Typer*   t, Vector__Stmt*   body);
bool   stmt_reassigns (Stmt*   s, const char*   name);
bool   expr_reassigns (Expr*   e, const char*   name);
void   analysis_missing_return (Typer*   t, Vector__Stmt*   program);
bool   body_always_returns (Vector__Stmt*   body);
bool   stmt_terminates (Stmt*   s);
void   analysis_unused_params (Typer*   t, Vector__Stmt*   program);
void   check_unused_params_fn (Typer*   t, Stmt*   fnstmt);
int   type_size_bytes (Type*   t, HashMap__Stmt*   structs);
void   analysis_large_return (Typer*   t, Vector__Stmt*   program);
int   lsp_main (void);
void   print_indent (int   n);
void   print_type (Type*   t);
const char*   op_name (int   op);
void   print_expr (Expr*   e, int   indent);
void   print_stmt (Stmt*   s, int   indent);
void   load_into (Vector__Stmt*   stmts, const char*   path, HashMap__bool*   loaded);
void   load_into_str (Vector__Stmt*   stmts, const char*   src, const char*   origin, HashMap__bool*   loaded);
const char*   strip_quotes (const char*   s);
const char*   parent_dir (const char*   path);
const char*   resolve_import (const char*   base, const char*   ipath);
int   __glide_redirect_to (const char*   path);
void   __glide_restore_stdout (int   saved);
int   __glide_shell (const char*   cmd);
const char*   __glide_exe_path (void);
const char*   __glide_exe_dir (void);
const char*   __glide_getenv (const char*   name);
int   __glide_is_windows (void);
bool   __glide_file_exists (const char*   path);
bool   write_file (const char*   path, const char*   content);
void   print_usage (void);
bool   parse_program_into (Vector__Stmt*   stmts, const char*   path);
const char*   pick_cc (void);
int   run_build (const char*   src_path, const char*   out_path, const char*   target, bool   then_run);
int main(int __glide_main_argc, char** __glide_main_argv);
Vector__Stmt*   Vector_new__Stmt (void);
void   Vector_push__Stmt (Vector__Stmt*   self, Stmt   x);
Vector__Param*   Vector_new__Param (void);
void   Vector_push__Param (Vector__Param*   self, Param   x);
Vector__string*   Vector_new__string (void);
void   Vector_push__string (Vector__string*   self, const char*   x);
Vector__Field*   Vector_new__Field (void);
void   Vector_push__Field (Vector__Field*   self, Field   x);
Vector__EnumVariant*   Vector_new__EnumVariant (void);
Vector__Type*   Vector_new__Type (void);
void   Vector_push__Type (Vector__Type*   self, Type   x);
void   Vector_push__EnumVariant (Vector__EnumVariant*   self, EnumVariant   x);
Vector__MatchArm*   Vector_new__MatchArm (void);
int   Vector_len__Stmt (Vector__Stmt*   self);
Stmt   Vector_get__Stmt (Vector__Stmt*   self, int   i);
void   Vector_push__MatchArm (Vector__MatchArm*   self, MatchArm   x);
Vector__Expr*   Vector_new__Expr (void);
void   Vector_push__Expr (Vector__Expr*   self, Expr   x);
HashMap__FnSig*   HashMap_new__FnSig (void);
HashMap__bool*   HashMap_new__bool (void);
HashMap__Type*   HashMap_new__Type (void);
Vector__BorrowEvent*   Vector_new__BorrowEvent (void);
Vector__DiagEntry*   Vector_new__DiagEntry (void);
void   HashMap_free__FnSig (HashMap__FnSig*   self);
void   HashMap_free__bool (HashMap__bool*   self);
void   HashMap_free__Type (HashMap__Type*   self);
void   Vector_free__BorrowEvent (Vector__BorrowEvent*   self);
void   Vector_free__DiagEntry (Vector__DiagEntry*   self);
void   Vector_push__DiagEntry (Vector__DiagEntry*   self, DiagEntry   x);
void   HashMap_insert__Type (HashMap__Type*   self, const char*   k, Type   v);
void   HashMap_insert__FnSig (HashMap__FnSig*   self, const char*   k, FnSig   v);
void   HashMap_insert__bool (HashMap__bool*   self, const char*   k, bool   v);
int   Vector_len__Field (Vector__Field*   self);
Field   Vector_get__Field (Vector__Field*   self, int   i);
int   Vector_len__BorrowEvent (Vector__BorrowEvent*   self);
BorrowEvent   Vector_get__BorrowEvent (Vector__BorrowEvent*   self, int   i);
void   Vector_push__BorrowEvent (Vector__BorrowEvent*   self, BorrowEvent   x);
BorrowEvent   Vector_pop__BorrowEvent (Vector__BorrowEvent*   self);
Vector__bool*   Vector_new__bool (void);
int   Vector_len__Expr (Vector__Expr*   self);
Expr   Vector_get__Expr (Vector__Expr*   self, int   i);
void   Vector_push__bool (Vector__bool*   self, bool   x);
int   Vector_len__string (Vector__string*   self);
const char*   Vector_get__string (Vector__string*   self, int   i);
bool   Vector_get__bool (Vector__bool*   self, int   i);
int   Vector_len__Param (Vector__Param*   self);
Param   Vector_get__Param (Vector__Param*   self, int   i);
bool   HashMap_contains__bool (HashMap__bool*   self, const char*   k);
bool   HashMap_contains__Type (HashMap__Type*   self, const char*   k);
Type   HashMap_get__Type (HashMap__Type*   self, const char*   k);
bool   HashMap_contains__FnSig (HashMap__FnSig*   self, const char*   k);
FnSig   HashMap_get__FnSig (HashMap__FnSig*   self, const char*   k);
void   Vector_push__AnonFn (Vector__AnonFn*   self, AnonFn   x);
int   Vector_len__Type (Vector__Type*   self);
Type   Vector_get__Type (Vector__Type*   self, int   i);
bool   HashMap_contains__Stmt (HashMap__Stmt*   self, const char*   k);
Stmt   HashMap_get__Stmt (HashMap__Stmt*   self, const char*   k);
HashMap__Stmt*   HashMap_new__Stmt (void);
Vector__FnMonoEntry*   Vector_new__FnMonoEntry (void);
Vector__AnonFn*   Vector_new__AnonFn (void);
Expr   Vector_pop__Expr (Vector__Expr*   self);
void   HashMap_free__Stmt (HashMap__Stmt*   self);
void   Vector_push__FnMonoEntry (Vector__FnMonoEntry*   self, FnMonoEntry   x);
void   HashMap_clear__Type (HashMap__Type*   self);
int   Vector_len__AnonFn (Vector__AnonFn*   self);
AnonFn   Vector_get__AnonFn (Vector__AnonFn*   self, int   i);
int   Vector_len__MatchArm (Vector__MatchArm*   self);
MatchArm   Vector_get__MatchArm (Vector__MatchArm*   self, int   i);
void   Vector_set__Stmt (Vector__Stmt*   self, int   i, Stmt   x);
int   Vector_len__EnumVariant (Vector__EnumVariant*   self);
EnumVariant   Vector_get__EnumVariant (Vector__EnumVariant*   self, int   i);
void   HashMap_insert__Stmt (HashMap__Stmt*   self, const char*   k, Stmt   v);
int   Vector_len__FnMonoEntry (Vector__FnMonoEntry*   self);
FnMonoEntry   Vector_get__FnMonoEntry (Vector__FnMonoEntry*   self, int   i);
FnMonoEntry   Vector_pop__FnMonoEntry (Vector__FnMonoEntry*   self);
Vector__JsonValue*   Vector_new__JsonValue (void);
void   Vector_push__JsonValue (Vector__JsonValue*   self, JsonValue   x);
JsonValue   Vector_get__JsonValue (Vector__JsonValue*   self, int   i);
int   Vector_len__JsonValue (Vector__JsonValue*   self);
HashMap__LspDoc*   HashMap_new__LspDoc (void);
bool   HashMap_contains__LspDoc (HashMap__LspDoc*   self, const char*   k);
LspDoc   HashMap_get__LspDoc (HashMap__LspDoc*   self, const char*   k);
void   HashMap_insert__LspDoc (HashMap__LspDoc*   self, const char*   k, LspDoc   v);
int   Vector_len__DiagEntry (Vector__DiagEntry*   self);
DiagEntry   Vector_get__DiagEntry (Vector__DiagEntry*   self, int   i);
bool   HashMap_remove__LspDoc (HashMap__LspDoc*   self, const char*   k);
void   Vector_push__UseSite (Vector__UseSite*   self, UseSite   x);
Vector__UseSite*   Vector_new__UseSite (void);
int   Vector_len__UseSite (Vector__UseSite*   self);
UseSite   Vector_get__UseSite (Vector__UseSite*   self, int   i);
void   HashMap_resize__Type (HashMap__Type*   self, int   new_cap);
int   HashMap_slot__Type (HashMap__Type*   self, const char*   k);
void   HashMap_resize__FnSig (HashMap__FnSig*   self, int   new_cap);
int   HashMap_slot__FnSig (HashMap__FnSig*   self, const char*   k);
void   HashMap_resize__bool (HashMap__bool*   self, int   new_cap);
int   HashMap_slot__bool (HashMap__bool*   self, const char*   k);
int   HashMap_slot__Stmt (HashMap__Stmt*   self, const char*   k);
void   HashMap_resize__Stmt (HashMap__Stmt*   self, int   new_cap);
int   HashMap_slot__LspDoc (HashMap__LspDoc*   self, const char*   k);
void   HashMap_resize__LspDoc (HashMap__LspDoc*   self, int   new_cap);
int   HashMap_hash_key__Type (HashMap__Type*   self, const char*   k);
int   HashMap_hash_key__FnSig (HashMap__FnSig*   self, const char*   k);
int   HashMap_hash_key__bool (HashMap__bool*   self, const char*   k);
int   HashMap_hash_key__Stmt (HashMap__Stmt*   self, const char*   k);
int   HashMap_hash_key__LspDoc (HashMap__LspDoc*   self, const char*   k);


Lexer*   Lexer_new (const char*   src) {
    Lexer*   l = (( Lexer* )malloc(sizeof( Lexer )));
    ((l-> src )  =  src);
    ((l-> src_len )  =  __glide_string_len(src));
    ((l-> pos )  =  0);
    ((l-> line )  =  1);
    ((l-> column )  =  1);
    return l;
}

void   Lexer_free (Lexer*   self) {
    free((( void* )self));
}

char   Lexer_peek (Lexer*   self) {
    if (((self-> pos )  >=  (self-> src_len ))) {
        return (char)0;
    }
    return __glide_string_at((self-> src ), (self-> pos ));
}

char   Lexer_peek_at (Lexer*   self, int   offset) {
    int   i = ((self-> pos )  +  offset);
    if ((i  >=  (self-> src_len ))) {
        return (char)0;
    }
    return __glide_string_at((self-> src ), i);
}

void   Lexer_advance (Lexer*   self) {
    char   c = Lexer_peek(self);
    ((self-> pos )  =  ((self-> pos )  +  1));
    if ((__glide_char_to_int(c)  ==  10)) {
        ((self-> line )  =  ((self-> line )  +  1));
        ((self-> column )  =  1);
    } else {
        ((self-> column )  =  ((self-> column )  +  1));
    }
}

void   Lexer_skip_trivia (Lexer*   self) {
    while (((self-> pos )  <  (self-> src_len ))) {
        char   c = Lexer_peek(self);
        int   ci = __glide_char_to_int(c);
        if (((((ci  ==  32)  ||  (ci  ==  9))  ||  (ci  ==  13))  ||  (ci  ==  10))) {
            Lexer_advance(self);
        } else {
            if ((ci  ==  47)) {
                char   n = Lexer_peek_at(self, 1);
                if ((__glide_char_to_int(n)  ==  47)) {
                    while (((self-> pos )  <  (self-> src_len ))) {
                        char   cc = Lexer_peek(self);
                        if ((__glide_char_to_int(cc)  ==  10)) {
                            break;
                        }
                        Lexer_advance(self);
                    }
                } else {
                    if ((__glide_char_to_int(n)  ==  42)) {
                        Lexer_advance(self);
                        Lexer_advance(self);
                        while (((self-> pos )  <  (self-> src_len ))) {
                            char   cc = Lexer_peek(self);
                            char   nn = Lexer_peek_at(self, 1);
                            if (((__glide_char_to_int(cc)  ==  42)  &&  (__glide_char_to_int(nn)  ==  47))) {
                                Lexer_advance(self);
                                Lexer_advance(self);
                                break;
                            }
                            Lexer_advance(self);
                        }
                    } else {
                        return;
                    }
                }
            } else {
                return;
            }
        }
    }
}

Token   Lexer_next_token (Lexer*   self) {
    Lexer_skip_trivia(self);
    int   start_line = (self-> line );
    int   start_col = (self-> column );
    int   start_pos = (self-> pos );
    if (((self-> pos )  >=  (self-> src_len ))) {
        return (( Token ){. kind  = TOK_EOF, . lexeme  = "", . line  = start_line, . column  = start_col});
    }
    char   c = Lexer_peek(self);
    int   ci = __glide_char_to_int(c);
    if ((__glide_char_is_alpha(c)  ||  (ci  ==  95))) {
        while (((self-> pos )  <  (self-> src_len ))) {
            char   cc = Lexer_peek(self);
            if (((__glide_char_is_alpha(cc)  ||  __glide_char_is_digit(cc))  ||  (__glide_char_to_int(cc)  ==  95))) {
                Lexer_advance(self);
            } else {
                break;
            }
        }
        const char*   lex = __glide_string_substring((self-> src ), start_pos, (self-> pos ));
        int   kind = TOK_IDENT;
        if (is_keyword(lex)) {
            (kind  =  TOK_KEYWORD);
        }
        return (( Token ){. kind  = kind, . lexeme  = lex, . line  = start_line, . column  = start_col});
    }
    if (__glide_char_is_digit(c)) {
        bool   is_float = false;
        while (((self-> pos )  <  (self-> src_len ))) {
            char   cc = Lexer_peek(self);
            if ((__glide_char_is_digit(cc)  ||  (__glide_char_to_int(cc)  ==  95))) {
                Lexer_advance(self);
            } else {
                break;
            }
        }
        if (((__glide_char_to_int(Lexer_peek(self))  ==  46)  &&  __glide_char_is_digit(Lexer_peek_at(self, 1)))) {
            (is_float  =  true);
            Lexer_advance(self);
            while (((self-> pos )  <  (self-> src_len ))) {
                char   cc = Lexer_peek(self);
                if ((__glide_char_is_digit(cc)  ||  (__glide_char_to_int(cc)  ==  95))) {
                    Lexer_advance(self);
                } else {
                    break;
                }
            }
        }
        const char*   lex = __glide_string_substring((self-> src ), start_pos, (self-> pos ));
        int   kind = TOK_INT;
        if (is_float) {
            (kind  =  TOK_FLOAT);
        }
        return (( Token ){. kind  = kind, . lexeme  = lex, . line  = start_line, . column  = start_col});
    }
    if ((ci  ==  34)) {
        Lexer_advance(self);
        while (((self-> pos )  <  (self-> src_len ))) {
            char   cc = Lexer_peek(self);
            if ((__glide_char_to_int(cc)  ==  34)) {
                Lexer_advance(self);
                break;
            }
            if ((__glide_char_to_int(cc)  ==  92)) {
                Lexer_advance(self);
            }
            Lexer_advance(self);
        }
        const char*   lex = __glide_string_substring((self-> src ), start_pos, (self-> pos ));
        return (( Token ){. kind  = TOK_STRING, . lexeme  = lex, . line  = start_line, . column  = start_col});
    }
    if ((ci  ==  39)) {
        Lexer_advance(self);
        if ((__glide_char_to_int(Lexer_peek(self))  ==  92)) {
            Lexer_advance(self);
            Lexer_advance(self);
        } else {
            Lexer_advance(self);
        }
        if ((__glide_char_to_int(Lexer_peek(self))  ==  39)) {
            Lexer_advance(self);
        }
        const char*   lex = __glide_string_substring((self-> src ), start_pos, (self-> pos ));
        return (( Token ){. kind  = TOK_CHAR, . lexeme  = lex, . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  61)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  61))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = "==", . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  33)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  61))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = "!=", . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  60)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  61))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = "<=", . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  62)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  61))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = ">=", . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  38)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  38))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = "&&", . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  124)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  124))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = "||", . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  60)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  60))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = "<<", . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  62)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  62))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = ">>", . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  43)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  43))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = "++", . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  45)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  45))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = "--", . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  45)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  62))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = "->", . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  61)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  62))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = "=>", . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  58)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  58))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = "::", . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  43)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  61))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = "+=", . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  45)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  61))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = "-=", . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  42)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  61))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = "*=", . line  = start_line, . column  = start_col});
    }
    if (((ci  ==  47)  &&  (__glide_char_to_int(Lexer_peek_at(self, 1))  ==  61))) {
        Lexer_advance(self);
        Lexer_advance(self);
        return (( Token ){. kind  = TOK_OP, . lexeme  = "/=", . line  = start_line, . column  = start_col});
    }
    Lexer_advance(self);
    const char*   lex = __glide_string_substring((self-> src ), start_pos, (self-> pos ));
    return (( Token ){. kind  = TOK_OP, . lexeme  = lex, . line  = start_line, . column  = start_col});
}

bool   is_keyword (const char*   s) {
    if (__glide_string_eq(s, "fn")) {
        return true;
    }
    if (__glide_string_eq(s, "let")) {
        return true;
    }
    if (__glide_string_eq(s, "mut")) {
        return true;
    }
    if (__glide_string_eq(s, "const")) {
        return true;
    }
    if (__glide_string_eq(s, "struct")) {
        return true;
    }
    if (__glide_string_eq(s, "enum")) {
        return true;
    }
    if (__glide_string_eq(s, "impl")) {
        return true;
    }
    if (__glide_string_eq(s, "if")) {
        return true;
    }
    if (__glide_string_eq(s, "else")) {
        return true;
    }
    if (__glide_string_eq(s, "while")) {
        return true;
    }
    if (__glide_string_eq(s, "for")) {
        return true;
    }
    if (__glide_string_eq(s, "return")) {
        return true;
    }
    if (__glide_string_eq(s, "break")) {
        return true;
    }
    if (__glide_string_eq(s, "continue")) {
        return true;
    }
    if (__glide_string_eq(s, "match")) {
        return true;
    }
    if (__glide_string_eq(s, "true")) {
        return true;
    }
    if (__glide_string_eq(s, "false")) {
        return true;
    }
    if (__glide_string_eq(s, "null")) {
        return true;
    }
    if (__glide_string_eq(s, "as")) {
        return true;
    }
    if (__glide_string_eq(s, "sizeof")) {
        return true;
    }
    if (__glide_string_eq(s, "new")) {
        return true;
    }
    if (__glide_string_eq(s, "pub")) {
        return true;
    }
    if (__glide_string_eq(s, "import")) {
        return true;
    }
    if (__glide_string_eq(s, "interface")) {
        return true;
    }
    if (__glide_string_eq(s, "defer")) {
        return true;
    }
    if (__glide_string_eq(s, "type")) {
        return true;
    }
    if (__glide_string_eq(s, "move")) {
        return true;
    }
    if (__glide_string_eq(s, "extern")) {
        return true;
    }
    if (__glide_string_eq(s, "c_include")) {
        return true;
    }
    if (__glide_string_eq(s, "c_link")) {
        return true;
    }
    if (__glide_string_eq(s, "spawn")) {
        return true;
    }
    if (__glide_string_eq(s, "chan")) {
        return true;
    }
    return false;
}

const char*   token_kind_name (int   k) {
    if ((k  ==  TOK_EOF)) {
        return "EOF";
    }
    if ((k  ==  TOK_IDENT)) {
        return "IDENT";
    }
    if ((k  ==  TOK_INT)) {
        return "INT";
    }
    if ((k  ==  TOK_FLOAT)) {
        return "FLOAT";
    }
    if ((k  ==  TOK_STRING)) {
        return "STRING";
    }
    if ((k  ==  TOK_CHAR)) {
        return "CHAR";
    }
    if ((k  ==  TOK_KEYWORD)) {
        return "KEYWORD";
    }
    if ((k  ==  TOK_OP)) {
        return "OP";
    }
    return "ERROR";
}

Type*   ty_fnptr (Vector__Type*   params, Type*   ret) {
    Type*   t = (( Type* )calloc(1, sizeof( Type )));
    ((t-> kind )  =  TY_FNPTR);
    ((t-> args )  =  params);
    ((t-> fnptr_ret )  =  ret);
    return t;
}

Type*   ty_generic (const char*   name, Vector__Type*   args) {
    Type*   t = (( Type* )calloc(1, sizeof( Type )));
    ((t-> kind )  =  TY_GENERIC);
    ((t-> name )  =  name);
    ((t-> inner )  =  NULL);
    ((t-> args )  =  args);
    return t;
}

Type*   ty_named (const char*   name) {
    Type*   t = (( Type* )calloc(1, sizeof( Type )));
    ((t-> kind )  =  TY_NAMED);
    ((t-> name )  =  name);
    ((t-> inner )  =  NULL);
    return t;
}

Type*   ty_result (Type*   inner) {
    Type*   t = (( Type* )calloc(1, sizeof( Type )));
    ((t-> kind )  =  TY_RESULT);
    ((t-> inner )  =  inner);
    return t;
}

Type*   ty_pointer (Type*   inner) {
    Type*   t = (( Type* )calloc(1, sizeof( Type )));
    ((t-> kind )  =  TY_POINTER);
    ((t-> name )  =  "");
    ((t-> inner )  =  inner);
    return t;
}

Expr*   expr_int (int   n, int   line, int   col) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_INT);
    ((e-> line )  =  line);
    ((e-> column )  =  col);
    ((e-> int_val )  =  n);
    return e;
}

Expr*   expr_string (const char*   s, int   line, int   col) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_STRING);
    ((e-> line )  =  line);
    ((e-> column )  =  col);
    ((e-> str_val )  =  s);
    return e;
}

Expr*   expr_ident (const char*   name, int   line, int   col) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_IDENT);
    ((e-> line )  =  line);
    ((e-> column )  =  col);
    ((e-> str_val )  =  name);
    return e;
}

Expr*   expr_bool (bool   b, int   line, int   col) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_BOOL);
    ((e-> line )  =  line);
    ((e-> column )  =  col);
    ((e-> bool_val )  =  b);
    return e;
}

Expr*   expr_binary (int   op, Expr*   lhs, Expr*   rhs) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_BINARY);
    ((e-> line )  =  (lhs-> line ));
    ((e-> column )  =  (lhs-> column ));
    ((e-> op_code )  =  op);
    ((e-> lhs )  =  lhs);
    ((e-> rhs )  =  rhs);
    return e;
}

Expr*   expr_call (Expr*   callee, Vector__Expr*   args) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_CALL);
    ((e-> line )  =  (callee-> line ));
    ((e-> column )  =  (callee-> column ));
    ((e-> lhs )  =  callee);
    ((e-> args )  =  args);
    return e;
}

Stmt*   stmt_let (const char*   name, Type*   ty, Expr*   value, bool   is_mut, int   line, int   col) {
    Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((s-> kind )  =  ST_LET);
    ((s-> line )  =  line);
    ((s-> column )  =  col);
    ((s-> name )  =  name);
    ((s-> let_ty )  =  ty);
    ((s-> let_value )  =  value);
    ((s-> is_mut )  =  is_mut);
    return s;
}

Stmt*   stmt_return (Expr*   v, int   line, int   col) {
    Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((s-> kind )  =  ST_RETURN);
    ((s-> line )  =  line);
    ((s-> column )  =  col);
    ((s-> expr_value )  =  v);
    return s;
}

Stmt*   stmt_expr (Expr*   e) {
    Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((s-> kind )  =  ST_EXPR);
    ((s-> line )  =  (e-> line ));
    ((s-> column )  =  (e-> column ));
    ((s-> expr_value )  =  e);
    return s;
}

Stmt*   stmt_fn (const char*   name, Vector__Param*   params, Type*   ret_ty, Vector__Stmt*   body, int   line, int   col) {
    Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((s-> kind )  =  ST_FN);
    ((s-> line )  =  line);
    ((s-> column )  =  col);
    ((s-> name )  =  name);
    ((s-> fn_params )  =  params);
    ((s-> fn_ret_ty )  =  ret_ty);
    ((s-> fn_body )  =  body);
    return s;
}

Stmt*   stmt_struct (const char*   name, Vector__Field*   fields, int   line, int   col) {
    Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((s-> kind )  =  ST_STRUCT);
    ((s-> line )  =  line);
    ((s-> column )  =  col);
    ((s-> name )  =  name);
    ((s-> struct_fields )  =  fields);
    return s;
}

Stmt*   stmt_impl (Type*   target, Vector__Stmt*   methods, int   line, int   col) {
    Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((s-> kind )  =  ST_IMPL);
    ((s-> line )  =  line);
    ((s-> column )  =  col);
    ((s-> impl_target )  =  target);
    ((s-> impl_methods )  =  methods);
    return s;
}

Stmt*   stmt_if (Expr*   cond, Vector__Stmt*   then_body, Vector__Stmt*   else_body, int   line, int   col) {
    Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((s-> kind )  =  ST_IF);
    ((s-> line )  =  line);
    ((s-> column )  =  col);
    ((s-> cond )  =  cond);
    ((s-> then_body )  =  then_body);
    ((s-> else_body )  =  else_body);
    return s;
}

Stmt*   stmt_while (Expr*   cond, Vector__Stmt*   body, int   line, int   col) {
    Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((s-> kind )  =  ST_WHILE);
    ((s-> line )  =  line);
    ((s-> column )  =  col);
    ((s-> cond )  =  cond);
    ((s-> then_body )  =  body);
    return s;
}

Stmt*   stmt_for (Stmt*   init, Expr*   cond, Expr*   step, Vector__Stmt*   body, int   line, int   col) {
    Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((s-> kind )  =  ST_FOR);
    ((s-> line )  =  line);
    ((s-> column )  =  col);
    ((s-> for_init )  =  init);
    ((s-> cond )  =  cond);
    ((s-> for_step )  =  step);
    ((s-> then_body )  =  body);
    return s;
}

Stmt*   stmt_break (int   line, int   col) {
    Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((s-> kind )  =  ST_BREAK);
    ((s-> line )  =  line);
    ((s-> column )  =  col);
    return s;
}

Stmt*   stmt_continue (int   line, int   col) {
    Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((s-> kind )  =  ST_CONTINUE);
    ((s-> line )  =  line);
    ((s-> column )  =  col);
    return s;
}

Stmt*   stmt_const (const char*   name, Type*   ty, Expr*   value, int   line, int   col) {
    Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((s-> kind )  =  ST_CONST);
    ((s-> line )  =  line);
    ((s-> column )  =  col);
    ((s-> name )  =  name);
    ((s-> let_ty )  =  ty);
    ((s-> let_value )  =  value);
    return s;
}

Stmt*   stmt_import (const char*   path, int   line, int   col) {
    Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((s-> kind )  =  ST_IMPORT);
    ((s-> line )  =  line);
    ((s-> column )  =  col);
    ((s-> import_path )  =  path);
    return s;
}

Stmt*   stmt_enum (const char*   name, Vector__EnumVariant*   variants, int   line, int   col) {
    Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((s-> kind )  =  ST_ENUM);
    ((s-> line )  =  line);
    ((s-> column )  =  col);
    ((s-> name )  =  name);
    ((s-> enum_variants )  =  variants);
    return s;
}

Stmt*   stmt_spawn (Expr*   call, int   line, int   col) {
    Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((s-> kind )  =  ST_SPAWN);
    ((s-> line )  =  line);
    ((s-> column )  =  col);
    ((s-> expr_value )  =  call);
    return s;
}

Stmt*   stmt_match (Expr*   scrutinee, Vector__MatchArm*   arms, int   line, int   col) {
    Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((s-> kind )  =  ST_MATCH);
    ((s-> line )  =  line);
    ((s-> column )  =  col);
    ((s-> scrutinee )  =  scrutinee);
    ((s-> arms )  =  arms);
    return s;
}

Expr*   expr_unary (int   op, Expr*   operand, int   line, int   col) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_UNARY);
    ((e-> line )  =  line);
    ((e-> column )  =  col);
    ((e-> op_code )  =  op);
    ((e-> operand )  =  operand);
    return e;
}

Expr*   expr_assign (int   op, Expr*   lhs, Expr*   rhs) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_ASSIGN);
    ((e-> line )  =  (lhs-> line ));
    ((e-> column )  =  (lhs-> column ));
    ((e-> op_code )  =  op);
    ((e-> lhs )  =  lhs);
    ((e-> rhs )  =  rhs);
    return e;
}

Expr*   expr_member (Expr*   obj, const char*   field) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_MEMBER);
    ((e-> line )  =  (obj-> line ));
    ((e-> column )  =  (obj-> column ));
    ((e-> lhs )  =  obj);
    ((e-> field )  =  field);
    return e;
}

Expr*   expr_index (Expr*   obj, Expr*   idx) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_INDEX);
    ((e-> line )  =  (obj-> line ));
    ((e-> column )  =  (obj-> column ));
    ((e-> lhs )  =  obj);
    ((e-> rhs )  =  idx);
    return e;
}

Expr*   expr_cast (Expr*   inner, Type*   target) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_CAST);
    ((e-> line )  =  (inner-> line ));
    ((e-> column )  =  (inner-> column ));
    ((e-> lhs )  =  inner);
    ((e-> cast_to )  =  target);
    return e;
}

Expr*   expr_path (const char*   ty, const char*   member, int   line, int   col) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_PATH);
    ((e-> line )  =  line);
    ((e-> column )  =  col);
    ((e-> str_val )  =  ty);
    ((e-> field )  =  member);
    return e;
}

Expr*   expr_macro (const char*   name, Vector__Expr*   args, int   line, int   col) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_MACRO);
    ((e-> line )  =  line);
    ((e-> column )  =  col);
    ((e-> str_val )  =  name);
    ((e-> args )  =  args);
    return e;
}

Expr*   expr_char (char   c, int   line, int   col) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_CHAR);
    ((e-> line )  =  line);
    ((e-> column )  =  col);
    ((e-> int_val )  =  __glide_char_to_int(c));
    return e;
}

Expr*   expr_null (int   line, int   col) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_NULL);
    ((e-> line )  =  line);
    ((e-> column )  =  col);
    return e;
}

Expr*   expr_postinc (Expr*   inner) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_POSTINC);
    ((e-> line )  =  (inner-> line ));
    ((e-> column )  =  (inner-> column ));
    ((e-> lhs )  =  inner);
    return e;
}

Expr*   expr_fnexpr (Vector__Param*   params, Type*   ret_ty, Vector__Stmt*   body, int   line, int   col) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_FNEXPR);
    ((e-> line )  =  line);
    ((e-> column )  =  col);
    ((e-> cast_to )  =  ret_ty);
    ((e-> fn_expr_params )  =  params);
    ((e-> fn_expr_body )  =  body);
    return e;
}

Expr*   expr_postdec (Expr*   inner) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_POSTDEC);
    ((e-> line )  =  (inner-> line ));
    ((e-> column )  =  (inner-> column ));
    ((e-> lhs )  =  inner);
    return e;
}

Expr*   expr_struct_lit (const char*   type_name, Vector__string*   names, Vector__Expr*   values, int   line, int   col) {
    Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
    ((e-> kind )  =  EX_STRUCT_LIT);
    ((e-> line )  =  line);
    ((e-> column )  =  col);
    ((e-> str_val )  =  type_name);
    ((e-> field_names )  =  names);
    ((e-> args )  =  values);
    ((e-> field )  =  "");
    return e;
}

Parser*   Parser_new (Lexer*   lex) {
    Parser*   p = (( Parser* )malloc(sizeof( Parser )));
    ((p-> lex )  =  lex);
    ((p-> current )  =  Lexer_next_token(lex));
    ((p-> peek )  =  Lexer_next_token(lex));
    ((p-> error_count )  =  0);
    ((p-> no_struct_lit )  =  false);
    return p;
}

void   Parser_free (Parser*   self) {
    free((( void* )self));
}

Token   Parser_advance (Parser*   self) {
    Token   prev = (self-> current );
    ((self-> current )  =  (self-> peek ));
    ((self-> peek )  =  Lexer_next_token((self-> lex )));
    return prev;
}

bool   Parser_at_eof (Parser*   self) {
    return (((self-> current ). kind )  ==  TOK_EOF);
}

bool   Parser_at_kw (Parser*   self, const char*   lexeme) {
    if ((((self-> current ). kind )  !=  TOK_KEYWORD)) {
        return false;
    }
    return __glide_string_eq(((self-> current ). lexeme ), lexeme);
}

bool   Parser_at_op (Parser*   self, const char*   lexeme) {
    if ((((self-> current ). kind )  !=  TOK_OP)) {
        return false;
    }
    return __glide_string_eq(((self-> current ). lexeme ), lexeme);
}

bool   Parser_peek_op (Parser*   self, const char*   lexeme) {
    if ((((self-> peek ). kind )  !=  TOK_OP)) {
        return false;
    }
    return __glide_string_eq(((self-> peek ). lexeme ), lexeme);
}

bool   Parser_eat_kw (Parser*   self, const char*   lexeme) {
    if (Parser_at_kw(self, lexeme)) {
        Parser_advance(self);
        return true;
    }
    return false;
}

bool   Parser_eat_op (Parser*   self, const char*   lexeme) {
    if (Parser_at_op(self, lexeme)) {
        Parser_advance(self);
        return true;
    }
    return false;
}

void   Parser_err (Parser*   self, const char*   msg) {
    printf("%s %d %s %d %s %s %s %s %s\n", "parse error at", ((self-> current ). line ), ":", ((self-> current ). column ), "-", msg, "near '", ((self-> current ). lexeme ), "'");
    ((self-> error_count )  =  ((self-> error_count )  +  1));
}

bool   Parser_expect_op (Parser*   self, const char*   lexeme) {
    if (Parser_at_op(self, lexeme)) {
        Parser_advance(self);
        return true;
    }
    Parser_err(self, __glide_string_concat(__glide_string_concat("expected '", lexeme), "'"));
    return false;
}

const char*   Parser_expect_ident (Parser*   self) {
    if (((((self-> current ). kind )  ==  TOK_IDENT)  ||  (((self-> current ). kind )  ==  TOK_KEYWORD))) {
        const char*   name = ((self-> current ). lexeme );
        Parser_advance(self);
        return name;
    }
    Parser_err(self, "expected identifier");
    return "";
}

Vector__Stmt*   parse_program (Parser*   p) {
    Vector__Stmt*   stmts = Vector_new__Stmt();
    while ((!Parser_at_eof(p))) {
        Stmt*   s = parse_top_stmt(p);
        if ((s  ==  NULL)) {
            Parser_advance(p);
            continue;
        }
        Vector_push__Stmt(stmts, (*s));
    }
    return stmts;
}

Stmt*   parse_top_stmt (Parser*   p) {
    bool   is_pub = false;
    if (Parser_eat_kw(p, "pub")) {
        (is_pub  =  true);
    }
    if (Parser_eat_kw(p, "extern")) {
        if (Parser_at_kw(p, "fn")) {
            Stmt*   s = parse_fn(p);
            if ((s  !=  NULL)) {
                ((s-> is_pub )  =  is_pub);
                ((s-> fn_body )  =  NULL);
            }
            return s;
        }
        while (((!Parser_at_op(p, ";"))  &&  (!Parser_at_eof(p)))) {
            Parser_advance(p);
        }
        Parser_eat_op(p, ";");
        return NULL;
    }
    if (Parser_at_kw(p, "fn")) {
        Stmt*   s = parse_fn(p);
        if ((s  !=  NULL)) {
            ((s-> is_pub )  =  is_pub);
        }
        return s;
    }
    if (Parser_at_kw(p, "let")) {
        Stmt*   s = parse_let(p);
        if ((s  !=  NULL)) {
            ((s-> is_pub )  =  is_pub);
        }
        return s;
    }
    if (Parser_at_kw(p, "const")) {
        Stmt*   s = parse_const(p);
        if ((s  !=  NULL)) {
            ((s-> is_pub )  =  is_pub);
        }
        return s;
    }
    if (Parser_at_kw(p, "struct")) {
        Stmt*   s = parse_struct(p);
        if ((s  !=  NULL)) {
            ((s-> is_pub )  =  is_pub);
        }
        return s;
    }
    if (Parser_at_kw(p, "enum")) {
        Stmt*   s = parse_enum(p);
        if ((s  !=  NULL)) {
            ((s-> is_pub )  =  is_pub);
        }
        return s;
    }
    if (Parser_at_kw(p, "impl")) {
        return parse_impl(p);
    }
    if (Parser_at_kw(p, "import")) {
        return parse_import(p);
    }
    Parser_err(p, "unexpected token at top level");
    return NULL;
}

Stmt*   parse_fn (Parser*   p) {
    int   line = ((p-> current ). line );
    int   col = ((p-> current ). column );
    Parser_advance(p);
    const char*   name = Parser_expect_ident(p);
    Vector__string*   tps = parse_type_params(p);
    Parser_expect_op(p, "(");
    Vector__Param*   params = Vector_new__Param();
    while (((!Parser_at_op(p, ")"))  &&  (!Parser_at_eof(p)))) {
        const char*   pname = Parser_expect_ident(p);
        Parser_expect_op(p, ":");
        Type*   pty = parse_type(p);
        Param   prm = (( Param ){. name  = pname, . ty  = pty});
        Vector_push__Param(params, prm);
        if ((!Parser_eat_op(p, ","))) {
            break;
        }
    }
    Parser_expect_op(p, ")");
    Type*   ret_ty = NULL;
    if (Parser_eat_op(p, "->")) {
        (ret_ty  =  parse_type(p));
    }
    if (Parser_eat_op(p, ";")) {
        Stmt*   s = stmt_fn(name, params, ret_ty, NULL, line, col);
        if ((s  !=  NULL)) {
            ((s-> type_params )  =  tps);
        }
        return s;
    }
    Vector__Stmt*   body = parse_block(p);
    Stmt*   s = stmt_fn(name, params, ret_ty, body, line, col);
    if ((s  !=  NULL)) {
        ((s-> type_params )  =  tps);
    }
    return s;
}

Vector__string*   parse_type_params (Parser*   p) {
    Vector__string*   tps = Vector_new__string();
    if ((!Parser_eat_op(p, "<"))) {
        return tps;
    }
    while (((!Parser_at_op(p, ">"))  &&  (!Parser_at_eof(p)))) {
        const char*   name = Parser_expect_ident(p);
        Vector_push__string(tps, name);
        if ((!Parser_eat_op(p, ","))) {
            break;
        }
    }
    Parser_expect_op(p, ">");
    return tps;
}

void   skip_type_params (Parser*   p) {
    Parser_advance(p);
    while (((!Parser_at_op(p, ">"))  &&  (!Parser_at_eof(p)))) {
        Parser_advance(p);
    }
    Parser_eat_op(p, ">");
}

Stmt*   parse_struct (Parser*   p) {
    int   line = ((p-> current ). line );
    int   col = ((p-> current ). column );
    Parser_advance(p);
    const char*   name = Parser_expect_ident(p);
    Vector__string*   tps = parse_type_params(p);
    Parser_expect_op(p, "{");
    Vector__Field*   fields = Vector_new__Field();
    while (((!Parser_at_op(p, "}"))  &&  (!Parser_at_eof(p)))) {
        const char*   fname = Parser_expect_ident(p);
        Parser_expect_op(p, ":");
        Type*   fty = parse_type(p);
        Field   f = (( Field ){. name  = fname, . ty  = fty});
        Vector_push__Field(fields, f);
        if ((!Parser_eat_op(p, ","))) {
            break;
        }
    }
    Parser_expect_op(p, "}");
    Stmt*   s = stmt_struct(name, fields, line, col);
    if ((s  !=  NULL)) {
        ((s-> type_params )  =  tps);
    }
    return s;
}

Stmt*   parse_enum (Parser*   p) {
    int   line = ((p-> current ). line );
    int   col = ((p-> current ). column );
    Parser_advance(p);
    const char*   name = Parser_expect_ident(p);
    if (Parser_at_op(p, "<")) {
        skip_type_params(p);
    }
    Parser_expect_op(p, "{");
    Vector__EnumVariant*   variants = Vector_new__EnumVariant();
    while (((!Parser_at_op(p, "}"))  &&  (!Parser_at_eof(p)))) {
        const char*   vname = Parser_expect_ident(p);
        Vector__Type*   fields = Vector_new__Type();
        if (Parser_eat_op(p, "(")) {
            while (((!Parser_at_op(p, ")"))  &&  (!Parser_at_eof(p)))) {
                Type*   t = parse_type(p);
                if ((t  !=  NULL)) {
                    Vector_push__Type(fields, (*t));
                }
                if ((!Parser_eat_op(p, ","))) {
                    break;
                }
            }
            Parser_expect_op(p, ")");
        }
        EnumVariant   v = (( EnumVariant ){. name  = vname, . fields  = fields});
        Vector_push__EnumVariant(variants, v);
        if ((!Parser_eat_op(p, ","))) {
            break;
        }
    }
    Parser_expect_op(p, "}");
    return stmt_enum(name, variants, line, col);
}

Stmt*   parse_impl (Parser*   p) {
    int   line = ((p-> current ). line );
    int   col = ((p-> current ). column );
    Parser_advance(p);
    Vector__string*   tps = parse_type_params(p);
    Type*   target = parse_type(p);
    Parser_expect_op(p, "{");
    Vector__Stmt*   methods = Vector_new__Stmt();
    while (((!Parser_at_op(p, "}"))  &&  (!Parser_at_eof(p)))) {
        bool   is_pub = false;
        if (Parser_eat_kw(p, "pub")) {
            (is_pub  =  true);
        }
        if (Parser_at_kw(p, "fn")) {
            Stmt*   m = parse_fn(p);
            if ((m  !=  NULL)) {
                ((m-> is_pub )  =  is_pub);
                Vector_push__Stmt(methods, (*m));
            }
        } else {
            Parser_err(p, "expected fn inside impl");
            Parser_advance(p);
        }
    }
    Parser_expect_op(p, "}");
    Stmt*   s = stmt_impl(target, methods, line, col);
    if ((s  !=  NULL)) {
        ((s-> type_params )  =  tps);
    }
    return s;
}

Stmt*   parse_import (Parser*   p) {
    int   line = ((p-> current ). line );
    int   col = ((p-> current ). column );
    Parser_advance(p);
    const char*   path = "";
    if ((((p-> current ). kind )  ==  TOK_STRING)) {
        (path  =  ((p-> current ). lexeme ));
        Parser_advance(p);
    } else {
        (path  =  Parser_expect_ident(p));
        while (Parser_eat_op(p, "::")) {
            const char*   part = Parser_expect_ident(p);
            (path  =  __glide_string_concat(__glide_string_concat(path, "/"), part));
        }
    }
    Parser_expect_op(p, ";");
    return stmt_import(path, line, col);
}

Vector__Stmt*   parse_block (Parser*   p) {
    Vector__Stmt*   stmts = Vector_new__Stmt();
    if ((!Parser_expect_op(p, "{"))) {
        return stmts;
    }
    while (((!Parser_at_op(p, "}"))  &&  (!Parser_at_eof(p)))) {
        Stmt*   s = parse_stmt(p);
        if ((s  !=  NULL)) {
            Vector_push__Stmt(stmts, (*s));
        }
    }
    Parser_expect_op(p, "}");
    return stmts;
}

Stmt*   parse_stmt (Parser*   p) {
    if (Parser_at_kw(p, "let")) {
        return parse_let(p);
    }
    if (Parser_at_kw(p, "const")) {
        return parse_const(p);
    }
    if (Parser_at_kw(p, "return")) {
        return parse_return(p);
    }
    if (Parser_at_kw(p, "if")) {
        return parse_if(p);
    }
    if (Parser_at_kw(p, "while")) {
        return parse_while(p);
    }
    if (Parser_at_kw(p, "for")) {
        return parse_for(p);
    }
    if (Parser_at_kw(p, "match")) {
        return parse_match(p);
    }
    if (Parser_at_kw(p, "defer")) {
        int   line = ((p-> current ). line );
        int   col = ((p-> current ). column );
        Parser_advance(p);
        Expr*   e = parse_expr(p, 0);
        Parser_expect_op(p, ";");
        Stmt*   s = (( Stmt* )calloc(1, sizeof( Stmt )));
        ((s-> kind )  =  ST_DEFER);
        ((s-> line )  =  line);
        ((s-> column )  =  col);
        ((s-> expr_value )  =  e);
        return s;
    }
    if (Parser_at_kw(p, "spawn")) {
        int   line = ((p-> current ). line );
        int   col = ((p-> current ). column );
        Parser_advance(p);
        Expr*   e = parse_expr(p, 0);
        Parser_expect_op(p, ";");
        return stmt_spawn(e, line, col);
    }
    if (Parser_at_kw(p, "break")) {
        int   line = ((p-> current ). line );
        int   col = ((p-> current ). column );
        Parser_advance(p);
        Parser_expect_op(p, ";");
        return stmt_break(line, col);
    }
    if (Parser_at_kw(p, "continue")) {
        int   line = ((p-> current ). line );
        int   col = ((p-> current ). column );
        Parser_advance(p);
        Parser_expect_op(p, ";");
        return stmt_continue(line, col);
    }
    if (Parser_at_op(p, "{")) {
        Vector__Stmt*   body = parse_block(p);
        Stmt*   s = (( Stmt* )malloc(sizeof( Stmt )));
        ((s-> kind )  =  ST_BLOCK);
        ((s-> then_body )  =  body);
        return s;
    }
    Expr*   e = parse_expr(p, 0);
    if ((e  ==  NULL)) {
        return NULL;
    }
    Expr*   assigned = parse_maybe_assign(p, e);
    Parser_expect_op(p, ";");
    return stmt_expr(assigned);
}

Expr*   parse_maybe_assign (Parser*   p, Expr*   lhs) {
    int   op = 0;
    if (Parser_at_op(p, "=")) {
        (op  =  OP_ASSIGN);
    } else {
        if (Parser_at_op(p, "+=")) {
            (op  =  OP_PLUS_ASSIGN);
        } else {
            if (Parser_at_op(p, "-=")) {
                (op  =  OP_MINUS_ASSIGN);
            } else {
                if (Parser_at_op(p, "*=")) {
                    (op  =  OP_STAR_ASSIGN);
                } else {
                    if (Parser_at_op(p, "/=")) {
                        (op  =  OP_SLASH_ASSIGN);
                    }
                }
            }
        }
    }
    if ((op  ==  0)) {
        return lhs;
    }
    Parser_advance(p);
    Expr*   rhs_raw = parse_expr(p, 0);
    Expr*   rhs = parse_maybe_assign(p, rhs_raw);
    return expr_assign(op, lhs, rhs);
}

Stmt*   parse_let (Parser*   p) {
    int   line = ((p-> current ). line );
    int   col = ((p-> current ). column );
    Parser_advance(p);
    bool   is_mut = false;
    if (Parser_eat_kw(p, "mut")) {
        (is_mut  =  true);
    }
    const char*   name = Parser_expect_ident(p);
    bool   is_auto_owned = false;
    if (Parser_eat_op(p, "*")) {
        (is_auto_owned  =  true);
    }
    Type*   ty = NULL;
    if (Parser_eat_op(p, ":")) {
        (ty  =  parse_type(p));
    }
    Expr*   value = NULL;
    if (Parser_eat_op(p, "=")) {
        (value  =  parse_expr(p, 0));
    }
    Parser_expect_op(p, ";");
    Stmt*   s = stmt_let(name, ty, value, is_mut, line, col);
    ((s-> is_auto_owned )  =  is_auto_owned);
    return s;
}

Stmt*   parse_const (Parser*   p) {
    int   line = ((p-> current ). line );
    int   col = ((p-> current ). column );
    Parser_advance(p);
    const char*   name = Parser_expect_ident(p);
    Type*   ty = NULL;
    if (Parser_eat_op(p, ":")) {
        (ty  =  parse_type(p));
    }
    Parser_expect_op(p, "=");
    Expr*   value = parse_expr(p, 0);
    Parser_expect_op(p, ";");
    return stmt_const(name, ty, value, line, col);
}

Stmt*   parse_return (Parser*   p) {
    int   line = ((p-> current ). line );
    int   col = ((p-> current ). column );
    Parser_advance(p);
    Expr*   value = NULL;
    if ((!Parser_at_op(p, ";"))) {
        (value  =  parse_expr(p, 0));
    }
    Parser_expect_op(p, ";");
    return stmt_return(value, line, col);
}

Stmt*   parse_match (Parser*   p) {
    int   line = ((p-> current ). line );
    int   col = ((p-> current ). column );
    Parser_advance(p);
    bool   saved = (p-> no_struct_lit );
    ((p-> no_struct_lit )  =  true);
    Expr*   scrut = parse_expr(p, 0);
    ((p-> no_struct_lit )  =  saved);
    Parser_expect_op(p, "{");
    Vector__MatchArm*   arms = Vector_new__MatchArm();
    while (((!Parser_at_op(p, "}"))  &&  (!Parser_at_eof(p)))) {
        const char*   variant = "_";
        Vector__string*   bindings = Vector_new__string();
        if ((((p-> current ). kind )  ==  TOK_IDENT)) {
            const char*   first = ((p-> current ). lexeme );
            Parser_advance(p);
            if (Parser_eat_op(p, "::")) {
                (variant  =  Parser_expect_ident(p));
            } else {
                (variant  =  first);
            }
            if (Parser_eat_op(p, "(")) {
                while (((!Parser_at_op(p, ")"))  &&  (!Parser_at_eof(p)))) {
                    Vector_push__string(bindings, Parser_expect_ident(p));
                    if ((!Parser_eat_op(p, ","))) {
                        break;
                    }
                }
                Parser_expect_op(p, ")");
            }
        }
        Parser_expect_op(p, "=>");
        Vector__Stmt*   body = Vector_new__Stmt();
        if (Parser_at_op(p, "{")) {
            Vector__Stmt*   blk = parse_block(p);
            for (int   i = 0; (i  <  Vector_len__Stmt(blk)); i++) {
                Vector_push__Stmt(body, Vector_get__Stmt(blk, i));
            }
        } else {
            Expr*   e = parse_expr(p, 0);
            Expr*   ass = parse_maybe_assign(p, e);
            Vector_push__Stmt(body, (*stmt_expr(ass)));
        }
        Vector_push__MatchArm(arms, (( MatchArm ){. variant  = variant, . bindings  = bindings, . body  = body}));
        Parser_eat_op(p, ",");
    }
    Parser_expect_op(p, "}");
    return stmt_match(scrut, arms, line, col);
}

Stmt*   parse_if (Parser*   p) {
    int   line = ((p-> current ). line );
    int   col = ((p-> current ). column );
    Parser_advance(p);
    bool   saved = (p-> no_struct_lit );
    ((p-> no_struct_lit )  =  true);
    Expr*   cond = parse_expr(p, 0);
    ((p-> no_struct_lit )  =  saved);
    Vector__Stmt*   then_body = parse_block(p);
    Vector__Stmt*   else_body = NULL;
    if (Parser_eat_kw(p, "else")) {
        if (Parser_at_kw(p, "if")) {
            Stmt*   inner = parse_if(p);
            Vector__Stmt*   one = Vector_new__Stmt();
            if ((inner  !=  NULL)) {
                Vector_push__Stmt(one, (*inner));
            }
            (else_body  =  one);
        } else {
            (else_body  =  parse_block(p));
        }
    }
    return stmt_if(cond, then_body, else_body, line, col);
}

Stmt*   parse_while (Parser*   p) {
    int   line = ((p-> current ). line );
    int   col = ((p-> current ). column );
    Parser_advance(p);
    bool   saved = (p-> no_struct_lit );
    ((p-> no_struct_lit )  =  true);
    Expr*   cond = parse_expr(p, 0);
    ((p-> no_struct_lit )  =  saved);
    Vector__Stmt*   body = parse_block(p);
    return stmt_while(cond, body, line, col);
}

Stmt*   parse_for (Parser*   p) {
    int   line = ((p-> current ). line );
    int   col = ((p-> current ). column );
    Parser_advance(p);
    bool   saved = (p-> no_struct_lit );
    ((p-> no_struct_lit )  =  true);
    Stmt*   init = NULL;
    if ((!Parser_at_op(p, ";"))) {
        if (Parser_at_kw(p, "let")) {
            int   l_line = ((p-> current ). line );
            int   l_col = ((p-> current ). column );
            Parser_advance(p);
            bool   is_mut = false;
            if (Parser_eat_kw(p, "mut")) {
                (is_mut  =  true);
            }
            const char*   lname = Parser_expect_ident(p);
            Type*   lty = NULL;
            if (Parser_eat_op(p, ":")) {
                (lty  =  parse_type(p));
            }
            Expr*   lval = NULL;
            if (Parser_eat_op(p, "=")) {
                (lval  =  parse_expr(p, 0));
            }
            (init  =  stmt_let(lname, lty, lval, is_mut, l_line, l_col));
        } else {
            Expr*   e = parse_expr(p, 0);
            Expr*   assigned = parse_maybe_assign(p, e);
            (init  =  stmt_expr(assigned));
        }
    }
    Parser_expect_op(p, ";");
    Expr*   cond = NULL;
    if ((!Parser_at_op(p, ";"))) {
        (cond  =  parse_expr(p, 0));
    }
    Parser_expect_op(p, ";");
    Expr*   step = NULL;
    if ((!Parser_at_op(p, "{"))) {
        Expr*   e = parse_expr(p, 0);
        (step  =  parse_maybe_assign(p, e));
    }
    ((p-> no_struct_lit )  =  saved);
    Vector__Stmt*   body = parse_block(p);
    return stmt_for(init, cond, step, body, line, col);
}

Type*   parse_type (Parser*   p) {
    if (Parser_eat_op(p, "!")) {
        Type*   inner = parse_type(p);
        return ty_result(inner);
    }
    if (Parser_eat_op(p, "*")) {
        Type*   inner = parse_type(p);
        return ty_pointer(inner);
    }
    if (Parser_eat_op(p, "&")) {
        if (Parser_eat_kw(p, "mut")) {
            Type*   inner = parse_type(p);
            Type*   t = (( Type* )malloc(sizeof( Type )));
            ((t-> kind )  =  TY_BORROW_MUT);
            ((t-> inner )  =  inner);
            return t;
        }
        Type*   inner = parse_type(p);
        Type*   t = (( Type* )malloc(sizeof( Type )));
        ((t-> kind )  =  TY_BORROW);
        ((t-> inner )  =  inner);
        return t;
    }
    if ((Parser_at_op(p, "[")  &&  Parser_peek_op(p, "]"))) {
        Parser_advance(p);
        Parser_advance(p);
        Type*   inner = parse_type(p);
        Type*   t = (( Type* )malloc(sizeof( Type )));
        ((t-> kind )  =  TY_SLICE);
        ((t-> inner )  =  inner);
        return t;
    }
    if (Parser_at_kw(p, "fn")) {
        Parser_advance(p);
        Parser_expect_op(p, "(");
        Vector__Type*   params = Vector_new__Type();
        while (((!Parser_at_op(p, ")"))  &&  (!Parser_at_eof(p)))) {
            if (((((p-> current ). kind )  ==  TOK_IDENT)  &&  Parser_peek_op(p, ":"))) {
                Parser_advance(p);
                Parser_advance(p);
            }
            Type*   t = parse_type(p);
            if ((t  !=  NULL)) {
                Vector_push__Type(params, (*t));
            }
            if ((!Parser_eat_op(p, ","))) {
                break;
            }
        }
        Parser_expect_op(p, ")");
        Type*   ret = NULL;
        if (Parser_eat_op(p, "->")) {
            (ret  =  parse_type(p));
        }
        return ty_fnptr(params, ret);
    }
    if (((((p-> current ). kind )  ==  TOK_IDENT)  ||  (((p-> current ). kind )  ==  TOK_KEYWORD))) {
        const char*   name = ((p-> current ). lexeme );
        Parser_advance(p);
        if (Parser_at_op(p, "<")) {
            Parser_advance(p);
            Vector__Type*   args = Vector_new__Type();
            while (((!Parser_at_op(p, ">"))  &&  (!Parser_at_eof(p)))) {
                Type*   inner = parse_type(p);
                if ((inner  !=  NULL)) {
                    Vector_push__Type(args, (*inner));
                }
                if ((!Parser_eat_op(p, ","))) {
                    break;
                }
            }
            Parser_expect_op(p, ">");
            return ty_generic(name, args);
        }
        return ty_named(name);
    }
    Parser_err(p, "expected type");
    return ty_named("__error__");
}

Expr*   parse_expr (Parser*   p, int   min_bp) {
    Expr*   left = parse_prefix(p);
    if ((left  ==  NULL)) {
        return NULL;
    }
    while (true) {
        if (Parser_at_op(p, "(")) {
            Parser_advance(p);
            Vector__Expr*   args = Vector_new__Expr();
            while (((!Parser_at_op(p, ")"))  &&  (!Parser_at_eof(p)))) {
                Expr*   a = parse_expr(p, 0);
                if ((a  !=  NULL)) {
                    Vector_push__Expr(args, (*a));
                }
                if ((!Parser_eat_op(p, ","))) {
                    break;
                }
            }
            Parser_expect_op(p, ")");
            (left  =  expr_call(left, args));
            continue;
        }
        if (Parser_at_op(p, "[")) {
            Parser_advance(p);
            Expr*   idx = parse_expr(p, 0);
            Parser_expect_op(p, "]");
            (left  =  expr_index(left, idx));
            continue;
        }
        if (Parser_at_op(p, ".")) {
            Parser_advance(p);
            const char*   name = Parser_expect_ident(p);
            (left  =  expr_member(left, name));
            continue;
        }
        if (Parser_at_op(p, "++")) {
            Parser_advance(p);
            (left  =  expr_postinc(left));
            continue;
        }
        if (Parser_at_op(p, "--")) {
            Parser_advance(p);
            (left  =  expr_postdec(left));
            continue;
        }
        if (Parser_at_op(p, "?")) {
            Parser_advance(p);
            (left  =  expr_unary(UN_TRY, left, (left-> line ), (left-> column )));
            continue;
        }
        if (Parser_at_kw(p, "as")) {
            Parser_advance(p);
            Type*   target = parse_type(p);
            (left  =  expr_cast(left, target));
            continue;
        }
        int   info = peek_binop_bp(p);
        int   lbp = (info  /  100);
        int   op = (info  -  (lbp  *  100));
        if ((lbp  ==  0)) {
            break;
        }
        if ((lbp  <  min_bp)) {
            break;
        }
        Parser_advance(p);
        int   rbp = (lbp  +  1);
        Expr*   right = parse_expr(p, rbp);
        if ((right  ==  NULL)) {
            break;
        }
        (left  =  expr_binary(op, left, right));
    }
    return left;
}

int   peek_binop_bp (Parser*   p) {
    if ((((p-> current ). kind )  !=  TOK_OP)) {
        return 0;
    }
    const char*   lex = ((p-> current ). lexeme );
    if (__glide_string_eq(lex, "||")) {
        return ((100  *  2)  +  OP_OR);
    }
    if (__glide_string_eq(lex, "&&")) {
        return ((100  *  3)  +  OP_AND);
    }
    if (__glide_string_eq(lex, "|")) {
        return ((100  *  4)  +  OP_BIT_OR);
    }
    if (__glide_string_eq(lex, "^")) {
        return ((100  *  5)  +  OP_BIT_XOR);
    }
    if (__glide_string_eq(lex, "&")) {
        return ((100  *  6)  +  OP_BIT_AND);
    }
    if (__glide_string_eq(lex, "==")) {
        return ((100  *  7)  +  OP_EQ);
    }
    if (__glide_string_eq(lex, "!=")) {
        return ((100  *  7)  +  OP_NE);
    }
    if (__glide_string_eq(lex, "<")) {
        return ((100  *  8)  +  OP_LT);
    }
    if (__glide_string_eq(lex, "<=")) {
        return ((100  *  8)  +  OP_LE);
    }
    if (__glide_string_eq(lex, ">")) {
        return ((100  *  8)  +  OP_GT);
    }
    if (__glide_string_eq(lex, ">=")) {
        return ((100  *  8)  +  OP_GE);
    }
    if (__glide_string_eq(lex, "<<")) {
        return ((100  *  9)  +  OP_SHL);
    }
    if (__glide_string_eq(lex, ">>")) {
        return ((100  *  9)  +  OP_SHR);
    }
    if (__glide_string_eq(lex, "+")) {
        return ((100  *  10)  +  OP_ADD);
    }
    if (__glide_string_eq(lex, "-")) {
        return ((100  *  10)  +  OP_SUB);
    }
    if (__glide_string_eq(lex, "*")) {
        return ((100  *  11)  +  OP_MUL);
    }
    if (__glide_string_eq(lex, "/")) {
        return ((100  *  11)  +  OP_DIV);
    }
    if (__glide_string_eq(lex, "%")) {
        return ((100  *  11)  +  OP_MOD);
    }
    return 0;
}

Expr*   parse_prefix (Parser*   p) {
    Token   tok = (p-> current );
    int   line = (tok. line );
    int   col = (tok. column );
    if (((tok. kind )  ==  TOK_INT)) {
        Parser_advance(p);
        return expr_int(parse_int_lexeme((tok. lexeme )), line, col);
    }
    if (((tok. kind )  ==  TOK_STRING)) {
        Parser_advance(p);
        int   len = __glide_string_len((tok. lexeme ));
        const char*   inner = __glide_string_substring((tok. lexeme ), 1, (len  -  1));
        return expr_string(inner, line, col);
    }
    if (((tok. kind )  ==  TOK_CHAR)) {
        Parser_advance(p);
        int   len = __glide_string_len((tok. lexeme ));
        const char*   inner = __glide_string_substring((tok. lexeme ), 1, (len  -  1));
        char   c = decode_char_inner(inner);
        return expr_char(c, line, col);
    }
    if (((tok. kind )  ==  TOK_KEYWORD)) {
        if (__glide_string_eq((tok. lexeme ), "true")) {
            Parser_advance(p);
            return expr_bool(true, line, col);
        }
        if (__glide_string_eq((tok. lexeme ), "false")) {
            Parser_advance(p);
            return expr_bool(false, line, col);
        }
        if (__glide_string_eq((tok. lexeme ), "null")) {
            Parser_advance(p);
            return expr_null(line, col);
        }
        if (__glide_string_eq((tok. lexeme ), "fn")) {
            Parser_advance(p);
            Parser_expect_op(p, "(");
            Vector__Param*   params = Vector_new__Param();
            while (((!Parser_at_op(p, ")"))  &&  (!Parser_at_eof(p)))) {
                const char*   pname = Parser_expect_ident(p);
                Parser_expect_op(p, ":");
                Type*   pty = parse_type(p);
                Vector_push__Param(params, (( Param ){. name  = pname, . ty  = pty}));
                if ((!Parser_eat_op(p, ","))) {
                    break;
                }
            }
            Parser_expect_op(p, ")");
            Type*   ret = NULL;
            if (Parser_eat_op(p, "->")) {
                (ret  =  parse_type(p));
            }
            Vector__Stmt*   body = parse_block(p);
            return expr_fnexpr(params, ret, body, line, col);
        }
        if (__glide_string_eq((tok. lexeme ), "move")) {
            Parser_advance(p);
            if (Parser_at_kw(p, "fn")) {
                Parser_advance(p);
                Parser_expect_op(p, "(");
                Vector__Param*   params = Vector_new__Param();
                while (((!Parser_at_op(p, ")"))  &&  (!Parser_at_eof(p)))) {
                    const char*   pname = Parser_expect_ident(p);
                    Parser_expect_op(p, ":");
                    Type*   pty = parse_type(p);
                    Vector_push__Param(params, (( Param ){. name  = pname, . ty  = pty}));
                    if ((!Parser_eat_op(p, ","))) {
                        break;
                    }
                }
                Parser_expect_op(p, ")");
                Type*   ret = NULL;
                if (Parser_eat_op(p, "->")) {
                    (ret  =  parse_type(p));
                }
                Vector__Stmt*   body = parse_block(p);
                return expr_fnexpr(params, ret, body, line, col);
            }
        }
        if (__glide_string_eq((tok. lexeme ), "sizeof")) {
            Parser_advance(p);
            Parser_expect_op(p, "(");
            Type*   t = parse_type(p);
            Parser_expect_op(p, ")");
            Expr*   e = (( Expr* )calloc(1, sizeof( Expr )));
            ((e-> kind )  =  EX_SIZEOF);
            ((e-> line )  =  line);
            ((e-> column )  =  col);
            ((e-> cast_to )  =  t);
            return e;
        }
        if (__glide_string_eq((tok. lexeme ), "new")) {
            Parser_advance(p);
            const char*   tn = Parser_expect_ident(p);
            if (Parser_at_op(p, "<")) {
                skip_type_params(p);
            }
            Parser_expect_op(p, "{");
            Vector__Expr*   args = Vector_new__Expr();
            while (((!Parser_at_op(p, "}"))  &&  (!Parser_at_eof(p)))) {
                const char*   _name = Parser_expect_ident(p);
                Parser_expect_op(p, ":");
                Expr*   v = parse_expr(p, 0);
                if ((v  !=  NULL)) {
                    Vector_push__Expr(args, (*v));
                }
                if ((!Parser_eat_op(p, ","))) {
                    break;
                }
            }
            Parser_expect_op(p, "}");
            return expr_macro(__glide_string_concat(tn, "__new"), args, line, col);
        }
    }
    if (((tok. kind )  ==  TOK_IDENT)) {
        Parser_advance(p);
        if (Parser_at_op(p, "::")) {
            Parser_advance(p);
            const char*   member = Parser_expect_ident(p);
            return expr_path((tok. lexeme ), member, line, col);
        }
        if ((Parser_at_op(p, "!")  &&  Parser_peek_op(p, "("))) {
            Parser_advance(p);
            Parser_advance(p);
            Vector__Expr*   args = Vector_new__Expr();
            while (((!Parser_at_op(p, ")"))  &&  (!Parser_at_eof(p)))) {
                Expr*   a = parse_expr(p, 0);
                if ((a  !=  NULL)) {
                    Vector_push__Expr(args, (*a));
                }
                if ((!Parser_eat_op(p, ","))) {
                    break;
                }
            }
            Parser_expect_op(p, ")");
            return expr_macro((tok. lexeme ), args, line, col);
        }
        if (((Parser_at_op(p, "{")  &&  starts_uppercase((tok. lexeme )))  &&  (!(p-> no_struct_lit )))) {
            Parser_advance(p);
            Vector__string*   names = Vector_new__string();
            Vector__Expr*   values = Vector_new__Expr();
            while (((!Parser_at_op(p, "}"))  &&  (!Parser_at_eof(p)))) {
                const char*   fname = Parser_expect_ident(p);
                Parser_expect_op(p, ":");
                Expr*   v = parse_expr(p, 0);
                if ((v  !=  NULL)) {
                    Vector_push__string(names, fname);
                    Vector_push__Expr(values, (*v));
                }
                if ((!Parser_eat_op(p, ","))) {
                    break;
                }
            }
            Parser_expect_op(p, "}");
            return expr_struct_lit((tok. lexeme ), names, values, line, col);
        }
        return expr_ident((tok. lexeme ), line, col);
    }
    if (Parser_at_op(p, "(")) {
        Parser_advance(p);
        Expr*   e = parse_expr(p, 0);
        Parser_expect_op(p, ")");
        return e;
    }
    if (Parser_at_op(p, "-")) {
        Parser_advance(p);
        Expr*   r = parse_expr(p, 13);
        return expr_unary(UN_NEG, r, line, col);
    }
    if (Parser_at_op(p, "!")) {
        Parser_advance(p);
        Expr*   r = parse_expr(p, 13);
        return expr_unary(UN_NOT, r, line, col);
    }
    if (Parser_at_op(p, "~")) {
        Parser_advance(p);
        Expr*   r = parse_expr(p, 13);
        return expr_unary(UN_BIT_NOT, r, line, col);
    }
    if (Parser_at_op(p, "*")) {
        Parser_advance(p);
        Expr*   r = parse_expr(p, 13);
        return expr_unary(UN_DEREF, r, line, col);
    }
    if (Parser_at_op(p, "&")) {
        Parser_advance(p);
        if (Parser_eat_kw(p, "mut")) {
            Expr*   r = parse_expr(p, 13);
            return expr_unary(UN_ADDR_MUT, r, line, col);
        }
        Expr*   r = parse_expr(p, 13);
        return expr_unary(UN_ADDR, r, line, col);
    }
    Parser_err(p, "unexpected token at start of expression");
    Parser_advance(p);
    return NULL;
}

int   parse_int_lexeme (const char*   s) {
    int   n = __glide_string_len(s);
    int   v = 0;
    for (int   i = 0; (i  <  n); i++) {
        char   c = __glide_string_at(s, i);
        int   ci = __glide_char_to_int(c);
        if ((ci  ==  95)) {
            continue;
        }
        (v  =  ((v  *  10)  +  (ci  -  48)));
    }
    return v;
}

bool   starts_uppercase (const char*   s) {
    if ((__glide_string_len(s)  ==  0)) {
        return false;
    }
    char   c = __glide_string_at(s, 0);
    int   ci = __glide_char_to_int(c);
    return ((ci  >=  65)  &&  (ci  <=  90));
}

char   decode_char_inner (const char*   s) {
    if ((__glide_string_len(s)  ==  0)) {
        return (char)0;
    }
    char   first = __glide_string_at(s, 0);
    if ((__glide_char_to_int(first)  !=  92)) {
        return first;
    }
    if ((__glide_string_len(s)  <  2)) {
        return (char)0;
    }
    char   esc = __glide_string_at(s, 1);
    int   ei = __glide_char_to_int(esc);
    if ((ei  ==  110)) {
        return (char)10;
    }
    if ((ei  ==  116)) {
        return (char)9;
    }
    if ((ei  ==  114)) {
        return (char)13;
    }
    if ((ei  ==  48)) {
        return (char)0;
    }
    if ((ei  ==  92)) {
        return (char)92;
    }
    if ((ei  ==  39)) {
        return (char)39;
    }
    if ((ei  ==  34)) {
        return (char)34;
    }
    return esc;
}

bool   type_eq (Type*   a, Type*   b) {
    if ((a  ==  NULL)) {
        return (b  ==  NULL);
    }
    if ((b  ==  NULL)) {
        return false;
    }
    if (((a-> kind )  !=  (b-> kind ))) {
        return false;
    }
    if (((a-> kind )  ==  TY_NAMED)) {
        return __glide_string_eq((a-> name ), (b-> name ));
    }
    return type_eq((a-> inner ), (b-> inner ));
}

const char*   type_to_string (Type*   t) {
    if ((t  ==  NULL)) {
        return "<none>";
    }
    if (((t-> kind )  ==  TY_NAMED)) {
        return (t-> name );
    }
    if (((t-> kind )  ==  TY_POINTER)) {
        return __glide_string_concat("*", type_to_string((t-> inner )));
    }
    if (((t-> kind )  ==  TY_BORROW)) {
        return __glide_string_concat("&", type_to_string((t-> inner )));
    }
    if (((t-> kind )  ==  TY_BORROW_MUT)) {
        return __glide_string_concat("&mut ", type_to_string((t-> inner )));
    }
    if (((t-> kind )  ==  TY_SLICE)) {
        return __glide_string_concat("[]", type_to_string((t-> inner )));
    }
    return "<ty?>";
}

bool   is_int_type (Type*   t) {
    if ((t  ==  NULL)) {
        return false;
    }
    if (((t-> kind )  !=  TY_NAMED)) {
        return false;
    }
    const char*   n = (t-> name );
    if (__glide_string_eq(n, "int")) {
        return true;
    }
    if (__glide_string_eq(n, "uint")) {
        return true;
    }
    if (__glide_string_eq(n, "long")) {
        return true;
    }
    if (__glide_string_eq(n, "ulong")) {
        return true;
    }
    if (__glide_string_eq(n, "i8")) {
        return true;
    }
    if (__glide_string_eq(n, "i16")) {
        return true;
    }
    if (__glide_string_eq(n, "i32")) {
        return true;
    }
    if (__glide_string_eq(n, "i64")) {
        return true;
    }
    if (__glide_string_eq(n, "u8")) {
        return true;
    }
    if (__glide_string_eq(n, "u16")) {
        return true;
    }
    if (__glide_string_eq(n, "u32")) {
        return true;
    }
    if (__glide_string_eq(n, "u64")) {
        return true;
    }
    if (__glide_string_eq(n, "usize")) {
        return true;
    }
    if (__glide_string_eq(n, "isize")) {
        return true;
    }
    if (__glide_string_eq(n, "char")) {
        return true;
    }
    return false;
}

bool   is_float_type (Type*   t) {
    if ((t  ==  NULL)) {
        return false;
    }
    if (((t-> kind )  !=  TY_NAMED)) {
        return false;
    }
    return ((__glide_string_eq((t-> name ), "float")  ||  __glide_string_eq((t-> name ), "f32"))  ||  __glide_string_eq((t-> name ), "f64"));
}

bool   is_bool_type (Type*   t) {
    if ((t  ==  NULL)) {
        return false;
    }
    return (((t-> kind )  ==  TY_NAMED)  &&  __glide_string_eq((t-> name ), "bool"));
}

bool   types_compat (Type*   want, Type*   got) {
    if (((want  ==  NULL)  ||  (got  ==  NULL))) {
        return true;
    }
    if (type_eq(want, got)) {
        return true;
    }
    if ((is_int_type(want)  &&  is_int_type(got))) {
        return true;
    }
    if ((is_float_type(want)  &&  is_float_type(got))) {
        return true;
    }
    if ((((want-> kind )  ==  TY_POINTER)  &&  ((got-> kind )  ==  TY_POINTER))) {
        if (((((want-> inner )  !=  NULL)  &&  (((want-> inner )-> kind )  ==  TY_NAMED))  &&  __glide_string_eq(((want-> inner )-> name ), "void"))) {
            return true;
        }
        if (((((got-> inner )  !=  NULL)  &&  (((got-> inner )-> kind )  ==  TY_NAMED))  &&  __glide_string_eq(((got-> inner )-> name ), "void"))) {
            return true;
        }
    }
    if ((((((want-> kind )  ==  TY_POINTER)  ||  ((want-> kind )  ==  TY_BORROW))  ||  ((want-> kind )  ==  TY_BORROW_MUT))  &&  ((((got-> kind )  ==  TY_POINTER)  ||  ((got-> kind )  ==  TY_BORROW))  ||  ((got-> kind )  ==  TY_BORROW_MUT)))) {
        return types_compat((want-> inner ), (got-> inner ));
    }
    return false;
}

Typer*   Typer_new (void) {
    Typer*   t = (( Typer* )malloc(sizeof( Typer )));
    HashMap__FnSig*   fns = HashMap_new__FnSig();
    HashMap__bool*   structs = HashMap_new__bool();
    HashMap__Type*   scope = HashMap_new__Type();
    HashMap__Type*   mscope = HashMap_new__Type();
    HashMap__bool*   owned = HashMap_new__bool();
    Vector__BorrowEvent*   bw = Vector_new__BorrowEvent();
    Vector__DiagEntry*   dg = Vector_new__DiagEntry();
    ((t-> fns )  =  fns);
    ((t-> structs )  =  structs);
    ((t-> scope )  =  scope);
    ((t-> module_scope )  =  mscope);
    ((t-> owned_locals )  =  owned);
    ((t-> borrows )  =  bw);
    ((t-> diagnostics )  =  dg);
    ((t-> current_ret )  =  NULL);
    ((t-> current_origin )  =  "");
    ((t-> error_count )  =  0);
    return t;
}

void   Typer_free (Typer*   self) {
    HashMap_free__FnSig((self-> fns ));
    HashMap_free__bool((self-> structs ));
    HashMap_free__Type((self-> scope ));
    HashMap_free__Type((self-> module_scope ));
    HashMap_free__bool((self-> owned_locals ));
    Vector_free__BorrowEvent((self-> borrows ));
    Vector_free__DiagEntry((self-> diagnostics ));
    free((( void* )self));
}

void   Typer_push_diag (Typer*   self, int   line, int   col, int   severity, const char*   code, const char*   msg) {
    const char*   origin = (self-> current_origin );
    if ((origin  ==  NULL)) {
        (origin  =  "");
    }
    DiagEntry   e = (( DiagEntry ){. line  = line, . col  = col, . end_line  = line, . end_col  = (col  +  1), . severity  = severity, . code  = code, . message  = msg, . origin  = origin});
    Vector_push__DiagEntry((self-> diagnostics ), e);
}

void   Typer_err (Typer*   self, int   line, int   col, const char*   msg) {
    Typer_push_diag(self, line, col, 1, "", msg);
    ((self-> error_count )  =  ((self-> error_count )  +  1));
}

void   Typer_err_code (Typer*   self, int   line, int   col, const char*   code, const char*   msg) {
    Typer_push_diag(self, line, col, 1, code, msg);
    ((self-> error_count )  =  ((self-> error_count )  +  1));
}

void   Typer_warn (Typer*   self, int   line, int   col, const char*   code, const char*   msg) {
    Typer_push_diag(self, line, col, 2, code, msg);
}

void   pre_register (Typer*   t, Vector__Stmt*   program) {
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if (((s. kind )  ==  ST_CONST)) {
            Type*   ty = (s. let_ty );
            if (((ty  ==  NULL)  &&  ((s. let_value )  !=  NULL))) {
                if ((((s. let_value )-> kind )  ==  EX_INT)) {
                    (ty  =  ty_named("int"));
                } else {
                    if ((((s. let_value )-> kind )  ==  EX_FLOAT)) {
                        (ty  =  ty_named("float"));
                    } else {
                        if ((((s. let_value )-> kind )  ==  EX_STRING)) {
                            (ty  =  ty_named("string"));
                        } else {
                            if ((((s. let_value )-> kind )  ==  EX_BOOL)) {
                                (ty  =  ty_named("bool"));
                            } else {
                                if ((((s. let_value )-> kind )  ==  EX_CHAR)) {
                                    (ty  =  ty_named("char"));
                                }
                            }
                        }
                    }
                }
            }
            if ((ty  !=  NULL)) {
                HashMap_insert__Type((t-> module_scope ), (s. name ), (*ty));
            }
        }
        if (((s. kind )  ==  ST_FN)) {
            FnSig   sig = (( FnSig ){. name  = (s. name ), . params  = (s. fn_params ), . ret_type  = (s. fn_ret_ty )});
            HashMap_insert__FnSig((t-> fns ), (s. name ), sig);
        }
        if (((s. kind )  ==  ST_STRUCT)) {
            HashMap_insert__bool((t-> structs ), (s. name ), true);
            if (((s. struct_fields )  !=  NULL)) {
                for (int   j = 0; (j  <  Vector_len__Field((s. struct_fields ))); j++) {
                    Field   f = Vector_get__Field((s. struct_fields ), j);
                    if ((((f. ty )  !=  NULL)  &&  ((((f. ty )-> kind )  ==  TY_BORROW)  ||  (((f. ty )-> kind )  ==  TY_BORROW_MUT)))) {
                        Typer_err_code(t, (s. line ), (s. column ), "borrow-in-field", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("borrow `", type_to_string((f. ty ))), "` not allowed in struct field `"), (f. name )), "` (use `*T` instead)"));
                    }
                }
            }
        }
    }
}

int   count_borrows (Typer*   t, const char*   name, bool   want_mut) {
    int   n = Vector_len__BorrowEvent((t-> borrows ));
    int   c = 0;
    for (int   i = 0; (i  <  n); i++) {
        BorrowEvent   ev = Vector_get__BorrowEvent((t-> borrows ), i);
        if ((__glide_string_eq((ev. source ), name)  &&  ((ev. is_mut )  ==  want_mut))) {
            (c  =  (c  +  1));
        }
    }
    return c;
}

void   record_borrow (Typer*   t, const char*   source, bool   is_mut, int   line, int   col) {
    int   n_shared = count_borrows(t, source, false);
    int   n_mut = count_borrows(t, source, true);
    if (is_mut) {
        if (((n_mut  >  0)  ||  (n_shared  >  0))) {
            Typer_err_code(t, line, col, "overlap-borrow", __glide_string_concat(__glide_string_concat("cannot borrow `", source), "` as mutable: already borrowed (across statements)"));
        }
    } else {
        if ((n_mut  >  0)) {
            Typer_err_code(t, line, col, "overlap-borrow", __glide_string_concat(__glide_string_concat("cannot borrow `", source), "` as immutable: already mutably borrowed"));
        }
    }
    BorrowEvent   ev = (( BorrowEvent ){. source  = source, . is_mut  = is_mut});
    Vector_push__BorrowEvent((t-> borrows ), ev);
}

int   enter_borrow_scope (Typer*   t) {
    return Vector_len__BorrowEvent((t-> borrows ));
}

void   exit_borrow_scope (Typer*   t, int   saved) {
    while ((Vector_len__BorrowEvent((t-> borrows ))  >  saved)) {
        BorrowEvent   _ev = Vector_pop__BorrowEvent((t-> borrows ));
    }
}

void   check_call_aliasing (Typer*   t, Expr*   call) {
    if ((((call  ==  NULL)  ||  ((call-> kind )  !=  EX_CALL))  ||  ((call-> args )  ==  NULL))) {
        return;
    }
    Vector__string*   names = Vector_new__string();
    Vector__bool*   muts = Vector_new__bool();
    for (int   i = 0; (i  <  Vector_len__Expr((call-> args ))); i++) {
        Expr   a = Vector_get__Expr((call-> args ), i);
        if ((((((a. kind )  ==  EX_UNARY)  &&  (((a. op_code )  ==  UN_ADDR)  ||  ((a. op_code )  ==  UN_ADDR_MUT)))  &&  ((a. operand )  !=  NULL))  &&  (((a. operand )-> kind )  ==  EX_IDENT))) {
            Vector_push__string(names, ((a. operand )-> str_val ));
            Vector_push__bool(muts, ((a. op_code )  ==  UN_ADDR_MUT));
        }
    }
    int   n = Vector_len__string(names);
    for (int   i = 0; (i  <  n); i++) {
        for (int   j = (i  +  1); (j  <  n); j++) {
            if (__glide_string_eq(Vector_get__string(names, i), Vector_get__string(names, j))) {
                bool   im = Vector_get__bool(muts, i);
                bool   jm = Vector_get__bool(muts, j);
                if ((im  ||  jm)) {
                    Typer_err_code(t, (call-> line ), (call-> column ), "borrow-alias-in-call", __glide_string_concat(__glide_string_concat("cannot pass `", Vector_get__string(names, i)), "` aliased through both shared and mutable borrow in same call"));
                }
            }
        }
    }
}

void   check_program (Typer*   t, Vector__Stmt*   program) {
    pre_register(t, program);
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        ((t-> current_origin )  =  (s. origin ));
        check_top(t, (&s));
    }
}

void   check_top (Typer*   t, Stmt*   s) {
    if (((s-> kind )  ==  ST_FN)) {
        check_fn(t, s);
        return;
    }
    if (((s-> kind )  ==  ST_STRUCT)) {
        return;
    }
    if (((s-> kind )  ==  ST_IMPL)) {
        return;
    }
    if (((s-> kind )  ==  ST_CONST)) {
        check_let_or_const(t, s);
        return;
    }
    if (((s-> kind )  ==  ST_LET)) {
        check_let_or_const(t, s);
        return;
    }
    if (((s-> kind )  ==  ST_IMPORT)) {
        return;
    }
}

void   check_fn (Typer*   t, Stmt*   s) {
    HashMap__Type*   saved_scope = (t-> scope );
    HashMap__bool*   saved_owned = (t-> owned_locals );
    Vector__BorrowEvent*   saved_borrows = (t-> borrows );
    Type*   saved_ret = (t-> current_ret );
    HashMap__Type*   new_scope = HashMap_new__Type();
    HashMap__bool*   new_owned = HashMap_new__bool();
    Vector__BorrowEvent*   new_borrows = Vector_new__BorrowEvent();
    ((t-> scope )  =  new_scope);
    ((t-> owned_locals )  =  new_owned);
    ((t-> borrows )  =  new_borrows);
    ((t-> current_ret )  =  (s-> fn_ret_ty ));
    if (((s-> fn_params )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Param((s-> fn_params ))); i++) {
            Param   p = Vector_get__Param((s-> fn_params ), i);
            if (((p. ty )  !=  NULL)) {
                HashMap_insert__Type((t-> scope ), (p. name ), (*(p. ty )));
            }
        }
    }
    if (((s-> fn_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> fn_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> fn_body ), i);
            check_stmt(t, (&b));
        }
    }
    HashMap_free__Type((t-> scope ));
    HashMap_free__bool((t-> owned_locals ));
    Vector_free__BorrowEvent((t-> borrows ));
    ((t-> scope )  =  saved_scope);
    ((t-> owned_locals )  =  saved_owned);
    ((t-> borrows )  =  saved_borrows);
    ((t-> current_ret )  =  saved_ret);
}

void   check_stmt (Typer*   t, Stmt*   s) {
    if ((((s-> kind )  ==  ST_LET)  ||  ((s-> kind )  ==  ST_CONST))) {
        check_let_or_const(t, s);
        return;
    }
    if (((s-> kind )  ==  ST_RETURN)) {
        check_return(t, s);
        return;
    }
    if (((s-> kind )  ==  ST_EXPR)) {
        if (((s-> expr_value )  !=  NULL)) {
            infer_expr(t, (s-> expr_value ));
        }
        return;
    }
    if (((s-> kind )  ==  ST_IF)) {
        if (((s-> cond )  !=  NULL)) {
            Type*   ct = infer_expr(t, (s-> cond ));
            if (((ct  !=  NULL)  &&  (!is_bool_type(ct)))) {
                Typer_err(t, (s-> line ), (s-> column ), "if condition must be bool");
            }
        }
        if (((s-> then_body )  !=  NULL)) {
            int   saved = enter_borrow_scope(t);
            for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
                Stmt   b = Vector_get__Stmt((s-> then_body ), i);
                check_stmt(t, (&b));
            }
            exit_borrow_scope(t, saved);
        }
        if (((s-> else_body )  !=  NULL)) {
            int   saved = enter_borrow_scope(t);
            for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
                Stmt   b = Vector_get__Stmt((s-> else_body ), i);
                check_stmt(t, (&b));
            }
            exit_borrow_scope(t, saved);
        }
        return;
    }
    if (((s-> kind )  ==  ST_WHILE)) {
        if (((s-> cond )  !=  NULL)) {
            Type*   ct = infer_expr(t, (s-> cond ));
            if (((ct  !=  NULL)  &&  (!is_bool_type(ct)))) {
                Typer_err(t, (s-> line ), (s-> column ), "while condition must be bool");
            }
        }
        if (((s-> then_body )  !=  NULL)) {
            int   saved = enter_borrow_scope(t);
            for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
                Stmt   b = Vector_get__Stmt((s-> then_body ), i);
                check_stmt(t, (&b));
            }
            exit_borrow_scope(t, saved);
        }
        return;
    }
    if (((s-> kind )  ==  ST_FOR)) {
        int   saved = enter_borrow_scope(t);
        if (((s-> for_init )  !=  NULL)) {
            check_stmt(t, (s-> for_init ));
        }
        if (((s-> cond )  !=  NULL)) {
            Type*   _u0 = infer_expr(t, (s-> cond ));
        }
        if (((s-> for_step )  !=  NULL)) {
            Type*   _u1 = infer_expr(t, (s-> for_step ));
        }
        if (((s-> then_body )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
                Stmt   b = Vector_get__Stmt((s-> then_body ), i);
                check_stmt(t, (&b));
            }
        }
        exit_borrow_scope(t, saved);
        return;
    }
    if (((s-> kind )  ==  ST_BLOCK)) {
        if (((s-> then_body )  !=  NULL)) {
            int   saved = enter_borrow_scope(t);
            for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
                Stmt   b = Vector_get__Stmt((s-> then_body ), i);
                check_stmt(t, (&b));
            }
            exit_borrow_scope(t, saved);
        }
        return;
    }
    if (((s-> kind )  ==  ST_BREAK)) {
        return;
    }
    if (((s-> kind )  ==  ST_CONTINUE)) {
        return;
    }
}

void   check_let_or_const (Typer*   t, Stmt*   s) {
    Type*   final_ty = (s-> let_ty );
    if ((((s-> let_ty )  !=  NULL)  &&  ((((s-> let_ty )-> kind )  ==  TY_BORROW)  ||  (((s-> let_ty )-> kind )  ==  TY_BORROW_MUT)))) {
        if ((((s-> let_value )  !=  NULL)  &&  (((s-> let_value )-> kind )  ==  EX_NULL))) {
            Typer_err_code(t, (s-> line ), (s-> column ), "null-borrow", __glide_string_concat(__glide_string_concat("borrow `", type_to_string((s-> let_ty ))), "` cannot be null"));
        }
    }
    if ((((((s-> kind )  ==  ST_LET)  &&  (s-> is_auto_owned ))  &&  ((s-> let_value )  !=  NULL))  &&  (((s-> let_value )-> kind )  ==  EX_NULL))) {
        Typer_err_code(t, (s-> line ), (s-> column ), "null-auto-drop", __glide_string_concat(__glide_string_concat("auto-drop binding `", (s-> name )), "` cannot be initialized with null"));
    }
    if ((((s-> let_value )  !=  NULL)  &&  (((s-> let_value )-> kind )  ==  EX_IDENT))) {
        const char*   src = ((s-> let_value )-> str_val );
        if (HashMap_contains__bool((t-> owned_locals ), src)) {
            Typer_err_code(t, (s-> line ), (s-> column ), "owned-move", __glide_string_concat(__glide_string_concat("cannot move owned value `", src), "` into another binding (auto-drop conflict)"));
        }
    }
    bool   is_auto_owned = false;
    if ((((s-> kind )  ==  ST_LET)  &&  (s-> is_auto_owned ))) {
        (is_auto_owned  =  true);
    }
    if ((((((((((s-> kind )  ==  ST_LET)  &&  (final_ty  !=  NULL))  &&  ((final_ty-> kind )  ==  TY_POINTER))  &&  ((s-> let_value )  !=  NULL))  &&  (((s-> let_value )-> kind )  ==  EX_STRUCT_LIT))  &&  ((final_ty-> inner )  !=  NULL))  &&  (((final_ty-> inner )-> kind )  ==  TY_NAMED))  &&  __glide_string_eq(((final_ty-> inner )-> name ), ((s-> let_value )-> str_val )))) {
        (is_auto_owned  =  true);
    }
    if (((s-> let_value )  !=  NULL)) {
        Type*   val_ty = infer_expr(t, (s-> let_value ));
        if ((final_ty  !=  NULL)) {
            if ((!types_compat(final_ty, val_ty))) {
                Typer_err(t, (s-> line ), (s-> column ), __glide_string_concat(__glide_string_concat(__glide_string_concat("let initializer type mismatch: expected ", type_to_string(final_ty)), ", got "), type_to_string(val_ty)));
            }
        } else {
            (final_ty  =  val_ty);
        }
    }
    if ((final_ty  !=  NULL)) {
        HashMap_insert__Type((t-> scope ), (s-> name ), (*final_ty));
        if (is_auto_owned) {
            HashMap_insert__bool((t-> owned_locals ), (s-> name ), true);
        }
    }
    if ((((((((s-> kind )  ==  ST_LET)  &&  ((s-> let_value )  !=  NULL))  &&  (((s-> let_value )-> kind )  ==  EX_UNARY))  &&  ((((s-> let_value )-> op_code )  ==  UN_ADDR)  ||  (((s-> let_value )-> op_code )  ==  UN_ADDR_MUT)))  &&  (((s-> let_value )-> operand )  !=  NULL))  &&  ((((s-> let_value )-> operand )-> kind )  ==  EX_IDENT))) {
        const char*   src = (((s-> let_value )-> operand )-> str_val );
        bool   is_mut = (((s-> let_value )-> op_code )  ==  UN_ADDR_MUT);
        record_borrow(t, src, is_mut, (s-> line ), (s-> column ));
    }
}

void   check_return (Typer*   t, Stmt*   s) {
    if (((s-> expr_value )  ==  NULL)) {
        if (((t-> current_ret )  !=  NULL)) {
            Typer_err(t, (s-> line ), (s-> column ), "missing return value");
        }
        return;
    }
    Expr*   e = (s-> expr_value );
    if ((((e-> kind )  ==  EX_IDENT)  &&  HashMap_contains__bool((t-> owned_locals ), (e-> str_val )))) {
        Typer_err(t, (s-> line ), (s-> column ), __glide_string_concat(__glide_string_concat("cannot return owned value `", (e-> str_val )), "` (auto-drop would free it)"));
    }
    if ((((((e-> kind )  ==  EX_UNARY)  &&  (((e-> op_code )  ==  UN_ADDR)  ||  ((e-> op_code )  ==  UN_ADDR_MUT)))  &&  ((e-> operand )  !=  NULL))  &&  (((e-> operand )-> kind )  ==  EX_IDENT))) {
        const char*   n = ((e-> operand )-> str_val );
        bool   is_param_borrow = false;
        if (HashMap_contains__Type((t-> scope ), n)) {
            Type   st = HashMap_get__Type((t-> scope ), n);
            if ((((st. kind )  ==  TY_BORROW)  ||  ((st. kind )  ==  TY_BORROW_MUT))) {
                (is_param_borrow  =  true);
            }
        }
        if ((!is_param_borrow)) {
            Typer_err(t, (s-> line ), (s-> column ), __glide_string_concat(__glide_string_concat("cannot return borrow of local `", n), "` (would dangle after function returns)"));
        }
    }
    Type*   vt = infer_expr(t, e);
    if (((t-> current_ret )  ==  NULL)) {
        Typer_err(t, (s-> line ), (s-> column ), "return with value in fn declared without ret type");
        return;
    }
    if ((!types_compat((t-> current_ret ), vt))) {
        Typer_err(t, (s-> line ), (s-> column ), __glide_string_concat(__glide_string_concat(__glide_string_concat("return type mismatch: expected ", type_to_string((t-> current_ret ))), ", got "), type_to_string(vt)));
    }
}

Type*   infer_expr (Typer*   t, Expr*   e) {
    if ((e  ==  NULL)) {
        return NULL;
    }
    if (((e-> kind )  ==  EX_INT)) {
        return ty_named("int");
    }
    if (((e-> kind )  ==  EX_FLOAT)) {
        return ty_named("float");
    }
    if (((e-> kind )  ==  EX_STRING)) {
        return ty_named("string");
    }
    if (((e-> kind )  ==  EX_BOOL)) {
        return ty_named("bool");
    }
    if (((e-> kind )  ==  EX_CHAR)) {
        return ty_named("char");
    }
    if (((e-> kind )  ==  EX_NULL)) {
        return ty_pointer(ty_named("void"));
    }
    if (((e-> kind )  ==  EX_IDENT)) {
        if (HashMap_contains__Type((t-> scope ), (e-> str_val ))) {
            Type   ty = HashMap_get__Type((t-> scope ), (e-> str_val ));
            Type*   p = (( Type* )malloc(sizeof( Type )));
            ((*p)  =  ty);
            return p;
        }
        if (HashMap_contains__Type((t-> module_scope ), (e-> str_val ))) {
            Type   ty = HashMap_get__Type((t-> module_scope ), (e-> str_val ));
            Type*   p = (( Type* )malloc(sizeof( Type )));
            ((*p)  =  ty);
            return p;
        }
        if (HashMap_contains__FnSig((t-> fns ), (e-> str_val ))) {
            return ty_named("__fn__");
        }
        Typer_err(t, (e-> line ), (e-> column ), __glide_string_concat(__glide_string_concat("unknown name `", (e-> str_val )), "`"));
        return NULL;
    }
    if (((e-> kind )  ==  EX_BINARY)) {
        Type*   lt = infer_expr(t, (e-> lhs ));
        Type*   rt = infer_expr(t, (e-> rhs ));
        int   op = (e-> op_code );
        if (((((((op  ==  OP_EQ)  ||  (op  ==  OP_NE))  ||  (op  ==  OP_LT))  ||  (op  ==  OP_LE))  ||  (op  ==  OP_GT))  ||  (op  ==  OP_GE))) {
            return ty_named("bool");
        }
        if (((op  ==  OP_AND)  ||  (op  ==  OP_OR))) {
            return ty_named("bool");
        }
        if ((!types_compat(lt, rt))) {
            Typer_err(t, (e-> line ), (e-> column ), __glide_string_concat(__glide_string_concat(__glide_string_concat("binary op type mismatch: ", type_to_string(lt)), " vs "), type_to_string(rt)));
        }
        return lt;
    }
    if (((e-> kind )  ==  EX_UNARY)) {
        Type*   inner = infer_expr(t, (e-> operand ));
        if (((e-> op_code )  ==  UN_NOT)) {
            return ty_named("bool");
        }
        if (((e-> op_code )  ==  UN_DEREF)) {
            if (((inner  !=  NULL)  &&  ((inner-> kind )  ==  TY_POINTER))) {
                return (inner-> inner );
            }
            return inner;
        }
        if (((e-> op_code )  ==  UN_ADDR)) {
            return ty_pointer(inner);
        }
        if (((e-> op_code )  ==  UN_ADDR_MUT)) {
            return ty_pointer(inner);
        }
        return inner;
    }
    if (((e-> kind )  ==  EX_ASSIGN)) {
        Type*   lt = infer_expr(t, (e-> lhs ));
        Type*   rt = infer_expr(t, (e-> rhs ));
        if ((!types_compat(lt, rt))) {
            Typer_err(t, (e-> line ), (e-> column ), __glide_string_concat(__glide_string_concat(__glide_string_concat("assignment type mismatch: ", type_to_string(lt)), " = "), type_to_string(rt)));
        }
        return lt;
    }
    if (((e-> kind )  ==  EX_CALL)) {
        check_call_aliasing(t, e);
        if ((((e-> lhs )  !=  NULL)  &&  (((e-> lhs )-> kind )  ==  EX_IDENT))) {
            const char*   name = ((e-> lhs )-> str_val );
            if (HashMap_contains__FnSig((t-> fns ), name)) {
                FnSig   sig = HashMap_get__FnSig((t-> fns ), name);
                if ((((sig. params )  !=  NULL)  &&  ((e-> args )  !=  NULL))) {
                    if ((Vector_len__Param((sig. params ))  !=  Vector_len__Expr((e-> args )))) {
                        Typer_err(t, (e-> line ), (e-> column ), __glide_string_concat(__glide_string_concat("wrong arg count for `", name), "`"));
                    } else {
                        for (int   i = 0; (i  <  Vector_len__Param((sig. params ))); i++) {
                            Param   p = Vector_get__Param((sig. params ), i);
                            Expr   a = Vector_get__Expr((e-> args ), i);
                            if ((((((a. kind )  ==  EX_IDENT)  &&  HashMap_contains__bool((t-> owned_locals ), (a. str_val )))  &&  ((p. ty )  !=  NULL))  &&  (((p. ty )-> kind )  ==  TY_POINTER))) {
                                Typer_err(t, (a. line ), (a. column ), __glide_string_concat(__glide_string_concat("cannot move owned value `", (a. str_val )), "` into `*T` parameter (auto-drop conflict)"));
                            }
                            Type*   at = infer_expr(t, (&a));
                            if ((!types_compat((p. ty ), at))) {
                                Typer_err(t, (a. line ), (a. column ), __glide_string_concat(__glide_string_concat("arg ", (p. name )), " type mismatch"));
                            }
                        }
                    }
                }
                return (sig. ret_type );
            }
        }
        if (((e-> args )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
                Expr   a = Vector_get__Expr((e-> args ), i);
                Type*   _u2 = infer_expr(t, (&a));
            }
        }
        return NULL;
    }
    if (((e-> kind )  ==  EX_MEMBER)) {
        Type*   _u3 = infer_expr(t, (e-> lhs ));
        return NULL;
    }
    if (((e-> kind )  ==  EX_INDEX)) {
        Type*   _u4 = infer_expr(t, (e-> lhs ));
        Type*   _u5 = infer_expr(t, (e-> rhs ));
        return NULL;
    }
    if (((e-> kind )  ==  EX_CAST)) {
        Type*   _u6 = infer_expr(t, (e-> lhs ));
        return (e-> cast_to );
    }
    if (((e-> kind )  ==  EX_PATH)) {
        return NULL;
    }
    if (((e-> kind )  ==  EX_MACRO)) {
        if (((e-> args )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
                Expr   a = Vector_get__Expr((e-> args ), i);
                Type*   _u7 = infer_expr(t, (&a));
            }
        }
        return NULL;
    }
    if ((((e-> kind )  ==  EX_POSTINC)  ||  ((e-> kind )  ==  EX_POSTDEC))) {
        return infer_expr(t, (e-> lhs ));
    }
    if (((e-> kind )  ==  EX_STRUCT_LIT)) {
        return ty_named((e-> str_val ));
    }
    return NULL;
}

const char*   format_spec_for (Type*   t) {
    if ((t  ==  NULL)) {
        return "%p";
    }
    if (((t-> kind )  !=  TY_NAMED)) {
        return "%p";
    }
    const char*   n = (t-> name );
    if ((((__glide_string_eq(n, "int")  ||  __glide_string_eq(n, "i8"))  ||  __glide_string_eq(n, "i16"))  ||  __glide_string_eq(n, "i32"))) {
        return "%d";
    }
    if ((((__glide_string_eq(n, "uint")  ||  __glide_string_eq(n, "u8"))  ||  __glide_string_eq(n, "u16"))  ||  __glide_string_eq(n, "u32"))) {
        return "%u";
    }
    if (((__glide_string_eq(n, "i64")  ||  __glide_string_eq(n, "isize"))  ||  __glide_string_eq(n, "long"))) {
        return "%lld";
    }
    if (((__glide_string_eq(n, "u64")  ||  __glide_string_eq(n, "usize"))  ||  __glide_string_eq(n, "ulong"))) {
        return "%llu";
    }
    if (((__glide_string_eq(n, "float")  ||  __glide_string_eq(n, "f32"))  ||  __glide_string_eq(n, "f64"))) {
        return "%g";
    }
    if (__glide_string_eq(n, "char")) {
        return "%c";
    }
    if (__glide_string_eq(n, "string")) {
        return "%s";
    }
    if (__glide_string_eq(n, "bool")) {
        return "%s";
    }
    return "%p";
}

Type*   stdlib_method_ret (const char*   ty_name, const char*   method) {
    if (__glide_string_eq(ty_name, "string")) {
        if (__glide_string_eq(method, "len")) {
            return ty_named("int");
        }
        if (__glide_string_eq(method, "eq")) {
            return ty_named("bool");
        }
        if (__glide_string_eq(method, "at")) {
            return ty_named("char");
        }
        if (__glide_string_eq(method, "concat")) {
            return ty_named("string");
        }
        if (__glide_string_eq(method, "substring")) {
            return ty_named("string");
        }
    }
    if (__glide_string_eq(ty_name, "char")) {
        if (__glide_string_eq(method, "to_int")) {
            return ty_named("int");
        }
        if (__glide_string_eq(method, "is_digit")) {
            return ty_named("bool");
        }
        if (__glide_string_eq(method, "is_alpha")) {
            return ty_named("bool");
        }
    }
    if (__glide_string_eq(ty_name, "int")) {
        if (__glide_string_eq(method, "abs")) {
            return ty_named("int");
        }
        if (__glide_string_eq(method, "to_string")) {
            return ty_named("string");
        }
    }
    if (__glide_string_eq(ty_name, "bool")) {
        if (__glide_string_eq(method, "to_string")) {
            return ty_named("string");
        }
    }
    return NULL;
}

void   lift_anons_in_expr (CG*   g, Expr*   e) {
    if ((e  ==  NULL)) {
        return;
    }
    if (((e-> kind )  ==  EX_FNEXPR)) {
        int   id = (g-> anon_count );
        ((g-> anon_count )  =  (id  +  1));
        const char*   name = __glide_string_concat("__glide_anon_", int_to_str(id));
        AnonFn   af = (( AnonFn ){. name  = name, . params  = (e-> fn_expr_params ), . ret_ty  = (e-> cast_to ), . body  = (e-> fn_expr_body )});
        Vector_push__AnonFn((g-> anon_fns ), af);
        ((e-> kind )  =  EX_IDENT);
        ((e-> str_val )  =  name);
        return;
    }
    if (((e-> lhs )  !=  NULL)) {
        lift_anons_in_expr(g, (e-> lhs ));
    }
    if (((e-> rhs )  !=  NULL)) {
        lift_anons_in_expr(g, (e-> rhs ));
    }
    if (((e-> operand )  !=  NULL)) {
        lift_anons_in_expr(g, (e-> operand ));
    }
    if (((e-> args )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
            Expr   a = Vector_get__Expr((e-> args ), i);
            lift_anons_in_expr(g, (&a));
        }
    }
}

void   lift_anons_in_stmt (CG*   g, Stmt*   s) {
    if ((s  ==  NULL)) {
        return;
    }
    if (((s-> let_value )  !=  NULL)) {
        lift_anons_in_expr(g, (s-> let_value ));
    }
    if (((s-> expr_value )  !=  NULL)) {
        lift_anons_in_expr(g, (s-> expr_value ));
    }
    if (((s-> cond )  !=  NULL)) {
        lift_anons_in_expr(g, (s-> cond ));
    }
    if (((s-> for_init )  !=  NULL)) {
        lift_anons_in_stmt(g, (s-> for_init ));
    }
    if (((s-> for_step )  !=  NULL)) {
        lift_anons_in_expr(g, (s-> for_step ));
    }
    if (((s-> then_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> then_body ), i);
            lift_anons_in_stmt(g, (&b));
        }
    }
    if (((s-> else_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> else_body ), i);
            lift_anons_in_stmt(g, (&b));
        }
    }
    if (((s-> fn_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> fn_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> fn_body ), i);
            lift_anons_in_stmt(g, (&b));
        }
    }
    if (((s-> impl_methods )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> impl_methods ))); i++) {
            Stmt   m = Vector_get__Stmt((s-> impl_methods ), i);
            lift_anons_in_stmt(g, (&m));
        }
    }
}

void   collect_result_in_type (CG*   g, Type*   t) {
    if ((t  ==  NULL)) {
        return;
    }
    if ((((t-> kind )  ==  TY_RESULT)  &&  ((t-> inner )  !=  NULL))) {
        const char*   m = mangle_type((t-> inner ));
        for (int   i = 0; (i  <  Vector_len__Type((g-> result_types ))); i++) {
            Type   ex = Vector_get__Type((g-> result_types ), i);
            if (__glide_string_eq(mangle_type((&ex)), m)) {
                return;
            }
        }
        Vector_push__Type((g-> result_types ), (*(t-> inner )));
    }
    if (((t-> inner )  !=  NULL)) {
        collect_result_in_type(g, (t-> inner ));
    }
    if (((t-> fnptr_ret )  !=  NULL)) {
        collect_result_in_type(g, (t-> fnptr_ret ));
    }
    if (((t-> args )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Type((t-> args ))); i++) {
            Type   a = Vector_get__Type((t-> args ), i);
            collect_result_in_type(g, (&a));
        }
    }
}

void   collect_result_in_expr (CG*   g, Expr*   e) {
    if ((e  ==  NULL)) {
        return;
    }
    if (((e-> cast_to )  !=  NULL)) {
        collect_result_in_type(g, (e-> cast_to ));
    }
    if (((e-> lhs )  !=  NULL)) {
        collect_result_in_expr(g, (e-> lhs ));
    }
    if (((e-> rhs )  !=  NULL)) {
        collect_result_in_expr(g, (e-> rhs ));
    }
    if (((e-> operand )  !=  NULL)) {
        collect_result_in_expr(g, (e-> operand ));
    }
    if (((e-> args )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
            Expr   a = Vector_get__Expr((e-> args ), i);
            collect_result_in_expr(g, (&a));
        }
    }
}

void   collect_result_in_stmt (CG*   g, Stmt*   s) {
    if ((s  ==  NULL)) {
        return;
    }
    if (((s-> let_ty )  !=  NULL)) {
        collect_result_in_type(g, (s-> let_ty ));
    }
    if (((s-> fn_ret_ty )  !=  NULL)) {
        collect_result_in_type(g, (s-> fn_ret_ty ));
    }
    if (((s-> fn_params )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Param((s-> fn_params ))); i++) {
            Param   p = Vector_get__Param((s-> fn_params ), i);
            collect_result_in_type(g, (p. ty ));
        }
    }
    if (((s-> let_value )  !=  NULL)) {
        collect_result_in_expr(g, (s-> let_value ));
    }
    if (((s-> expr_value )  !=  NULL)) {
        collect_result_in_expr(g, (s-> expr_value ));
    }
    if (((s-> cond )  !=  NULL)) {
        collect_result_in_expr(g, (s-> cond ));
    }
    if (((s-> for_step )  !=  NULL)) {
        collect_result_in_expr(g, (s-> for_step ));
    }
    if (((s-> for_init )  !=  NULL)) {
        collect_result_in_stmt(g, (s-> for_init ));
    }
    if (((s-> then_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> then_body ), i);
            collect_result_in_stmt(g, (&b));
        }
    }
    if (((s-> else_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> else_body ), i);
            collect_result_in_stmt(g, (&b));
        }
    }
    if (((s-> fn_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> fn_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> fn_body ), i);
            collect_result_in_stmt(g, (&b));
        }
    }
    if (((s-> impl_methods )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> impl_methods ))); i++) {
            Stmt   mm = Vector_get__Stmt((s-> impl_methods ), i);
            collect_result_in_stmt(g, (&mm));
        }
    }
}

void   emit_result_runtime (CG*   g) {
    if ((Vector_len__Type((g-> result_types ))  ==  0)) {
        return;
    }
    for (int   i = 0; (i  <  Vector_len__Type((g-> result_types ))); i++) {
        Type   t = Vector_get__Type((g-> result_types ), i);
        const char*   m = mangle_type((&t));
        const char*   tc = type_to_c((&t));
        const char*   st = __glide_string_concat(__glide_string_concat("__glide_result_", m), "_t");
        printf("%s\n", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("typedef struct ", st), " { int ok; "), tc), " val; const char* err; } "), st), ";"));
        printf("%s\n", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("static ", st), " __glide_ok_"), m), "("), tc), " v) { "), st), " r; r.ok = 1; r.val = v; r.err = (const char*)0; return r; }"));
        printf("%s\n", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("static ", st), " __glide_err_"), m), "(const char* msg) { "), st), " r; r.ok = 0; r.err = msg; return r; }"));
        printf("%s\n", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("static ", tc), " __glide_unwrap_"), m), "("), st), " r) { "), tc), " z; if (r.ok) return r.val; memset(&z, 0, sizeof(z)); return z; }"));
    }
    printf("%s\n", "");
}

void   collect_chan_in_type (CG*   g, Type*   t) {
    if ((t  ==  NULL)) {
        return;
    }
    if ((((((t-> kind )  ==  TY_GENERIC)  &&  __glide_string_eq((t-> name ), "chan"))  &&  ((t-> args )  !=  NULL))  &&  (Vector_len__Type((t-> args ))  >  0))) {
        Type   inner = Vector_get__Type((t-> args ), 0);
        const char*   m = mangle_type((&inner));
        int   n = Vector_len__Type((g-> chan_types ));
        for (int   i = 0; (i  <  n); i++) {
            Type   existing = Vector_get__Type((g-> chan_types ), i);
            if (__glide_string_eq(mangle_type((&existing)), m)) {
                return;
            }
        }
        Vector_push__Type((g-> chan_types ), inner);
    }
    if (((t-> inner )  !=  NULL)) {
        collect_chan_in_type(g, (t-> inner ));
    }
    if (((t-> fnptr_ret )  !=  NULL)) {
        collect_chan_in_type(g, (t-> fnptr_ret ));
    }
    if (((t-> args )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Type((t-> args ))); i++) {
            Type   a = Vector_get__Type((t-> args ), i);
            collect_chan_in_type(g, (&a));
        }
    }
}

void   collect_chan_in_expr (CG*   g, Expr*   e) {
    if ((e  ==  NULL)) {
        return;
    }
    if (((e-> cast_to )  !=  NULL)) {
        collect_chan_in_type(g, (e-> cast_to ));
    }
    if (((e-> lhs )  !=  NULL)) {
        collect_chan_in_expr(g, (e-> lhs ));
    }
    if (((e-> rhs )  !=  NULL)) {
        collect_chan_in_expr(g, (e-> rhs ));
    }
    if (((e-> operand )  !=  NULL)) {
        collect_chan_in_expr(g, (e-> operand ));
    }
    if (((e-> args )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
            Expr   a = Vector_get__Expr((e-> args ), i);
            collect_chan_in_expr(g, (&a));
        }
    }
}

void   collect_chan_in_stmt (CG*   g, Stmt*   s) {
    if ((s  ==  NULL)) {
        return;
    }
    if (((s-> let_ty )  !=  NULL)) {
        collect_chan_in_type(g, (s-> let_ty ));
    }
    if (((s-> fn_ret_ty )  !=  NULL)) {
        collect_chan_in_type(g, (s-> fn_ret_ty ));
    }
    if (((s-> fn_params )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Param((s-> fn_params ))); i++) {
            Param   p = Vector_get__Param((s-> fn_params ), i);
            collect_chan_in_type(g, (p. ty ));
        }
    }
    if (((s-> struct_fields )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Field((s-> struct_fields ))); i++) {
            Field   f = Vector_get__Field((s-> struct_fields ), i);
            collect_chan_in_type(g, (f. ty ));
        }
    }
    if (((s-> let_value )  !=  NULL)) {
        collect_chan_in_expr(g, (s-> let_value ));
    }
    if (((s-> expr_value )  !=  NULL)) {
        collect_chan_in_expr(g, (s-> expr_value ));
    }
    if (((s-> cond )  !=  NULL)) {
        collect_chan_in_expr(g, (s-> cond ));
    }
    if (((s-> for_step )  !=  NULL)) {
        collect_chan_in_expr(g, (s-> for_step ));
    }
    if (((s-> for_init )  !=  NULL)) {
        collect_chan_in_stmt(g, (s-> for_init ));
    }
    if (((s-> then_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> then_body ), i);
            collect_chan_in_stmt(g, (&b));
        }
    }
    if (((s-> else_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> else_body ), i);
            collect_chan_in_stmt(g, (&b));
        }
    }
    if (((s-> fn_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> fn_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> fn_body ), i);
            collect_chan_in_stmt(g, (&b));
        }
    }
    if (((s-> impl_methods )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> impl_methods ))); i++) {
            Stmt   m = Vector_get__Stmt((s-> impl_methods ), i);
            collect_chan_in_stmt(g, (&m));
        }
    }
}

void   emit_chan_runtime (CG*   g) {
    if ((Vector_len__Type((g-> chan_types ))  ==  0)) {
        return;
    }
    ((g-> uses_pthread )  =  true);
    printf("%s\n", "#include <pthread.h>");
    printf("%s\n", "");
    for (int   i = 0; (i  <  Vector_len__Type((g-> chan_types ))); i++) {
        Type   t = Vector_get__Type((g-> chan_types ), i);
        const char*   m = mangle_type((&t));
        const char*   st = __glide_string_concat(__glide_string_concat("__glide_chan_", m), "_t");
        printf("%s\n", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("typedef struct ", st), " "), st), ";"));
    }
    printf("%s\n", "");
    for (int   i = 0; (i  <  Vector_len__Type((g-> chan_types ))); i++) {
        Type   t = Vector_get__Type((g-> chan_types ), i);
        const char*   m = mangle_type((&t));
        const char*   tc = type_to_c((&t));
        const char*   st = __glide_string_concat(__glide_string_concat("__glide_chan_", m), "_t");
        printf("%s\n", __glide_string_concat(__glide_string_concat("struct ", st), " {"));
        printf("%s\n", "    pthread_mutex_t mu;");
        printf("%s\n", "    pthread_cond_t  not_empty;");
        printf("%s\n", "    pthread_cond_t  not_full;");
        printf("%s\n", __glide_string_concat(__glide_string_concat("    ", tc), "* buf;"));
        printf("%s\n", "    int cap;");
        printf("%s\n", "    int len;");
        printf("%s\n", "    int head;");
        printf("%s\n", "    int tail;");
        printf("%s\n", "    int closed;");
        printf("%s\n", "};");
        printf("%s\n", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("static ", st), "* __glide_make_chan_"), m), "(int cap) {"));
        printf("%s\n", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("    ", st), "* c = ("), st), "*)malloc(sizeof("), st), "));"));
        printf("%s\n", "    if (cap < 1) cap = 1;");
        printf("%s\n", "    pthread_mutex_init(&c->mu, NULL);");
        printf("%s\n", "    pthread_cond_init(&c->not_empty, NULL);");
        printf("%s\n", "    pthread_cond_init(&c->not_full, NULL);");
        printf("%s\n", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("    c->buf = (", tc), "*)malloc(sizeof("), tc), ") * cap);"));
        printf("%s\n", "    c->cap = cap; c->len = 0; c->head = 0; c->tail = 0; c->closed = 0;");
        printf("%s\n", "    return c;");
        printf("%s\n", "}");
        printf("%s\n", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("static void __glide_send_", m), "("), st), "* c, "), tc), " v) {"));
        printf("%s\n", "    pthread_mutex_lock(&c->mu);");
        printf("%s\n", "    while (c->len == c->cap && !c->closed) pthread_cond_wait(&c->not_full, &c->mu);");
        printf("%s\n", "    if (c->closed) { pthread_mutex_unlock(&c->mu); return; }");
        printf("%s\n", "    c->buf[c->tail] = v;");
        printf("%s\n", "    c->tail = (c->tail + 1) % c->cap;");
        printf("%s\n", "    c->len++;");
        printf("%s\n", "    pthread_cond_signal(&c->not_empty);");
        printf("%s\n", "    pthread_mutex_unlock(&c->mu);");
        printf("%s\n", "}");
        printf("%s\n", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("static ", tc), " __glide_recv_"), m), "("), st), "* c) {"));
        printf("%s\n", "    pthread_mutex_lock(&c->mu);");
        printf("%s\n", "    while (c->len == 0 && !c->closed) pthread_cond_wait(&c->not_empty, &c->mu);");
        printf("%s\n", __glide_string_concat(__glide_string_concat("    ", tc), " v;"));
        printf("%s\n", "    if (c->len == 0) { memset(&v, 0, sizeof(v)); pthread_mutex_unlock(&c->mu); return v; }");
        printf("%s\n", "    v = c->buf[c->head];");
        printf("%s\n", "    c->head = (c->head + 1) % c->cap;");
        printf("%s\n", "    c->len--;");
        printf("%s\n", "    pthread_cond_signal(&c->not_full);");
        printf("%s\n", "    pthread_mutex_unlock(&c->mu);");
        printf("%s\n", "    return v;");
        printf("%s\n", "}");
        printf("%s\n", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("static void __glide_close_", m), "("), st), "* c) {"));
        printf("%s\n", "    pthread_mutex_lock(&c->mu);");
        printf("%s\n", "    c->closed = 1;");
        printf("%s\n", "    pthread_cond_broadcast(&c->not_empty);");
        printf("%s\n", "    pthread_cond_broadcast(&c->not_full);");
        printf("%s\n", "    pthread_mutex_unlock(&c->mu);");
        printf("%s\n", "}");
        printf("%s\n", "");
    }
}

void   pre_emit_spawn_stubs_in_stmt (CG*   g, Stmt*   s) {
    if ((s  ==  NULL)) {
        return;
    }
    if (((s-> kind )  ==  ST_SPAWN)) {
        int   id = (g-> spawn_count );
        ((g-> spawn_count )  =  (id  +  1));
        emit_spawn_stub(g, (s-> expr_value ), id);
        return;
    }
    if (((s-> for_init )  !=  NULL)) {
        pre_emit_spawn_stubs_in_stmt(g, (s-> for_init ));
    }
    if (((s-> then_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> then_body ), i);
            pre_emit_spawn_stubs_in_stmt(g, (&b));
        }
    }
    if (((s-> else_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> else_body ), i);
            pre_emit_spawn_stubs_in_stmt(g, (&b));
        }
    }
    if (((s-> fn_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> fn_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> fn_body ), i);
            pre_emit_spawn_stubs_in_stmt(g, (&b));
        }
    }
    if (((s-> impl_methods )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> impl_methods ))); i++) {
            Stmt   m = Vector_get__Stmt((s-> impl_methods ), i);
            pre_emit_spawn_stubs_in_stmt(g, (&m));
        }
    }
}

void   emit_spawn_stub (CG*   g, Expr*   call, int   id) {
    if ((((call  ==  NULL)  ||  ((call-> kind )  !=  EX_CALL))  ||  ((call-> lhs )  ==  NULL))) {
        return;
    }
    if ((((call-> lhs )-> kind )  !=  EX_IDENT)) {
        return;
    }
    const char*   fn_name = ((call-> lhs )-> str_val );
    if ((!HashMap_contains__Stmt((g-> fns ), fn_name))) {
        return;
    }
    Stmt   fnstmt = HashMap_get__Stmt((g-> fns ), fn_name);
    if (((fnstmt. fn_params )  ==  NULL)) {
        return;
    }
    int   n = Vector_len__Param((fnstmt. fn_params ));
    const char*   ids = int_to_str(id);
    const char*   an = __glide_string_concat("__glide_spawn_args_", ids);
    printf("%s\n", __glide_string_concat(__glide_string_concat("typedef struct ", an), " {"));
    for (int   i = 0; (i  <  n); i++) {
        Param   p = Vector_get__Param((fnstmt. fn_params ), i);
        printf("%s\n", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("    ", type_to_c((p. ty ))), " a"), int_to_str(i)), ";"));
    }
    printf("%s\n", __glide_string_concat(__glide_string_concat("} ", an), ";"));
    printf("%s\n", __glide_string_concat(__glide_string_concat("static void* __glide_spawn_stub_", ids), "(void* __raw) {"));
    printf("%s\n", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("    ", an), "* __args = ("), an), "*)__raw;"));
    const char*   call_line = __glide_string_concat(__glide_string_concat("    ", fn_name), "(");
    for (int   i = 0; (i  <  n); i++) {
        if ((i  >  0)) {
            (call_line  =  __glide_string_concat(call_line, ", "));
        }
        (call_line  =  __glide_string_concat(__glide_string_concat(call_line, "__args->a"), int_to_str(i)));
    }
    (call_line  =  __glide_string_concat(call_line, ");"));
    printf("%s\n", call_line);
    printf("%s\n", "    free(__raw);");
    printf("%s\n", "    return NULL;");
    printf("%s\n", "}");
}

const char*   int_to_str (int   n) {
    if ((n  ==  0)) {
        return "0";
    }
    int   x = n;
    bool   neg = false;
    if ((x  <  0)) {
        (neg  =  true);
        (x  =  (-x));
    }
    const char*   s = "";
    while ((x  >  0)) {
        int   d = (x  %  10);
        const char*   ch = digit_str(d);
        (s  =  __glide_string_concat(ch, s));
        (x  =  (x  /  10));
    }
    if (neg) {
        (s  =  __glide_string_concat("-", s));
    }
    return s;
}

const char*   digit_str (int   d) {
    if ((d  ==  0)) {
        return "0";
    }
    if ((d  ==  1)) {
        return "1";
    }
    if ((d  ==  2)) {
        return "2";
    }
    if ((d  ==  3)) {
        return "3";
    }
    if ((d  ==  4)) {
        return "4";
    }
    if ((d  ==  5)) {
        return "5";
    }
    if ((d  ==  6)) {
        return "6";
    }
    if ((d  ==  7)) {
        return "7";
    }
    if ((d  ==  8)) {
        return "8";
    }
    return "9";
}

bool   is_stdlib_primitive (const char*   name) {
    if (__glide_string_eq(name, "int")) {
        return true;
    }
    if (__glide_string_eq(name, "uint")) {
        return true;
    }
    if (__glide_string_eq(name, "long")) {
        return true;
    }
    if (__glide_string_eq(name, "float")) {
        return true;
    }
    if (__glide_string_eq(name, "char")) {
        return true;
    }
    if (__glide_string_eq(name, "bool")) {
        return true;
    }
    if (__glide_string_eq(name, "string")) {
        return true;
    }
    return false;
}

void   emit_stdlib_runtime (void) {
    printf("%s\n", "// ---- glide runtime ----");
    printf("%s\n", "static int __glide_string_len(const char* s) { return (int)strlen(s); }");
    printf("%s\n", "static bool __glide_string_eq(const char* a, const char* b) { return strcmp(a, b) == 0; }");
    printf("%s\n", "static char __glide_string_at(const char* s, int i) { return s[i]; }");
    printf("%s\n", "static const char* __glide_string_concat(const char* a, const char* b) {");
    printf("%s\n", "    size_t la = strlen(a), lb = strlen(b);");
    printf("%s\n", "    char* out = (char*)malloc(la + lb + 1);");
    printf("%s\n", "    memcpy(out, a, la); memcpy(out + la, b, lb); out[la + lb] = 0;");
    printf("%s\n", "    return out;");
    printf("%s\n", "}");
    printf("%s\n", "static const char* __glide_string_substring(const char* s, int start, int end) {");
    printf("%s\n", "    int n = (int)strlen(s);");
    printf("%s\n", "    if (start < 0) start = 0;");
    printf("%s\n", "    if (end > n) end = n;");
    printf("%s\n", "    if (start > end) start = end;");
    printf("%s\n", "    int len = end - start;");
    printf("%s\n", "    char* out = (char*)malloc((size_t)len + 1);");
    printf("%s\n", "    memcpy(out, s + start, (size_t)len); out[len] = 0;");
    printf("%s\n", "    return out;");
    printf("%s\n", "}");
    printf("%s\n", "static int __glide_char_to_int(char c) { return (int)(unsigned char)c; }");
    printf("%s\n", "static bool __glide_char_is_digit(char c) { return c >= '0' && c <= '9'; }");
    printf("%s\n", "static bool __glide_char_is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }");
    printf("%s\n", "static int __glide_int_abs(int n) { return n < 0 ? -n : n; }");
    printf("%s\n", "static const char* __glide_int_to_string(int n) {");
    printf("%s\n", "    char buf[32];");
    printf("%s\n", "    int len = snprintf(buf, sizeof(buf), \"%d\", n);");
    printf("%s\n", "    char* out = (char*)malloc((size_t)len + 1);");
    printf("%s\n", "    memcpy(out, buf, (size_t)len + 1);");
    printf("%s\n", "    return out;");
    printf("%s\n", "}");
    printf("%s\n", "static const char* __glide_bool_to_string(bool b) { return b ? \"true\" : \"false\"; }");
    printf("%s\n", "#include <stdarg.h>");
    printf("%s\n", "static const char* __glide_format(const char* fmt, ...) {");
    printf("%s\n", "    va_list ap1, ap2;");
    printf("%s\n", "    va_start(ap1, fmt);");
    printf("%s\n", "    va_copy(ap2, ap1);");
    printf("%s\n", "    int n = vsnprintf(NULL, 0, fmt, ap1);");
    printf("%s\n", "    va_end(ap1);");
    printf("%s\n", "    char* out = (char*)malloc((size_t)n + 1);");
    printf("%s\n", "    vsnprintf(out, (size_t)n + 1, fmt, ap2);");
    printf("%s\n", "    va_end(ap2);");
    printf("%s\n", "    return out;");
    printf("%s\n", "}");
    printf("%s\n", "static int __glide_argc_g = 0;");
    printf("%s\n", "static char** __glide_argv_g = NULL;");
    printf("%s\n", "static void __glide_args_init(int argc, char** argv) { __glide_argc_g = argc; __glide_argv_g = argv; }");
    printf("%s\n", "static int args_count(void) { return __glide_argc_g; }");
    printf("%s\n", "static const char* args_at(int i) {");
    printf("%s\n", "    if (i < 0 || i >= __glide_argc_g) return \"\";");
    printf("%s\n", "    return __glide_argv_g[i];");
    printf("%s\n", "}");
    printf("%s\n", "static const char* read_file(const char* path) {");
    printf("%s\n", "    FILE* f = fopen(path, \"rb\");");
    printf("%s\n", "    if (!f) return \"\";");
    printf("%s\n", "    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);");
    printf("%s\n", "    char* buf = (char*)malloc((size_t)n + 1);");
    printf("%s\n", "    size_t got = fread(buf, 1, (size_t)n, f); fclose(f); buf[got] = 0;");
    printf("%s\n", "    return buf;");
    printf("%s\n", "}");
    printf("%s\n", "static bool write_file(const char* path, const char* content) {");
    printf("%s\n", "    FILE* f = fopen(path, \"wb\"); if (!f) return false;");
    printf("%s\n", "    size_t n = strlen(content);");
    printf("%s\n", "    size_t wrote = fwrite(content, 1, n, f); fclose(f);");
    printf("%s\n", "    return wrote == n;");
    printf("%s\n", "}");
    printf("%s\n", "#ifdef _WIN32");
    printf("%s\n", "#include <io.h>");
    printf("%s\n", "#include <windows.h>");
    printf("%s\n", "#define __glide_dup _dup");
    printf("%s\n", "#define __glide_dup2 _dup2");
    printf("%s\n", "#define __glide_close _close");
    printf("%s\n", "#define __glide_fileno _fileno");
    printf("%s\n", "#else");
    printf("%s\n", "#include <unistd.h>");
    printf("%s\n", "#define __glide_dup dup");
    printf("%s\n", "#define __glide_dup2 dup2");
    printf("%s\n", "#define __glide_close close");
    printf("%s\n", "#define __glide_fileno fileno");
    printf("%s\n", "#endif");
    printf("%s\n", "static int __glide_redirect_to(const char* path) {");
    printf("%s\n", "    fflush(stdout);");
    printf("%s\n", "    int saved = __glide_dup(1);");
    printf("%s\n", "    if (saved < 0) return -1;");
    printf("%s\n", "    FILE* f = fopen(path, \"w\");");
    printf("%s\n", "    if (!f) { __glide_close(saved); return -1; }");
    printf("%s\n", "    __glide_dup2(__glide_fileno(f), 1);");
    printf("%s\n", "    fclose(f);");
    printf("%s\n", "    return saved;");
    printf("%s\n", "}");
    printf("%s\n", "static void __glide_restore_stdout(int saved) {");
    printf("%s\n", "    fflush(stdout);");
    printf("%s\n", "    if (saved >= 0) { __glide_dup2(saved, 1); __glide_close(saved); }");
    printf("%s\n", "}");
    printf("%s\n", "static int __glide_shell(const char* cmd) { return system(cmd); }");
    printf("%s\n", "static const char* __glide_getenv(const char* name) { const char* v = getenv(name); return v ? v : \"\"; }");
    printf("%s\n", "static bool __glide_file_exists(const char* path) {");
    printf("%s\n", "    FILE* f = fopen(path, \"rb\"); if (!f) return false; fclose(f); return true;");
    printf("%s\n", "}");
    printf("%s\n", "#ifdef _WIN32");
    printf("%s\n", "#include <fcntl.h>");
    printf("%s\n", "static void __glide_set_binary_io(void) {");
    printf("%s\n", "    _setmode(_fileno(stdin), _O_BINARY);");
    printf("%s\n", "    _setmode(_fileno(stdout), _O_BINARY);");
    printf("%s\n", "}");
    printf("%s\n", "#else");
    printf("%s\n", "static void __glide_set_binary_io(void) {}");
    printf("%s\n", "#endif");
    printf("%s\n", "static const char* __glide_read_line(void) {");
    printf("%s\n", "    static char buf[8192];");
    printf("%s\n", "    if (!fgets(buf, sizeof(buf), stdin)) { buf[0] = 0; return buf; }");
    printf("%s\n", "    return buf;");
    printf("%s\n", "}");
    printf("%s\n", "static const char* __glide_read_bytes(int n) {");
    printf("%s\n", "    if (n <= 0) return \"\";");
    printf("%s\n", "    char* buf = (char*)malloc((size_t)n + 1);");
    printf("%s\n", "    size_t got = fread(buf, 1, (size_t)n, stdin);");
    printf("%s\n", "    buf[got] = 0;");
    printf("%s\n", "    return buf;");
    printf("%s\n", "}");
    printf("%s\n", "static void __glide_write_str(const char* s) {");
    printf("%s\n", "    fputs(s, stdout);");
    printf("%s\n", "}");
    printf("%s\n", "static void __glide_write_bytes(const char* s, int n) {");
    printf("%s\n", "    if (n > 0) fwrite(s, 1, (size_t)n, stdout);");
    printf("%s\n", "}");
    printf("%s\n", "static void __glide_flush_stdout(void) { fflush(stdout); }");
    printf("%s\n", "static void __glide_log(const char* s) {");
    printf("%s\n", "    fputs(s, stderr); fputc('\\n', stderr); fflush(stderr);");
    printf("%s\n", "}");
    printf("%s\n", "static int __glide_is_windows(void) {");
    printf("%s\n", "#ifdef _WIN32");
    printf("%s\n", "    return 1;");
    printf("%s\n", "#else");
    printf("%s\n", "    return 0;");
    printf("%s\n", "#endif");
    printf("%s\n", "}");
    printf("%s\n", "#ifdef _WIN32");
    printf("%s\n", "static const char* __glide_exe_path(void) {");
    printf("%s\n", "    static char buf[1024];");
    printf("%s\n", "    DWORD n = GetModuleFileNameA(NULL, buf, sizeof(buf));");
    printf("%s\n", "    if (n == 0 || n >= sizeof(buf)) buf[0] = 0;");
    printf("%s\n", "    return buf;");
    printf("%s\n", "}");
    printf("%s\n", "static const char* __glide_exe_dir(void) {");
    printf("%s\n", "    static char buf[1024];");
    printf("%s\n", "    DWORD n = GetModuleFileNameA(NULL, buf, sizeof(buf));");
    printf("%s\n", "    if (n == 0 || n >= sizeof(buf)) { buf[0] = 0; return buf; }");
    printf("%s\n", "    for (DWORD i = n; i > 0; i--) { if (buf[i-1] == '\\\\' || buf[i-1] == '/') { buf[i-1] = 0; return buf; } }");
    printf("%s\n", "    return \"\";");
    printf("%s\n", "}");
    printf("%s\n", "#else");
    printf("%s\n", "static const char* __glide_exe_path(void) {");
    printf("%s\n", "    static char buf[1024];");
    printf("%s\n", "    ssize_t n = readlink(\"/proc/self/exe\", buf, sizeof(buf) - 1);");
    printf("%s\n", "    if (n <= 0) { buf[0] = 0; return buf; }");
    printf("%s\n", "    buf[n] = 0;");
    printf("%s\n", "    return buf;");
    printf("%s\n", "}");
    printf("%s\n", "static const char* __glide_exe_dir(void) {");
    printf("%s\n", "    static char buf[1024];");
    printf("%s\n", "    ssize_t n = readlink(\"/proc/self/exe\", buf, sizeof(buf) - 1);");
    printf("%s\n", "    if (n <= 0) { buf[0] = 0; return buf; }");
    printf("%s\n", "    buf[n] = 0;");
    printf("%s\n", "    for (ssize_t i = n; i > 0; i--) { if (buf[i-1] == '/') { buf[i-1] = 0; return buf; } }");
    printf("%s\n", "    return \"\";");
    printf("%s\n", "}");
    printf("%s\n", "#endif");
    printf("%s\n", "typedef struct Arena { unsigned char* head; int cap; int used; } Arena;");
    printf("%s\n", "static Arena* Arena_new(int cap) {");
    printf("%s\n", "    Arena* a = (Arena*)malloc(sizeof(Arena));");
    printf("%s\n", "    a->head = (unsigned char*)malloc((size_t)cap);");
    printf("%s\n", "    a->cap = cap; a->used = 0;");
    printf("%s\n", "    return a;");
    printf("%s\n", "}");
    printf("%s\n", "static void* Arena_alloc(Arena* a, int size) {");
    printf("%s\n", "    int aligned = (size + 7) & ~7;");
    printf("%s\n", "    if (a->used + aligned > a->cap) { fprintf(stderr, \"arena oom\\n\"); exit(1); }");
    printf("%s\n", "    void* p = (void*)(a->head + a->used);");
    printf("%s\n", "    a->used += aligned;");
    printf("%s\n", "    return p;");
    printf("%s\n", "}");
    printf("%s\n", "static void Arena_free(Arena* a) { free(a->head); free(a); }");
    printf("%s\n", "static int Arena_used(Arena* a) { return a->used; }");
    printf("%s\n", "static void Arena_reset(Arena* a) { a->used = 0; }");
    printf("%s\n", "");
}

CG*   CG_new (void) {
    CG*   g = (( CG* )malloc(sizeof( CG )));
    HashMap__Type*   s = HashMap_new__Type();
    HashMap__Stmt*   gs = HashMap_new__Stmt();
    HashMap__Stmt*   gf = HashMap_new__Stmt();
    HashMap__Stmt*   st = HashMap_new__Stmt();
    HashMap__Stmt*   en = HashMap_new__Stmt();
    HashMap__Stmt*   fnm = HashMap_new__Stmt();
    HashMap__bool*   em = HashMap_new__bool();
    Vector__FnMonoEntry*   q = Vector_new__FnMonoEntry();
    Vector__Expr*   ds = Vector_new__Expr();
    ((g-> defer_stack )  =  ds);
    Vector__Expr*   ads = Vector_new__Expr();
    ((g-> auto_drop_stack )  =  ads);
    Vector__AnonFn*   af = Vector_new__AnonFn();
    ((g-> anon_fns )  =  af);
    ((g-> anon_count )  =  0);
    Vector__Type*   ct = Vector_new__Type();
    ((g-> chan_types )  =  ct);
    ((g-> spawn_count )  =  0);
    ((g-> uses_pthread )  =  false);
    Vector__Type*   rt = Vector_new__Type();
    ((g-> result_types )  =  rt);
    ((g-> current_ret_ty )  =  NULL);
    ((g-> scope )  =  s);
    ((g-> generic_structs )  =  gs);
    ((g-> generic_fns )  =  gf);
    ((g-> structs )  =  st);
    ((g-> enums )  =  en);
    ((g-> fns )  =  fnm);
    ((g-> emitted_monos )  =  em);
    ((g-> fn_mono_queue )  =  q);
    return g;
}

void   CG_emit_deferred_reverse (CG*   self, int   depth) {
    int   na = Vector_len__Expr((self-> auto_drop_stack ));
    for (int   i = (na  -  1); (i  >=  0); (i  =  (i  -  1))) {
        ind(depth);
        Expr   ex = Vector_get__Expr((self-> auto_drop_stack ), i);
        emit_expr(self, (&ex));
        printf("%s\n", ";");
    }
    int   nd = Vector_len__Expr((self-> defer_stack ));
    for (int   i = (nd  -  1); (i  >=  0); (i  =  (i  -  1))) {
        ind(depth);
        Expr   ex = Vector_get__Expr((self-> defer_stack ), i);
        emit_expr(self, (&ex));
        printf("%s\n", ";");
    }
}

void   CG_emit_block_drops (CG*   self, int   saved, int   depth) {
    int   n = Vector_len__Expr((self-> auto_drop_stack ));
    for (int   i = (n  -  1); (i  >=  saved); (i  =  (i  -  1))) {
        ind(depth);
        Expr   ex = Vector_get__Expr((self-> auto_drop_stack ), i);
        emit_expr(self, (&ex));
        printf("%s\n", ";");
    }
    while ((Vector_len__Expr((self-> auto_drop_stack ))  >  saved)) {
        Expr   _ev = Vector_pop__Expr((self-> auto_drop_stack ));
    }
}

void   CG_free (CG*   self) {
    HashMap_free__Type((self-> scope ));
    HashMap_free__Stmt((self-> generic_structs ));
    HashMap_free__Stmt((self-> generic_fns ));
    HashMap_free__Stmt((self-> structs ));
    HashMap_free__Stmt((self-> enums ));
    HashMap_free__Stmt((self-> fns ));
    HashMap_free__bool((self-> emitted_monos ));
    free((( void* )self));
}

void   CG_declare (CG*   self, const char*   name, Type*   ty) {
    if ((ty  !=  NULL)) {
        HashMap_insert__Type((self-> scope ), name, (*ty));
    }
}

Type*   CG_lookup (CG*   self, const char*   name) {
    if ((!HashMap_contains__Type((self-> scope ), name))) {
        return NULL;
    }
    Type   t = HashMap_get__Type((self-> scope ), name);
    Type*   p = (( Type* )malloc(sizeof( Type )));
    ((*p)  =  t);
    return p;
}

Type*   strip_ptr (Type*   t) {
    if ((t  ==  NULL)) {
        return NULL;
    }
    if (((((t-> kind )  ==  TY_POINTER)  ||  ((t-> kind )  ==  TY_BORROW))  ||  ((t-> kind )  ==  TY_BORROW_MUT))) {
        return strip_ptr((t-> inner ));
    }
    return t;
}

Type*   infer_for_codegen (CG*   g, Expr*   e) {
    if ((e  ==  NULL)) {
        return NULL;
    }
    if (((e-> kind )  ==  EX_INT)) {
        return ty_named("int");
    }
    if (((e-> kind )  ==  EX_FLOAT)) {
        return ty_named("float");
    }
    if (((e-> kind )  ==  EX_STRING)) {
        return ty_named("string");
    }
    if (((e-> kind )  ==  EX_BOOL)) {
        return ty_named("bool");
    }
    if (((e-> kind )  ==  EX_CHAR)) {
        return ty_named("char");
    }
    if (((e-> kind )  ==  EX_NULL)) {
        return ty_pointer(ty_named("void"));
    }
    if (((e-> kind )  ==  EX_IDENT)) {
        return CG_lookup(g, (e-> str_val ));
    }
    if (((e-> kind )  ==  EX_UNARY)) {
        Type*   inner = infer_for_codegen(g, (e-> operand ));
        if (((((e-> op_code )  ==  UN_DEREF)  &&  (inner  !=  NULL))  &&  ((inner-> kind )  ==  TY_POINTER))) {
            return (inner-> inner );
        }
        if ((((e-> op_code )  ==  UN_ADDR)  ||  ((e-> op_code )  ==  UN_ADDR_MUT))) {
            return ty_pointer(inner);
        }
        return inner;
    }
    if (((e-> kind )  ==  EX_MEMBER)) {
        Type*   recv_ty = infer_for_codegen(g, (e-> lhs ));
        Type*   stripped = strip_ptr(recv_ty);
        if ((stripped  ==  NULL)) {
            return NULL;
        }
        if (((stripped-> kind )  ==  TY_SLICE)) {
            if (__glide_string_eq((e-> field ), "len")) {
                return ty_named("usize");
            }
            return NULL;
        }
        if ((((stripped-> kind )  ==  TY_NAMED)  &&  HashMap_contains__Stmt((g-> structs ), (stripped-> name )))) {
            Stmt   sd = HashMap_get__Stmt((g-> structs ), (stripped-> name ));
            if (((sd. struct_fields )  !=  NULL)) {
                for (int   i = 0; (i  <  Vector_len__Field((sd. struct_fields ))); i++) {
                    Field   f = Vector_get__Field((sd. struct_fields ), i);
                    if (__glide_string_eq((f. name ), (e-> field ))) {
                        return (f. ty );
                    }
                }
            }
        }
        if ((((stripped-> kind )  ==  TY_GENERIC)  &&  HashMap_contains__Stmt((g-> generic_structs ), (stripped-> name )))) {
            Stmt   sd = HashMap_get__Stmt((g-> generic_structs ), (stripped-> name ));
            if ((((sd. struct_fields )  !=  NULL)  &&  ((sd. type_params )  !=  NULL))) {
                for (int   i = 0; (i  <  Vector_len__Field((sd. struct_fields ))); i++) {
                    Field   f = Vector_get__Field((sd. struct_fields ), i);
                    if (__glide_string_eq((f. name ), (e-> field ))) {
                        return subst_type((f. ty ), (sd. type_params ), (stripped-> args ));
                    }
                }
            }
        }
        return NULL;
    }
    if (((e-> kind )  ==  EX_CAST)) {
        return (e-> cast_to );
    }
    if (((e-> kind )  ==  EX_BINARY)) {
        int   op = (e-> op_code );
        if (((((((((op  ==  OP_EQ)  ||  (op  ==  OP_NE))  ||  (op  ==  OP_LT))  ||  (op  ==  OP_LE))  ||  (op  ==  OP_GT))  ||  (op  ==  OP_GE))  ||  (op  ==  OP_AND))  ||  (op  ==  OP_OR))) {
            return ty_named("bool");
        }
        return infer_for_codegen(g, (e-> lhs ));
    }
    if (((e-> kind )  ==  EX_INDEX)) {
        Type*   base = infer_for_codegen(g, (e-> lhs ));
        if ((base  ==  NULL)) {
            return NULL;
        }
        if ((((((base-> kind )  ==  TY_POINTER)  ||  ((base-> kind )  ==  TY_BORROW))  ||  ((base-> kind )  ==  TY_BORROW_MUT))  ||  ((base-> kind )  ==  TY_SLICE))) {
            return (base-> inner );
        }
        return NULL;
    }
    if (((e-> kind )  ==  EX_CALL)) {
        if ((((e-> lhs )  !=  NULL)  &&  (((e-> lhs )-> kind )  ==  EX_IDENT))) {
            const char*   nm = ((e-> lhs )-> str_val );
            if (__glide_string_eq(nm, "args_count")) {
                return ty_named("int");
            }
            if (__glide_string_eq(nm, "args_at")) {
                return ty_named("string");
            }
            if (__glide_string_eq(nm, "read_file")) {
                return ty_named("string");
            }
            if (__glide_string_eq(nm, "write_file")) {
                return ty_named("bool");
            }
            if ((__glide_string_eq(nm, "malloc")  ||  __glide_string_eq(nm, "calloc"))) {
                return ty_pointer(ty_named("void"));
            }
            if (HashMap_contains__Stmt((g-> fns ), nm)) {
                Stmt   s = HashMap_get__Stmt((g-> fns ), nm);
                return (s. fn_ret_ty );
            }
            if (HashMap_contains__Stmt((g-> generic_fns ), nm)) {
                Stmt   gn = HashMap_get__Stmt((g-> generic_fns ), nm);
                return (gn. fn_ret_ty );
            }
        }
        if ((((e-> lhs )  !=  NULL)  &&  (((e-> lhs )-> kind )  ==  EX_PATH))) {
            const char*   pname = __glide_string_concat(__glide_string_concat(((e-> lhs )-> str_val ), "_"), ((e-> lhs )-> field ));
            if (HashMap_contains__Stmt((g-> fns ), pname)) {
                return (HashMap_get__Stmt((g-> fns ), pname). fn_ret_ty );
            }
            if (HashMap_contains__Stmt((g-> generic_fns ), pname)) {
                return (HashMap_get__Stmt((g-> generic_fns ), pname). fn_ret_ty );
            }
        }
        if ((((e-> lhs )  !=  NULL)  &&  (((e-> lhs )-> kind )  ==  EX_MEMBER))) {
            Type*   rt = infer_for_codegen(g, ((e-> lhs )-> lhs ));
            Type*   stripped = strip_ptr(rt);
            if (((stripped  !=  NULL)  &&  ((stripped-> kind )  ==  TY_NAMED))) {
                if (is_stdlib_primitive((stripped-> name ))) {
                    Type*   std_ret = stdlib_method_ret((stripped-> name ), ((e-> lhs )-> field ));
                    if ((std_ret  !=  NULL)) {
                        return std_ret;
                    }
                }
                const char*   mname = __glide_string_concat(__glide_string_concat((stripped-> name ), "_"), ((e-> lhs )-> field ));
                if (HashMap_contains__Stmt((g-> fns ), mname)) {
                    return (HashMap_get__Stmt((g-> fns ), mname). fn_ret_ty );
                }
                if (HashMap_contains__Stmt((g-> generic_fns ), mname)) {
                    return (HashMap_get__Stmt((g-> generic_fns ), mname). fn_ret_ty );
                }
            }
            if (((stripped  !=  NULL)  &&  ((stripped-> kind )  ==  TY_GENERIC))) {
                const char*   mname = __glide_string_concat(__glide_string_concat((stripped-> name ), "_"), ((e-> lhs )-> field ));
                if (HashMap_contains__Stmt((g-> generic_fns ), mname)) {
                    Stmt   template = HashMap_get__Stmt((g-> generic_fns ), mname);
                    return subst_type((template. fn_ret_ty ), (template. type_params ), (stripped-> args ));
                }
            }
        }
        return NULL;
    }
    if (((e-> kind )  ==  EX_PATH)) {
        const char*   pname = __glide_string_concat(__glide_string_concat((e-> str_val ), "_"), (e-> field ));
        if (HashMap_contains__Stmt((g-> fns ), pname)) {
            return (HashMap_get__Stmt((g-> fns ), pname). fn_ret_ty );
        }
        if (HashMap_contains__Stmt((g-> generic_fns ), pname)) {
            return (HashMap_get__Stmt((g-> generic_fns ), pname). fn_ret_ty );
        }
        return NULL;
    }
    return NULL;
}

const char*   type_to_c (Type*   t) {
    if ((t  ==  NULL)) {
        return "void";
    }
    if (((t-> kind )  ==  TY_NAMED)) {
        const char*   n = (t-> name );
        if (__glide_string_eq(n, "int")) {
            return "int";
        }
        if (__glide_string_eq(n, "uint")) {
            return "unsigned int";
        }
        if (__glide_string_eq(n, "long")) {
            return "long";
        }
        if (__glide_string_eq(n, "ulong")) {
            return "unsigned long";
        }
        if (__glide_string_eq(n, "i8")) {
            return "int8_t";
        }
        if (__glide_string_eq(n, "i16")) {
            return "int16_t";
        }
        if (__glide_string_eq(n, "i32")) {
            return "int32_t";
        }
        if (__glide_string_eq(n, "i64")) {
            return "int64_t";
        }
        if (__glide_string_eq(n, "u8")) {
            return "uint8_t";
        }
        if (__glide_string_eq(n, "u16")) {
            return "uint16_t";
        }
        if (__glide_string_eq(n, "u32")) {
            return "uint32_t";
        }
        if (__glide_string_eq(n, "u64")) {
            return "uint64_t";
        }
        if (__glide_string_eq(n, "usize")) {
            return "size_t";
        }
        if (__glide_string_eq(n, "isize")) {
            return "ptrdiff_t";
        }
        if (__glide_string_eq(n, "float")) {
            return "double";
        }
        if (__glide_string_eq(n, "f32")) {
            return "float";
        }
        if (__glide_string_eq(n, "f64")) {
            return "double";
        }
        if (__glide_string_eq(n, "bool")) {
            return "bool";
        }
        if (__glide_string_eq(n, "char")) {
            return "char";
        }
        if (__glide_string_eq(n, "string")) {
            return "const char*";
        }
        if (__glide_string_eq(n, "void")) {
            return "void";
        }
        if (__glide_string_eq(n, "__fn_ptr__")) {
            return "void*";
        }
        return n;
    }
    if (((t-> kind )  ==  TY_POINTER)) {
        return __glide_string_concat(type_to_c((t-> inner )), "*");
    }
    if (((t-> kind )  ==  TY_BORROW)) {
        return __glide_string_concat(type_to_c((t-> inner )), " const*");
    }
    if (((t-> kind )  ==  TY_BORROW_MUT)) {
        return __glide_string_concat(type_to_c((t-> inner )), "*");
    }
    if (((t-> kind )  ==  TY_SLICE)) {
        return __glide_string_concat(__glide_string_concat("/* []", type_to_c((t-> inner ))), " */ void*");
    }
    if (((t-> kind )  ==  TY_GENERIC)) {
        if (((__glide_string_eq((t-> name ), "chan")  &&  ((t-> args )  !=  NULL))  &&  (Vector_len__Type((t-> args ))  >  0))) {
            Type   inner = Vector_get__Type((t-> args ), 0);
            return __glide_string_concat(__glide_string_concat("__glide_chan_", mangle_type((&inner))), "_t*");
        }
        return mangle_generic((t-> name ), (t-> args ));
    }
    if (((t-> kind )  ==  TY_FNPTR)) {
        return "void*";
    }
    if (((t-> kind )  ==  TY_RESULT)) {
        return __glide_string_concat(__glide_string_concat("__glide_result_", mangle_type((t-> inner ))), "_t");
    }
    return "void";
}

const char*   fnptr_cast_str (Type*   t) {
    const char*   s = __glide_string_concat(type_to_c((t-> fnptr_ret )), "(*)(");
    if ((((t-> args )  ==  NULL)  ||  (Vector_len__Type((t-> args ))  ==  0))) {
        (s  =  __glide_string_concat(s, "void"));
    } else {
        for (int   i = 0; (i  <  Vector_len__Type((t-> args ))); i++) {
            if ((i  >  0)) {
                (s  =  __glide_string_concat(s, ", "));
            }
            Type   p = Vector_get__Type((t-> args ), i);
            (s  =  __glide_string_concat(s, type_to_c((&p))));
        }
    }
    (s  =  __glide_string_concat(s, ")"));
    return s;
}

const char*   mangle_generic (const char*   name, Vector__Type*   args) {
    const char*   out = __glide_string_concat(name, "__");
    if ((args  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Type(args)); i++) {
            if ((i  >  0)) {
                (out  =  __glide_string_concat(out, "_"));
            }
            Type   a = Vector_get__Type(args, i);
            (out  =  __glide_string_concat(out, mangle_type((&a))));
        }
    }
    return out;
}

const char*   mangle_type (Type*   t) {
    if ((t  ==  NULL)) {
        return "void";
    }
    if (((t-> kind )  ==  TY_NAMED)) {
        return (t-> name );
    }
    if (((t-> kind )  ==  TY_POINTER)) {
        return __glide_string_concat(mangle_type((t-> inner )), "_ptr");
    }
    if (((t-> kind )  ==  TY_BORROW)) {
        return __glide_string_concat(mangle_type((t-> inner )), "_ref");
    }
    if (((t-> kind )  ==  TY_BORROW_MUT)) {
        return __glide_string_concat(mangle_type((t-> inner )), "_refmut");
    }
    if (((t-> kind )  ==  TY_SLICE)) {
        return __glide_string_concat("slice_", mangle_type((t-> inner )));
    }
    if (((t-> kind )  ==  TY_GENERIC)) {
        return mangle_generic((t-> name ), (t-> args ));
    }
    if (((t-> kind )  ==  TY_RESULT)) {
        return __glide_string_concat("result_", mangle_type((t-> inner )));
    }
    return "void";
}

bool   try_mono_call (CG*   g, Expr*   call, Type*   ret_hint) {
    if ((call  ==  NULL)) {
        return false;
    }
    if (((call-> kind )  !=  EX_CALL)) {
        return false;
    }
    Expr*   callee = (call-> lhs );
    const char*   gname = "";
    if ((callee  !=  NULL)) {
        if (((callee-> kind )  ==  EX_IDENT)) {
            (gname  =  (callee-> str_val ));
        }
        if (((callee-> kind )  ==  EX_PATH)) {
            (gname  =  __glide_string_concat(__glide_string_concat((callee-> str_val ), "_"), (callee-> field )));
        }
    }
    if ((__glide_string_len(gname)  ==  0)) {
        return false;
    }
    if ((!HashMap_contains__Stmt((g-> generic_fns ), gname))) {
        return false;
    }
    Stmt   template = HashMap_get__Stmt((g-> generic_fns ), gname);
    HashMap__Type*   bindings = HashMap_new__Type();
    if (((ret_hint  !=  NULL)  &&  ((template. fn_ret_ty )  !=  NULL))) {
        unify_for_inference((template. fn_ret_ty ), ret_hint, (template. type_params ), bindings);
    }
    if ((((call-> args )  !=  NULL)  &&  ((template. fn_params )  !=  NULL))) {
        int   n = Vector_len__Param((template. fn_params ));
        if ((Vector_len__Expr((call-> args ))  <  n)) {
            (n  =  Vector_len__Expr((call-> args )));
        }
        for (int   i = 0; (i  <  n); i++) {
            Param   p = Vector_get__Param((template. fn_params ), i);
            Expr   a = Vector_get__Expr((call-> args ), i);
            Type*   at = infer_for_codegen(g, (&a));
            if (((at  !=  NULL)  &&  ((p. ty )  !=  NULL))) {
                unify_for_inference((p. ty ), at, (template. type_params ), bindings);
            }
        }
    }
    Vector__Type*   derived = Vector_new__Type();
    bool   all = true;
    if (((template. type_params )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__string((template. type_params ))); i++) {
            const char*   tn = Vector_get__string((template. type_params ), i);
            if (HashMap_contains__Type(bindings, tn)) {
                Vector_push__Type(derived, HashMap_get__Type(bindings, tn));
            } else {
                (all  =  false);
            }
        }
    }
    if (((!all)  ||  (Vector_len__Type(derived)  ==  0))) {
        HashMap_free__Type(bindings);
        return false;
    }
    const char*   mangled = mangle_generic(gname, derived);
    ((callee-> kind )  =  EX_IDENT);
    ((callee-> str_val )  =  mangled);
    ((callee-> field )  =  "");
    if ((!HashMap_contains__bool((g-> emitted_monos ), mangled))) {
        HashMap_insert__bool((g-> emitted_monos ), mangled, true);
        FnMonoEntry   entry = (( FnMonoEntry ){. name  = (template. name ), . args  = derived});
        Vector_push__FnMonoEntry((g-> fn_mono_queue ), entry);
    }
    HashMap_free__Type(bindings);
    return true;
}

void   prescan_expr (CG*   g, Expr*   e, Type*   ret_hint) {
    if ((e  ==  NULL)) {
        return;
    }
    if (((e-> kind )  ==  EX_CALL)) {
        bool   _ok = try_mono_call(g, e, ret_hint);
    }
    if (((((e-> kind )  ==  EX_CALL)  &&  ((e-> lhs )  !=  NULL))  &&  (((e-> lhs )-> kind )  ==  EX_MEMBER))) {
        Expr*   recv = ((e-> lhs )-> lhs );
        const char*   method = ((e-> lhs )-> field );
        Type*   rt = infer_for_codegen(g, recv);
        Type*   stripped = strip_ptr(rt);
        if (((stripped  !=  NULL)  &&  ((stripped-> kind )  ==  TY_GENERIC))) {
            const char*   mname = __glide_string_concat(__glide_string_concat((stripped-> name ), "_"), method);
            if (HashMap_contains__Stmt((g-> generic_fns ), mname)) {
                Stmt   template = HashMap_get__Stmt((g-> generic_fns ), mname);
                const char*   mangled = mangle_generic(mname, (stripped-> args ));
                if ((!HashMap_contains__bool((g-> emitted_monos ), mangled))) {
                    HashMap_insert__bool((g-> emitted_monos ), mangled, true);
                    FnMonoEntry   entry = (( FnMonoEntry ){. name  = (template. name ), . args  = (stripped-> args )});
                    Vector_push__FnMonoEntry((g-> fn_mono_queue ), entry);
                }
            }
        }
    }
    if (((e-> lhs )  !=  NULL)) {
        prescan_expr(g, (e-> lhs ), NULL);
    }
    if (((e-> rhs )  !=  NULL)) {
        prescan_expr(g, (e-> rhs ), NULL);
    }
    if (((e-> operand )  !=  NULL)) {
        prescan_expr(g, (e-> operand ), NULL);
    }
    if (((e-> args )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
            Expr   a = Vector_get__Expr((e-> args ), i);
            prescan_expr(g, (&a), NULL);
        }
    }
}

void   prescan_stmt (CG*   g, Stmt*   s) {
    if ((s  ==  NULL)) {
        return;
    }
    if ((((s-> kind )  ==  ST_LET)  ||  ((s-> kind )  ==  ST_CONST))) {
        if (((s-> let_ty )  !=  NULL)) {
            CG_declare(g, (s-> name ), (s-> let_ty ));
        }
        if (((s-> let_value )  !=  NULL)) {
            prescan_expr(g, (s-> let_value ), (s-> let_ty ));
        }
        if (((((s-> kind )  ==  ST_LET)  &&  (s-> is_auto_owned ))  &&  ((s-> let_ty )  !=  NULL))) {
            Type*   inner_ty = strip_ptr((s-> let_ty ));
            if (((inner_ty  !=  NULL)  &&  ((inner_ty-> kind )  ==  TY_GENERIC))) {
                const char*   free_name = __glide_string_concat((inner_ty-> name ), "_free");
                if (HashMap_contains__Stmt((g-> generic_fns ), free_name)) {
                    const char*   mangled = mangle_generic(free_name, (inner_ty-> args ));
                    if ((!HashMap_contains__bool((g-> emitted_monos ), mangled))) {
                        HashMap_insert__bool((g-> emitted_monos ), mangled, true);
                        Stmt   template = HashMap_get__Stmt((g-> generic_fns ), free_name);
                        FnMonoEntry   entry = (( FnMonoEntry ){. name  = (template. name ), . args  = (inner_ty-> args )});
                        Vector_push__FnMonoEntry((g-> fn_mono_queue ), entry);
                    }
                }
            }
        }
        return;
    }
    if (((s-> expr_value )  !=  NULL)) {
        prescan_expr(g, (s-> expr_value ), NULL);
    }
    if (((s-> cond )  !=  NULL)) {
        prescan_expr(g, (s-> cond ), NULL);
    }
    if (((s-> for_init )  !=  NULL)) {
        prescan_stmt(g, (s-> for_init ));
    }
    if (((s-> for_step )  !=  NULL)) {
        prescan_expr(g, (s-> for_step ), NULL);
    }
    if (((s-> then_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> then_body ), i);
            prescan_stmt(g, (&b));
        }
    }
    if (((s-> else_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> else_body ), i);
            prescan_stmt(g, (&b));
        }
    }
    if (((s-> fn_body )  !=  NULL)) {
        HashMap_clear__Type((g-> scope ));
        if (((s-> fn_params )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Param((s-> fn_params ))); i++) {
                Param   p = Vector_get__Param((s-> fn_params ), i);
                CG_declare(g, (p. name ), (p. ty ));
            }
        }
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> fn_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> fn_body ), i);
            prescan_stmt(g, (&b));
        }
    }
    if (((s-> impl_methods )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> impl_methods ))); i++) {
            Stmt   m = Vector_get__Stmt((s-> impl_methods ), i);
            prescan_stmt(g, (&m));
        }
    }
}

void   emit_mono_forward_decl (CG*   g, const char*   mangled, Stmt*   template, Vector__Type*   args) {
    Type*   new_ret = subst_type((template-> fn_ret_ty ), (template-> type_params ), args);
    printf("%s %s %s %s", type_to_c(new_ret), " ", mangled, "(");
    if ((((template-> fn_params )  ==  NULL)  ||  (Vector_len__Param((template-> fn_params ))  ==  0))) {
        printf("%s", "void");
    } else {
        for (int   j = 0; (j  <  Vector_len__Param((template-> fn_params ))); j++) {
            if ((j  >  0)) {
                printf("%s", ", ");
            }
            Param   pp = Vector_get__Param((template-> fn_params ), j);
            Type*   pt = subst_type((pp. ty ), (template-> type_params ), args);
            printf("%s %s %s", type_to_c(pt), " ", (pp. name ));
        }
    }
    printf("%s\n", ");");
}

void   unify_for_inference (Type*   pat, Type*   actual, Vector__string*   params, HashMap__Type*   bindings) {
    if ((pat  ==  NULL)) {
        return;
    }
    if ((actual  ==  NULL)) {
        return;
    }
    if (((pat-> kind )  ==  TY_NAMED)) {
        if ((params  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__string(params)); i++) {
                if (__glide_string_eq((pat-> name ), Vector_get__string(params, i))) {
                    if ((!HashMap_contains__Type(bindings, (pat-> name )))) {
                        HashMap_insert__Type(bindings, (pat-> name ), (*actual));
                    }
                    return;
                }
            }
        }
    }
    if ((((pat-> kind )  ==  TY_POINTER)  &&  ((actual-> kind )  ==  TY_POINTER))) {
        unify_for_inference((pat-> inner ), (actual-> inner ), params, bindings);
        return;
    }
    if ((((pat-> kind )  ==  TY_BORROW)  &&  ((actual-> kind )  ==  TY_BORROW))) {
        unify_for_inference((pat-> inner ), (actual-> inner ), params, bindings);
        return;
    }
    if ((((pat-> kind )  ==  TY_BORROW_MUT)  &&  ((actual-> kind )  ==  TY_BORROW_MUT))) {
        unify_for_inference((pat-> inner ), (actual-> inner ), params, bindings);
        return;
    }
    if ((((pat-> kind )  ==  TY_SLICE)  &&  ((actual-> kind )  ==  TY_SLICE))) {
        unify_for_inference((pat-> inner ), (actual-> inner ), params, bindings);
        return;
    }
    if ((((pat-> kind )  ==  TY_GENERIC)  &&  ((actual-> kind )  ==  TY_GENERIC))) {
        if ((((__glide_string_eq((pat-> name ), (actual-> name ))  &&  ((pat-> args )  !=  NULL))  &&  ((actual-> args )  !=  NULL))  &&  (Vector_len__Type((pat-> args ))  ==  Vector_len__Type((actual-> args ))))) {
            for (int   i = 0; (i  <  Vector_len__Type((pat-> args ))); i++) {
                Type   pa = Vector_get__Type((pat-> args ), i);
                Type   aa = Vector_get__Type((actual-> args ), i);
                unify_for_inference((&pa), (&aa), params, bindings);
            }
        }
    }
}

Type*   subst_type (Type*   t, Vector__string*   params, Vector__Type*   args) {
    if ((t  ==  NULL)) {
        return NULL;
    }
    if (((t-> kind )  ==  TY_NAMED)) {
        if ((params  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__string(params)); i++) {
                if (__glide_string_eq((t-> name ), Vector_get__string(params, i))) {
                    Type   a = Vector_get__Type(args, i);
                    Type*   p = (( Type* )malloc(sizeof( Type )));
                    ((*p)  =  a);
                    return p;
                }
            }
        }
        return t;
    }
    if (((t-> kind )  ==  TY_POINTER)) {
        return ty_pointer(subst_type((t-> inner ), params, args));
    }
    if (((t-> kind )  ==  TY_BORROW)) {
        Type*   p = (( Type* )malloc(sizeof( Type )));
        ((p-> kind )  =  TY_BORROW);
        ((p-> inner )  =  subst_type((t-> inner ), params, args));
        return p;
    }
    if (((t-> kind )  ==  TY_BORROW_MUT)) {
        Type*   p = (( Type* )malloc(sizeof( Type )));
        ((p-> kind )  =  TY_BORROW_MUT);
        ((p-> inner )  =  subst_type((t-> inner ), params, args));
        return p;
    }
    if (((t-> kind )  ==  TY_SLICE)) {
        Type*   p = (( Type* )malloc(sizeof( Type )));
        ((p-> kind )  =  TY_SLICE);
        ((p-> inner )  =  subst_type((t-> inner ), params, args));
        return p;
    }
    if (((t-> kind )  ==  TY_GENERIC)) {
        Vector__Type*   new_args = Vector_new__Type();
        if (((t-> args )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Type((t-> args ))); i++) {
                Type   a = Vector_get__Type((t-> args ), i);
                Type*   na = subst_type((&a), params, args);
                if ((na  !=  NULL)) {
                    Vector_push__Type(new_args, (*na));
                }
            }
        }
        return ty_generic((t-> name ), new_args);
    }
    return t;
}

Expr*   subst_expr (Expr*   e, Vector__string*   params, Vector__Type*   args, HashMap__Stmt*   gs) {
    if ((e  ==  NULL)) {
        return NULL;
    }
    Expr*   n = (( Expr* )calloc(1, sizeof( Expr )));
    ((*n)  =  (*e));
    if (((e-> cast_to )  !=  NULL)) {
        ((n-> cast_to )  =  subst_type((e-> cast_to ), params, args));
    }
    if (((e-> lhs )  !=  NULL)) {
        ((n-> lhs )  =  subst_expr((e-> lhs ), params, args, gs));
    }
    if (((e-> rhs )  !=  NULL)) {
        ((n-> rhs )  =  subst_expr((e-> rhs ), params, args, gs));
    }
    if (((e-> operand )  !=  NULL)) {
        ((n-> operand )  =  subst_expr((e-> operand ), params, args, gs));
    }
    if (((e-> args )  !=  NULL)) {
        Vector__Expr*   new_args = Vector_new__Expr();
        for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
            Expr   a = Vector_get__Expr((e-> args ), i);
            Expr*   nu = subst_expr((&a), params, args, gs);
            if ((nu  !=  NULL)) {
                Vector_push__Expr(new_args, (*nu));
            }
        }
        ((n-> args )  =  new_args);
    }
    if (((((e-> kind )  ==  EX_STRUCT_LIT)  &&  (gs  !=  NULL))  &&  HashMap_contains__Stmt(gs, (e-> str_val )))) {
        Stmt   template = HashMap_get__Stmt(gs, (e-> str_val ));
        if (((((template. type_params )  !=  NULL)  &&  (args  !=  NULL))  &&  (Vector_len__string((template. type_params ))  ==  Vector_len__Type(args)))) {
            ((n-> str_val )  =  mangle_generic((e-> str_val ), args));
        }
    }
    return n;
}

Stmt*   subst_stmt (Stmt*   s, Vector__string*   params, Vector__Type*   args, HashMap__Stmt*   gs) {
    if ((s  ==  NULL)) {
        return NULL;
    }
    Stmt*   n = (( Stmt* )calloc(1, sizeof( Stmt )));
    ((*n)  =  (*s));
    if (((s-> let_ty )  !=  NULL)) {
        ((n-> let_ty )  =  subst_type((s-> let_ty ), params, args));
    }
    if (((s-> let_value )  !=  NULL)) {
        ((n-> let_value )  =  subst_expr((s-> let_value ), params, args, gs));
    }
    if (((s-> expr_value )  !=  NULL)) {
        ((n-> expr_value )  =  subst_expr((s-> expr_value ), params, args, gs));
    }
    if (((s-> cond )  !=  NULL)) {
        ((n-> cond )  =  subst_expr((s-> cond ), params, args, gs));
    }
    if (((s-> for_init )  !=  NULL)) {
        ((n-> for_init )  =  subst_stmt((s-> for_init ), params, args, gs));
    }
    if (((s-> for_step )  !=  NULL)) {
        ((n-> for_step )  =  subst_expr((s-> for_step ), params, args, gs));
    }
    if (((s-> fn_ret_ty )  !=  NULL)) {
        ((n-> fn_ret_ty )  =  subst_type((s-> fn_ret_ty ), params, args));
    }
    if (((s-> then_body )  !=  NULL)) {
        Vector__Stmt*   nb = Vector_new__Stmt();
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> then_body ), i);
            Stmt*   nu = subst_stmt((&b), params, args, gs);
            if ((nu  !=  NULL)) {
                Vector_push__Stmt(nb, (*nu));
            }
        }
        ((n-> then_body )  =  nb);
    }
    if (((s-> else_body )  !=  NULL)) {
        Vector__Stmt*   nb = Vector_new__Stmt();
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> else_body ), i);
            Stmt*   nu = subst_stmt((&b), params, args, gs);
            if ((nu  !=  NULL)) {
                Vector_push__Stmt(nb, (*nu));
            }
        }
        ((n-> else_body )  =  nb);
    }
    if (((s-> fn_body )  !=  NULL)) {
        Vector__Stmt*   nb = Vector_new__Stmt();
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> fn_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> fn_body ), i);
            Stmt*   nu = subst_stmt((&b), params, args, gs);
            if ((nu  !=  NULL)) {
                Vector_push__Stmt(nb, (*nu));
            }
        }
        ((n-> fn_body )  =  nb);
    }
    return n;
}

const char*   op_to_c (int   op) {
    if ((op  ==  OP_ADD)) {
        return "+";
    }
    if ((op  ==  OP_SUB)) {
        return "-";
    }
    if ((op  ==  OP_MUL)) {
        return "*";
    }
    if ((op  ==  OP_DIV)) {
        return "/";
    }
    if ((op  ==  OP_MOD)) {
        return "%";
    }
    if ((op  ==  OP_EQ)) {
        return "==";
    }
    if ((op  ==  OP_NE)) {
        return "!=";
    }
    if ((op  ==  OP_LT)) {
        return "<";
    }
    if ((op  ==  OP_LE)) {
        return "<=";
    }
    if ((op  ==  OP_GT)) {
        return ">";
    }
    if ((op  ==  OP_GE)) {
        return ">=";
    }
    if ((op  ==  OP_AND)) {
        return "&&";
    }
    if ((op  ==  OP_OR)) {
        return "||";
    }
    if ((op  ==  OP_BIT_AND)) {
        return "&";
    }
    if ((op  ==  OP_BIT_OR)) {
        return "|";
    }
    if ((op  ==  OP_BIT_XOR)) {
        return "^";
    }
    if ((op  ==  OP_SHL)) {
        return "<<";
    }
    if ((op  ==  OP_SHR)) {
        return ">>";
    }
    if ((op  ==  OP_ASSIGN)) {
        return "=";
    }
    if ((op  ==  OP_PLUS_ASSIGN)) {
        return "+=";
    }
    if ((op  ==  OP_MINUS_ASSIGN)) {
        return "-=";
    }
    if ((op  ==  OP_STAR_ASSIGN)) {
        return "*=";
    }
    if ((op  ==  OP_SLASH_ASSIGN)) {
        return "/=";
    }
    return "?";
}

const char*   unop_to_c (int   op) {
    if ((op  ==  UN_NEG)) {
        return "-";
    }
    if ((op  ==  UN_NOT)) {
        return "!";
    }
    if ((op  ==  UN_BIT_NOT)) {
        return "~";
    }
    if ((op  ==  UN_DEREF)) {
        return "*";
    }
    if ((op  ==  UN_ADDR)) {
        return "&";
    }
    if ((op  ==  UN_ADDR_MUT)) {
        return "&";
    }
    return "?";
}

void   ind (int   n) {
    for (int   i = 0; (i  <  n); i++) {
        printf("%s", "    ");
    }
}

void   emit_expr (CG*   g, Expr*   e) {
    if ((e  ==  NULL)) {
        printf("%s", "/* null */");
        return;
    }
    if (((e-> kind )  ==  EX_INT)) {
        printf("%d", (e-> int_val ));
        return;
    }
    if (((e-> kind )  ==  EX_FLOAT)) {
        printf("%d", (e-> int_val ));
        return;
    }
    if (((e-> kind )  ==  EX_STRING)) {
        printf("%s", "\"");
        printf("%s", (e-> str_val ));
        printf("%s", "\"");
        return;
    }
    if (((e-> kind )  ==  EX_CHAR)) {
        printf("%s", "(char)");
        printf("%d", (e-> int_val ));
        return;
    }
    if (((e-> kind )  ==  EX_BOOL)) {
        if ((e-> bool_val )) {
            printf("%s", "true");
        } else {
            printf("%s", "false");
        }
        return;
    }
    if (((e-> kind )  ==  EX_NULL)) {
        printf("%s", "NULL");
        return;
    }
    if (((e-> kind )  ==  EX_IDENT)) {
        printf("%s", (e-> str_val ));
        return;
    }
    if (((e-> kind )  ==  EX_BINARY)) {
        printf("%s", "(");
        emit_expr(g, (e-> lhs ));
        printf("%s %s %s", " ", op_to_c((e-> op_code )), " ");
        emit_expr(g, (e-> rhs ));
        printf("%s", ")");
        return;
    }
    if (((e-> kind )  ==  EX_UNARY)) {
        if (((e-> op_code )  ==  UN_TRY)) {
            Type*   inner_ty = infer_for_codegen(g, (e-> operand ));
            if (((((inner_ty  ==  NULL)  ||  ((inner_ty-> kind )  !=  TY_RESULT))  ||  ((g-> current_ret_ty )  ==  NULL))  ||  (((g-> current_ret_ty )-> kind )  !=  TY_RESULT))) {
                printf("%s", "/* invalid `?` */ 0");
                return;
            }
            const char*   m_in = mangle_type((inner_ty-> inner ));
            const char*   m_out = mangle_type(((g-> current_ret_ty )-> inner ));
            printf("%s", __glide_string_concat(__glide_string_concat("({ __glide_result_", m_in), "_t __r = ("));
            emit_expr(g, (e-> operand ));
            printf("%s", __glide_string_concat(__glide_string_concat("); if (!__r.ok) return __glide_err_", m_out), "(__r.err); __r.val; })"));
            return;
        }
        printf("%s", __glide_string_concat("(", unop_to_c((e-> op_code ))));
        emit_expr(g, (e-> operand ));
        printf("%s", ")");
        return;
    }
    if (((e-> kind )  ==  EX_ASSIGN)) {
        printf("%s", "(");
        emit_expr(g, (e-> lhs ));
        printf("%s %s %s", " ", op_to_c((e-> op_code )), " ");
        emit_expr(g, (e-> rhs ));
        printf("%s", ")");
        return;
    }
    if (((e-> kind )  ==  EX_CALL)) {
        if (((((e-> lhs )  !=  NULL)  &&  (((e-> lhs )-> kind )  ==  EX_PATH))  &&  HashMap_contains__Stmt((g-> enums ), ((e-> lhs )-> str_val )))) {
            const char*   ename = ((e-> lhs )-> str_val );
            const char*   vname = ((e-> lhs )-> field );
            printf("%s", "((");
            printf("%s", ename);
            printf("%s", "){.tag = ");
            printf("%s", __glide_string_concat(__glide_string_concat(ename, "_"), vname));
            if ((((e-> args )  !=  NULL)  &&  (Vector_len__Expr((e-> args ))  >  0))) {
                if ((Vector_len__Expr((e-> args ))  ==  1)) {
                    printf("%s", ", .data.v_");
                    printf("%s", vname);
                    printf("%s", " = ");
                    Expr   a = Vector_get__Expr((e-> args ), 0);
                    emit_expr(g, (&a));
                } else {
                    printf("%s", ", .data.v_");
                    printf("%s", vname);
                    printf("%s", " = {");
                    for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
                        if ((i  >  0)) {
                            printf("%s", ", ");
                        }
                        Expr   a = Vector_get__Expr((e-> args ), i);
                        emit_expr(g, (&a));
                    }
                    printf("%s", "}");
                }
            }
            printf("%s", "})");
            return;
        }
        if (((((e-> lhs )  !=  NULL)  &&  (((e-> lhs )-> kind )  ==  EX_IDENT))  &&  ((e-> args )  !=  NULL))) {
            const char*   cname = ((e-> lhs )-> str_val );
            if ((__glide_string_eq(cname, "send")  &&  (Vector_len__Expr((e-> args ))  ==  2))) {
                Expr   c0 = Vector_get__Expr((e-> args ), 0);
                Type*   ct = infer_for_codegen(g, (&c0));
                if ((((((ct  !=  NULL)  &&  ((ct-> kind )  ==  TY_GENERIC))  &&  __glide_string_eq((ct-> name ), "chan"))  &&  ((ct-> args )  !=  NULL))  &&  (Vector_len__Type((ct-> args ))  >  0))) {
                    Type   inner = Vector_get__Type((ct-> args ), 0);
                    const char*   m = mangle_type((&inner));
                    printf("%s", __glide_string_concat(__glide_string_concat("__glide_send_", m), "("));
                    emit_expr(g, (&c0));
                    printf("%s", ", ");
                    Expr   v = Vector_get__Expr((e-> args ), 1);
                    emit_expr(g, (&v));
                    printf("%s", ")");
                    return;
                }
            }
            if ((__glide_string_eq(cname, "recv")  &&  (Vector_len__Expr((e-> args ))  ==  1))) {
                Expr   c0 = Vector_get__Expr((e-> args ), 0);
                Type*   ct = infer_for_codegen(g, (&c0));
                if ((((((ct  !=  NULL)  &&  ((ct-> kind )  ==  TY_GENERIC))  &&  __glide_string_eq((ct-> name ), "chan"))  &&  ((ct-> args )  !=  NULL))  &&  (Vector_len__Type((ct-> args ))  >  0))) {
                    Type   inner = Vector_get__Type((ct-> args ), 0);
                    const char*   m = mangle_type((&inner));
                    printf("%s", __glide_string_concat(__glide_string_concat("__glide_recv_", m), "("));
                    emit_expr(g, (&c0));
                    printf("%s", ")");
                    return;
                }
            }
            if ((__glide_string_eq(cname, "close")  &&  (Vector_len__Expr((e-> args ))  ==  1))) {
                Expr   c0 = Vector_get__Expr((e-> args ), 0);
                Type*   ct = infer_for_codegen(g, (&c0));
                if ((((((ct  !=  NULL)  &&  ((ct-> kind )  ==  TY_GENERIC))  &&  __glide_string_eq((ct-> name ), "chan"))  &&  ((ct-> args )  !=  NULL))  &&  (Vector_len__Type((ct-> args ))  >  0))) {
                    Type   inner = Vector_get__Type((ct-> args ), 0);
                    const char*   m = mangle_type((&inner));
                    printf("%s", __glide_string_concat(__glide_string_concat("__glide_close_", m), "("));
                    emit_expr(g, (&c0));
                    printf("%s", ")");
                    return;
                }
            }
            if ((__glide_string_eq(cname, "unwrap")  &&  (Vector_len__Expr((e-> args ))  ==  1))) {
                Expr   r0 = Vector_get__Expr((e-> args ), 0);
                Type*   rt = infer_for_codegen(g, (&r0));
                if (((rt  !=  NULL)  &&  ((rt-> kind )  ==  TY_RESULT))) {
                    const char*   m = mangle_type((rt-> inner ));
                    printf("%s", __glide_string_concat(__glide_string_concat("__glide_unwrap_", m), "("));
                    emit_expr(g, (&r0));
                    printf("%s", ")");
                    return;
                }
            }
        }
        bool   _mono_ok = try_mono_call(g, e, NULL);
        if ((((e-> lhs )  !=  NULL)  &&  (((e-> lhs )-> kind )  ==  EX_MEMBER))) {
            Expr*   recv = ((e-> lhs )-> lhs );
            const char*   method = ((e-> lhs )-> field );
            Type*   recv_ty = infer_for_codegen(g, recv);
            Type*   stripped = strip_ptr(recv_ty);
            if (((stripped  !=  NULL)  &&  ((stripped-> kind )  ==  TY_GENERIC))) {
                const char*   base = (stripped-> name );
                const char*   mname = __glide_string_concat(__glide_string_concat(base, "_"), method);
                if (HashMap_contains__Stmt((g-> generic_fns ), mname)) {
                    Stmt   template = HashMap_get__Stmt((g-> generic_fns ), mname);
                    const char*   mangled = mangle_generic(mname, (stripped-> args ));
                    if ((!HashMap_contains__bool((g-> emitted_monos ), mangled))) {
                        HashMap_insert__bool((g-> emitted_monos ), mangled, true);
                        FnMonoEntry   entry = (( FnMonoEntry ){. name  = (template. name ), . args  = (stripped-> args )});
                        Vector_push__FnMonoEntry((g-> fn_mono_queue ), entry);
                    }
                    printf("%s", mangled);
                    printf("%s", "(");
                    bool   needs_addr = ((((recv_ty  !=  NULL)  &&  ((recv_ty-> kind )  !=  TY_POINTER))  &&  ((recv_ty-> kind )  !=  TY_BORROW))  &&  ((recv_ty-> kind )  !=  TY_BORROW_MUT));
                    if (needs_addr) {
                        printf("%s", "&");
                    }
                    emit_expr(g, recv);
                    if (((e-> args )  !=  NULL)) {
                        for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
                            printf("%s", ", ");
                            Expr   a = Vector_get__Expr((e-> args ), i);
                            emit_expr(g, (&a));
                        }
                    }
                    printf("%s", ")");
                    return;
                }
            }
            if (((stripped  !=  NULL)  &&  ((stripped-> kind )  ==  TY_NAMED))) {
                const char*   prefix = (stripped-> name );
                const char*   fn_name = "";
                if (is_stdlib_primitive((stripped-> name ))) {
                    (fn_name  =  __glide_string_concat(__glide_string_concat(__glide_string_concat("__glide_", prefix), "_"), method));
                } else {
                    (fn_name  =  __glide_string_concat(__glide_string_concat(prefix, "_"), method));
                }
                printf("%s", fn_name);
                printf("%s", "(");
                bool   is_prim = is_stdlib_primitive((stripped-> name ));
                bool   needs_addr = (((((!is_prim)  &&  (recv_ty  !=  NULL))  &&  ((recv_ty-> kind )  !=  TY_POINTER))  &&  ((recv_ty-> kind )  !=  TY_BORROW))  &&  ((recv_ty-> kind )  !=  TY_BORROW_MUT));
                if (needs_addr) {
                    printf("%s", "&");
                }
                emit_expr(g, recv);
                if (((e-> args )  !=  NULL)) {
                    for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
                        printf("%s", ", ");
                        Expr   a = Vector_get__Expr((e-> args ), i);
                        emit_expr(g, (&a));
                    }
                }
                printf("%s", ")");
                return;
            }
        }
        Type*   callee_ty = infer_for_codegen(g, (e-> lhs ));
        if (((callee_ty  !=  NULL)  &&  ((callee_ty-> kind )  ==  TY_FNPTR))) {
            printf("%s", "((");
            printf("%s", fnptr_cast_str(callee_ty));
            printf("%s", ")");
            emit_expr(g, (e-> lhs ));
            printf("%s", ")");
        } else {
            emit_expr(g, (e-> lhs ));
        }
        printf("%s", "(");
        if (((e-> args )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
                if ((i  >  0)) {
                    printf("%s", ", ");
                }
                Expr   a = Vector_get__Expr((e-> args ), i);
                emit_expr(g, (&a));
            }
        }
        printf("%s", ")");
        return;
    }
    if (((e-> kind )  ==  EX_MEMBER)) {
        Type*   recv_ty = infer_for_codegen(g, (e-> lhs ));
        if (((recv_ty  !=  NULL)  &&  ((((recv_ty-> kind )  ==  TY_POINTER)  ||  ((recv_ty-> kind )  ==  TY_BORROW))  ||  ((recv_ty-> kind )  ==  TY_BORROW_MUT)))) {
            printf("%s", "(");
            emit_expr(g, (e-> lhs ));
            printf("%s %s %s", "->", (e-> field ), ")");
            return;
        }
        printf("%s", "(");
        emit_expr(g, (e-> lhs ));
        printf("%s %s %s", ".", (e-> field ), ")");
        return;
    }
    if (((e-> kind )  ==  EX_INDEX)) {
        emit_expr(g, (e-> lhs ));
        printf("%s", "[");
        emit_expr(g, (e-> rhs ));
        printf("%s", "]");
        return;
    }
    if (((e-> kind )  ==  EX_CAST)) {
        printf("%s %s %s", "((", type_to_c((e-> cast_to )), ")");
        emit_expr(g, (e-> lhs ));
        printf("%s", ")");
        return;
    }
    if (((e-> kind )  ==  EX_SIZEOF)) {
        printf("%s %s %s", "sizeof(", type_to_c((e-> cast_to )), ")");
        return;
    }
    if (((e-> kind )  ==  EX_FNEXPR)) {
        int   id = (g-> anon_count );
        ((g-> anon_count )  =  (id  +  1));
        const char*   name = __glide_string_concat("__glide_anon_", int_to_str(id));
        AnonFn   af = (( AnonFn ){. name  = name, . params  = (e-> fn_expr_params ), . ret_ty  = (e-> cast_to ), . body  = (e-> fn_expr_body )});
        Vector_push__AnonFn((g-> anon_fns ), af);
        printf("%s", name);
        return;
    }
    if (((e-> kind )  ==  EX_PATH)) {
        printf("%s", __glide_string_concat(__glide_string_concat((e-> str_val ), "_"), (e-> field )));
        return;
    }
    if (((e-> kind )  ==  EX_POSTINC)) {
        emit_expr(g, (e-> lhs ));
        printf("%s", "++");
        return;
    }
    if (((e-> kind )  ==  EX_POSTDEC)) {
        emit_expr(g, (e-> lhs ));
        printf("%s", "--");
        return;
    }
    if (((e-> kind )  ==  EX_MACRO)) {
        if ((__glide_string_eq((e-> str_val ), "println")  ||  __glide_string_eq((e-> str_val ), "print"))) {
            printf("%s", "printf(\"");
            if (((e-> args )  !=  NULL)) {
                for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
                    if ((i  >  0)) {
                        printf("%s", " ");
                    }
                    Expr   a = Vector_get__Expr((e-> args ), i);
                    Type*   at = infer_for_codegen(g, (&a));
                    printf("%s", format_spec_for(at));
                }
            }
            if (__glide_string_eq((e-> str_val ), "println")) {
                printf("%s", "\\n");
            }
            printf("%s", "\"");
            if (((e-> args )  !=  NULL)) {
                for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
                    printf("%s", ", ");
                    Expr   a = Vector_get__Expr((e-> args ), i);
                    Type*   at = infer_for_codegen(g, (&a));
                    if ((((at  !=  NULL)  &&  ((at-> kind )  ==  TY_NAMED))  &&  __glide_string_eq((at-> name ), "bool"))) {
                        printf("%s", "__glide_bool_to_string(");
                        emit_expr(g, (&a));
                        printf("%s", ")");
                    } else {
                        emit_expr(g, (&a));
                    }
                }
            }
            printf("%s", ")");
            return;
        }
        if (__glide_string_eq((e-> str_val ), "format")) {
            printf("%s", "__glide_format(");
            if ((((e-> args )  !=  NULL)  &&  (Vector_len__Expr((e-> args ))  >  0))) {
                Expr   first = Vector_get__Expr((e-> args ), 0);
                if (((first. kind )  ==  EX_STRING)) {
                    printf("%s", "\"");
                    const char*   raw = (first. str_val );
                    int   arg_idx = 1;
                    int   n = __glide_string_len(raw);
                    int   i = 0;
                    while ((i  <  n)) {
                        char   c = __glide_string_at(raw, i);
                        if ((((__glide_char_to_int(c)  ==  123)  &&  ((i  +  1)  <  n))  &&  (__glide_char_to_int(__glide_string_at(raw, (i  +  1)))  ==  125))) {
                            if ((((e-> args )  !=  NULL)  &&  (arg_idx  <  Vector_len__Expr((e-> args ))))) {
                                Expr   a = Vector_get__Expr((e-> args ), arg_idx);
                                Type*   at = infer_for_codegen(g, (&a));
                                printf("%s", format_spec_for(at));
                            } else {
                                printf("%s", "%s");
                            }
                            (arg_idx  =  (arg_idx  +  1));
                            (i  =  (i  +  2));
                        } else {
                            if ((__glide_char_to_int(c)  ==  34)) {
                                printf("%s", "\\\"");
                            } else {
                                if ((__glide_char_to_int(c)  ==  92)) {
                                    printf("%s", "\\\\");
                                } else {
                                    printf("%c", c);
                                }
                            }
                            (i  =  (i  +  1));
                        }
                    }
                    printf("%s", "\"");
                } else {
                    emit_expr(g, (&first));
                }
                for (int   j = 1; (j  <  Vector_len__Expr((e-> args ))); j++) {
                    printf("%s", ", ");
                    Expr   a = Vector_get__Expr((e-> args ), j);
                    emit_expr(g, (&a));
                }
            }
            printf("%s", ")");
            return;
        }
        printf("%s %s %s", "/* macro ", (e-> str_val ), " */ 0");
        return;
    }
    if (((e-> kind )  ==  EX_STRUCT_LIT)) {
        printf("%s %s %s", "((", (e-> str_val ), "){");
        if ((((e-> field_names )  !=  NULL)  &&  ((e-> args )  !=  NULL))) {
            for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
                if ((i  >  0)) {
                    printf("%s", ", ");
                }
                printf("%s %s %s", ".", Vector_get__string((e-> field_names ), i), " = ");
                Expr   v = Vector_get__Expr((e-> args ), i);
                emit_expr(g, (&v));
            }
        }
        printf("%s", "})");
        return;
    }
    printf("%s %d %s", "/* unknown expr ", (e-> kind ), " */ 0");
}

void   emit_stmt (CG*   g, Stmt*   s, int   depth) {
    if ((s  ==  NULL)) {
        return;
    }
    if ((((s-> kind )  ==  ST_LET)  ||  ((s-> kind )  ==  ST_CONST))) {
        bool   struct_lit_auto = false;
        if (((((((s-> kind )  ==  ST_LET)  &&  ((s-> let_ty )  !=  NULL))  &&  (((s-> let_ty )-> kind )  ==  TY_POINTER))  &&  ((s-> let_value )  !=  NULL))  &&  (((s-> let_value )-> kind )  ==  EX_STRUCT_LIT))) {
            if (((((s-> let_ty )-> inner )  !=  NULL)  &&  ((((s-> let_ty )-> inner )-> kind )  ==  TY_NAMED))) {
                if (__glide_string_eq((((s-> let_ty )-> inner )-> name ), ((s-> let_value )-> str_val ))) {
                    (struct_lit_auto  =  true);
                }
            }
        }
        bool   auto_owned = (struct_lit_auto  ||  (s-> is_auto_owned ));
        if (((((s-> let_value )  !=  NULL)  &&  ((s-> let_ty )  !=  NULL))  &&  (((s-> let_ty )-> kind )  ==  TY_GENERIC))) {
            if (((((s-> let_value )-> kind )  ==  EX_STRUCT_LIT)  &&  __glide_string_eq(((s-> let_value )-> str_val ), ((s-> let_ty )-> name )))) {
                (((s-> let_value )-> str_val )  =  mangle_generic(((s-> let_ty )-> name ), ((s-> let_ty )-> args )));
            }
        }
        if ((((((((((((s-> let_value )  !=  NULL)  &&  ((s-> let_ty )  !=  NULL))  &&  (((s-> let_ty )-> kind )  ==  TY_GENERIC))  &&  __glide_string_eq(((s-> let_ty )-> name ), "chan"))  &&  (((s-> let_ty )-> args )  !=  NULL))  &&  (Vector_len__Type(((s-> let_ty )-> args ))  >  0))  &&  (((s-> let_value )-> kind )  ==  EX_CALL))  &&  (((s-> let_value )-> lhs )  !=  NULL))  &&  ((((s-> let_value )-> lhs )-> kind )  ==  EX_IDENT))  &&  __glide_string_eq((((s-> let_value )-> lhs )-> str_val ), "make_chan"))) {
            Type   inner = Vector_get__Type(((s-> let_ty )-> args ), 0);
            const char*   m = mangle_type((&inner));
            ((((s-> let_value )-> lhs )-> str_val )  =  __glide_string_concat("__glide_make_chan_", m));
        }
        if ((((((((s-> let_value )  !=  NULL)  &&  ((s-> let_ty )  !=  NULL))  &&  (((s-> let_ty )-> kind )  ==  TY_RESULT))  &&  (((s-> let_value )-> kind )  ==  EX_CALL))  &&  (((s-> let_value )-> lhs )  !=  NULL))  &&  ((((s-> let_value )-> lhs )-> kind )  ==  EX_IDENT))) {
            const char*   m = mangle_type(((s-> let_ty )-> inner ));
            if (__glide_string_eq((((s-> let_value )-> lhs )-> str_val ), "ok")) {
                ((((s-> let_value )-> lhs )-> str_val )  =  __glide_string_concat("__glide_ok_", m));
            }
            if (__glide_string_eq((((s-> let_value )-> lhs )-> str_val ), "err")) {
                ((((s-> let_value )-> lhs )-> str_val )  =  __glide_string_concat("__glide_err_", m));
            }
        }
        if ((((s-> let_value )  !=  NULL)  &&  (((s-> let_value )-> kind )  ==  EX_CALL))) {
            bool   _ok = try_mono_call(g, (s-> let_value ), (s-> let_ty ));
        }
        ind(depth);
        if (((s-> let_ty )  !=  NULL)) {
            printf("%s %s %s", type_to_c((s-> let_ty )), " ", (s-> name ));
            CG_declare(g, (s-> name ), (s-> let_ty ));
        } else {
            if (((s-> let_value )  !=  NULL)) {
                bool   handled = false;
                if ((((s-> let_value )-> kind )  ==  EX_IDENT)) {
                    for (int   k = 0; (k  <  Vector_len__AnonFn((g-> anon_fns ))); k++) {
                        AnonFn   af = Vector_get__AnonFn((g-> anon_fns ), k);
                        if (__glide_string_eq((af. name ), ((s-> let_value )-> str_val ))) {
                            printf("%s %s", "void* ", (s-> name ));
                            Type*   void_ptr = ty_pointer(ty_named("void"));
                            CG_declare(g, (s-> name ), void_ptr);
                            (handled  =  true);
                            break;
                        }
                    }
                }
                if ((!handled)) {
                    Type*   vt = infer_for_codegen(g, (s-> let_value ));
                    if ((vt  !=  NULL)) {
                        printf("%s %s %s", type_to_c(vt), " ", (s-> name ));
                        CG_declare(g, (s-> name ), vt);
                    } else {
                        printf("%s %s", "/* infer */ int ", (s-> name ));
                    }
                }
            } else {
                printf("%s %s", "/* infer */ int ", (s-> name ));
            }
        }
        if (struct_lit_auto) {
            printf("%s", " = (");
            printf("%s", type_to_c((s-> let_ty )));
            printf("%s", ")malloc(sizeof(");
            printf("%s", type_to_c(((s-> let_ty )-> inner )));
            printf("%s", "));");
            printf("%s\n", "");
            ind(depth);
            printf("%s", "*");
            printf("%s", (s-> name ));
            printf("%s", " = ");
            emit_expr(g, (s-> let_value ));
            printf("%s\n", ";");
        } else {
            if (((s-> let_value )  !=  NULL)) {
                printf("%s", " = ");
                emit_expr(g, (s-> let_value ));
            }
            printf("%s\n", ";");
        }
        if (auto_owned) {
            Type*   inner_ty = strip_ptr((s-> let_ty ));
            const char*   method_name = "";
            bool   has_method = false;
            if (((inner_ty  !=  NULL)  &&  ((inner_ty-> kind )  ==  TY_NAMED))) {
                (method_name  =  __glide_string_concat((inner_ty-> name ), "_free"));
                if (HashMap_contains__Stmt((g-> fns ), method_name)) {
                    (has_method  =  true);
                }
            }
            if (((inner_ty  !=  NULL)  &&  ((inner_ty-> kind )  ==  TY_GENERIC))) {
                (method_name  =  __glide_string_concat((inner_ty-> name ), "_free"));
                if (HashMap_contains__Stmt((g-> generic_fns ), method_name)) {
                    (has_method  =  true);
                }
            }
            if (has_method) {
                Expr*   recv = expr_ident((s-> name ), (s-> line ), (s-> column ));
                Expr*   mem = expr_member(recv, "free");
                Vector__Expr*   no_args = Vector_new__Expr();
                Expr*   call = expr_call(mem, no_args);
                Vector_push__Expr((g-> auto_drop_stack ), (*call));
            } else {
                Expr*   free_call = (( Expr* )calloc(1, sizeof( Expr )));
                ((free_call-> kind )  =  EX_CALL);
                ((free_call-> line )  =  (s-> line ));
                ((free_call-> column )  =  (s-> column ));
                Expr*   callee = expr_ident("free", (s-> line ), (s-> column ));
                ((free_call-> lhs )  =  callee);
                Vector__Expr*   args = Vector_new__Expr();
                Expr*   cast_arg = expr_cast(expr_ident((s-> name ), (s-> line ), (s-> column )), ty_pointer(ty_named("void")));
                Vector_push__Expr(args, (*cast_arg));
                ((free_call-> args )  =  args);
                Vector_push__Expr((g-> auto_drop_stack ), (*free_call));
            }
        }
        return;
    }
    if (((s-> kind )  ==  ST_RETURN)) {
        if ((((((((s-> expr_value )  !=  NULL)  &&  ((g-> current_ret_ty )  !=  NULL))  &&  (((g-> current_ret_ty )-> kind )  ==  TY_RESULT))  &&  (((s-> expr_value )-> kind )  ==  EX_CALL))  &&  (((s-> expr_value )-> lhs )  !=  NULL))  &&  ((((s-> expr_value )-> lhs )-> kind )  ==  EX_IDENT))) {
            const char*   m = mangle_type(((g-> current_ret_ty )-> inner ));
            if (__glide_string_eq((((s-> expr_value )-> lhs )-> str_val ), "ok")) {
                ((((s-> expr_value )-> lhs )-> str_val )  =  __glide_string_concat("__glide_ok_", m));
            }
            if (__glide_string_eq((((s-> expr_value )-> lhs )-> str_val ), "err")) {
                ((((s-> expr_value )-> lhs )-> str_val )  =  __glide_string_concat("__glide_err_", m));
            }
        }
        bool   has_cleanup = ((Vector_len__Expr((g-> defer_stack ))  >  0)  ||  (Vector_len__Expr((g-> auto_drop_stack ))  >  0));
        if ((((s-> expr_value )  !=  NULL)  &&  has_cleanup)) {
            ind(depth);
            printf("%s", "__typeof__(");
            emit_expr(g, (s-> expr_value ));
            printf("%s", ") __glide_ret = ");
            emit_expr(g, (s-> expr_value ));
            printf("%s\n", ";");
            CG_emit_deferred_reverse(g, depth);
            ind(depth);
            printf("%s\n", "return __glide_ret;");
        } else {
            CG_emit_deferred_reverse(g, depth);
            ind(depth);
            printf("%s", "return");
            if (((s-> expr_value )  !=  NULL)) {
                printf("%s", " ");
                emit_expr(g, (s-> expr_value ));
            }
            printf("%s\n", ";");
        }
        return;
    }
    if (((s-> kind )  ==  ST_DEFER)) {
        if (((s-> expr_value )  !=  NULL)) {
            Vector_push__Expr((g-> defer_stack ), (*(s-> expr_value )));
        }
        return;
    }
    if (((s-> kind )  ==  ST_SPAWN)) {
        int   id = (g-> spawn_count );
        ((g-> spawn_count )  =  (id  +  1));
        const char*   ids = int_to_str(id);
        const char*   an = __glide_string_concat("__glide_spawn_args_", ids);
        Expr*   call = (s-> expr_value );
        ind(depth);
        printf("%s\n", "{");
        ind((depth  +  1));
        printf("%s\n", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(an, "* __args = ("), an), "*)malloc(sizeof("), an), "));"));
        if (((call  !=  NULL)  &&  ((call-> args )  !=  NULL))) {
            for (int   i = 0; (i  <  Vector_len__Expr((call-> args ))); i++) {
                ind((depth  +  1));
                printf("%s", __glide_string_concat(__glide_string_concat("__args->a", int_to_str(i)), " = "));
                Expr   a = Vector_get__Expr((call-> args ), i);
                emit_expr(g, (&a));
                printf("%s\n", ";");
            }
        }
        ind((depth  +  1));
        printf("%s\n", "pthread_t __tid;");
        ind((depth  +  1));
        printf("%s\n", __glide_string_concat(__glide_string_concat("pthread_create(&__tid, NULL, __glide_spawn_stub_", ids), ", __args);"));
        ind((depth  +  1));
        printf("%s\n", "pthread_detach(__tid);");
        ind(depth);
        printf("%s\n", "}");
        return;
    }
    if (((s-> kind )  ==  ST_EXPR)) {
        ind(depth);
        emit_expr(g, (s-> expr_value ));
        printf("%s\n", ";");
        return;
    }
    if (((s-> kind )  ==  ST_BREAK)) {
        ind(depth);
        printf("%s\n", "break;");
        return;
    }
    if (((s-> kind )  ==  ST_CONTINUE)) {
        ind(depth);
        printf("%s\n", "continue;");
        return;
    }
    if (((s-> kind )  ==  ST_IF)) {
        ind(depth);
        printf("%s", "if (");
        emit_expr(g, (s-> cond ));
        printf("%s\n", ") {");
        if (((s-> then_body )  !=  NULL)) {
            int   saved = Vector_len__Expr((g-> auto_drop_stack ));
            for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
                Stmt   b = Vector_get__Stmt((s-> then_body ), i);
                emit_stmt(g, (&b), (depth  +  1));
            }
            CG_emit_block_drops(g, saved, (depth  +  1));
        }
        ind(depth);
        if (((s-> else_body )  !=  NULL)) {
            printf("%s\n", "} else {");
            int   saved = Vector_len__Expr((g-> auto_drop_stack ));
            for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
                Stmt   b = Vector_get__Stmt((s-> else_body ), i);
                emit_stmt(g, (&b), (depth  +  1));
            }
            CG_emit_block_drops(g, saved, (depth  +  1));
            ind(depth);
            printf("%s\n", "}");
        } else {
            printf("%s\n", "}");
        }
        return;
    }
    if (((s-> kind )  ==  ST_WHILE)) {
        ind(depth);
        printf("%s", "while (");
        emit_expr(g, (s-> cond ));
        printf("%s\n", ") {");
        if (((s-> then_body )  !=  NULL)) {
            int   saved = Vector_len__Expr((g-> auto_drop_stack ));
            for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
                Stmt   b = Vector_get__Stmt((s-> then_body ), i);
                emit_stmt(g, (&b), (depth  +  1));
            }
            CG_emit_block_drops(g, saved, (depth  +  1));
        }
        ind(depth);
        printf("%s\n", "}");
        return;
    }
    if (((s-> kind )  ==  ST_FOR)) {
        ind(depth);
        printf("%s", "for (");
        if (((s-> for_init )  !=  NULL)) {
            if ((((s-> for_init )-> kind )  ==  ST_LET)) {
                if ((((s-> for_init )-> let_ty )  !=  NULL)) {
                    printf("%s %s %s", type_to_c(((s-> for_init )-> let_ty )), " ", ((s-> for_init )-> name ));
                    CG_declare(g, ((s-> for_init )-> name ), ((s-> for_init )-> let_ty ));
                } else {
                    printf("%s %s", "int ", ((s-> for_init )-> name ));
                }
                if ((((s-> for_init )-> let_value )  !=  NULL)) {
                    printf("%s", " = ");
                    emit_expr(g, ((s-> for_init )-> let_value ));
                }
            } else {
                if ((((s-> for_init )-> expr_value )  !=  NULL)) {
                    emit_expr(g, ((s-> for_init )-> expr_value ));
                }
            }
        }
        printf("%s", "; ");
        if (((s-> cond )  !=  NULL)) {
            emit_expr(g, (s-> cond ));
        }
        printf("%s", "; ");
        if (((s-> for_step )  !=  NULL)) {
            emit_expr(g, (s-> for_step ));
        }
        printf("%s\n", ") {");
        if (((s-> then_body )  !=  NULL)) {
            int   saved = Vector_len__Expr((g-> auto_drop_stack ));
            for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
                Stmt   b = Vector_get__Stmt((s-> then_body ), i);
                emit_stmt(g, (&b), (depth  +  1));
            }
            CG_emit_block_drops(g, saved, (depth  +  1));
        }
        ind(depth);
        printf("%s\n", "}");
        return;
    }
    if (((s-> kind )  ==  ST_MATCH)) {
        ind(depth);
        printf("%s\n", "{");
        Type*   scrut_ty = infer_for_codegen(g, (s-> scrutinee ));
        const char*   enum_name = "X";
        if (((scrut_ty  !=  NULL)  &&  ((scrut_ty-> kind )  ==  TY_NAMED))) {
            (enum_name  =  (scrut_ty-> name ));
        }
        ind((depth  +  1));
        printf("%s %s", enum_name, " __m = ");
        emit_expr(g, (s-> scrutinee ));
        printf("%s\n", ";");
        ind((depth  +  1));
        printf("%s\n", "switch (__m.tag) {");
        if (((s-> arms )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__MatchArm((s-> arms ))); i++) {
                MatchArm   a = Vector_get__MatchArm((s-> arms ), i);
                ind((depth  +  1));
                if (__glide_string_eq((a. variant ), "_")) {
                    printf("%s", "default: ");
                } else {
                    printf("%s", "case ");
                    printf("%s", __glide_string_concat(__glide_string_concat(enum_name, "_"), (a. variant )));
                    printf("%s", ": ");
                }
                printf("%s\n", "{");
                if ((((a. bindings )  !=  NULL)  &&  (Vector_len__string((a. bindings ))  >  0))) {
                    if ((Vector_len__string((a. bindings ))  ==  1)) {
                        ind((depth  +  2));
                        const char*   payload = __glide_string_concat("__m.data.v_", (a. variant ));
                        printf("%s", "__typeof__(");
                        printf("%s", payload);
                        printf("%s", ") ");
                        printf("%s", Vector_get__string((a. bindings ), 0));
                        printf("%s", " = ");
                        printf("%s", payload);
                        printf("%s\n", ";");
                    } else {
                        for (int   k = 0; (k  <  Vector_len__string((a. bindings ))); k++) {
                            ind((depth  +  2));
                            const char*   payload = __glide_string_concat("__m.data.v_", (a. variant ));
                            printf("%s", "__typeof__(");
                            printf("%s", payload);
                            printf("%s", ".f");
                            printf("%d", k);
                            printf("%s", ") ");
                            printf("%s", Vector_get__string((a. bindings ), k));
                            printf("%s", " = ");
                            printf("%s", payload);
                            printf("%s", ".f");
                            printf("%d", k);
                            printf("%s\n", ";");
                        }
                    }
                }
                if (((a. body )  !=  NULL)) {
                    for (int   k = 0; (k  <  Vector_len__Stmt((a. body ))); k++) {
                        Stmt   bb = Vector_get__Stmt((a. body ), k);
                        emit_stmt(g, (&bb), (depth  +  2));
                    }
                }
                ind((depth  +  1));
                printf("%s\n", "break; }");
            }
        }
        ind((depth  +  1));
        printf("%s\n", "}");
        ind(depth);
        printf("%s\n", "}");
        return;
    }
    if (((s-> kind )  ==  ST_BLOCK)) {
        ind(depth);
        printf("%s\n", "{");
        if (((s-> then_body )  !=  NULL)) {
            int   saved = Vector_len__Expr((g-> auto_drop_stack ));
            for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
                Stmt   b = Vector_get__Stmt((s-> then_body ), i);
                emit_stmt(g, (&b), (depth  +  1));
            }
            CG_emit_block_drops(g, saved, (depth  +  1));
        }
        ind(depth);
        printf("%s\n", "}");
        return;
    }
    ind(depth);
    printf("%s %d %s\n", "/* unknown stmt ", (s-> kind ), " */");
}

void   emit_fn_signature (Stmt*   s) {
    if (__glide_string_eq((s-> name ), "main")) {
        printf("%s", "int main(int __glide_main_argc, char** __glide_main_argv)");
        return;
    }
    printf("%s %s %s %s", type_to_c((s-> fn_ret_ty )), " ", (s-> name ), "(");
    if ((((s-> fn_params )  ==  NULL)  ||  (Vector_len__Param((s-> fn_params ))  ==  0))) {
        printf("%s", "void");
    } else {
        for (int   i = 0; (i  <  Vector_len__Param((s-> fn_params ))); i++) {
            if ((i  >  0)) {
                printf("%s", ", ");
            }
            Param   p = Vector_get__Param((s-> fn_params ), i);
            printf("%s %s %s", type_to_c((p. ty )), " ", (p. name ));
        }
    }
    printf("%s", ")");
}

Type*   try_constrain_from_expr (CG*   g, Expr*   e, const char*   var, const char*   gen_name, Vector__string*   tparams) {
    if ((e  ==  NULL)) {
        return NULL;
    }
    if ((((((((e-> kind )  ==  EX_CALL)  &&  ((e-> lhs )  !=  NULL))  &&  (((e-> lhs )-> kind )  ==  EX_MEMBER))  &&  (((e-> lhs )-> lhs )  !=  NULL))  &&  ((((e-> lhs )-> lhs )-> kind )  ==  EX_IDENT))  &&  __glide_string_eq((((e-> lhs )-> lhs )-> str_val ), var))) {
        const char*   method = ((e-> lhs )-> field );
        const char*   mname = __glide_string_concat(__glide_string_concat(gen_name, "_"), method);
        if (HashMap_contains__Stmt((g-> generic_fns ), mname)) {
            Stmt   mtmpl = HashMap_get__Stmt((g-> generic_fns ), mname);
            if ((((mtmpl. fn_params )  !=  NULL)  &&  ((e-> args )  !=  NULL))) {
                Vector__Type*   resolved = Vector_new__Type();
                bool   all = true;
                for (int   pi = 0; (pi  <  Vector_len__string(tparams)); pi++) {
                    const char*   tp = Vector_get__string(tparams, pi);
                    Type*   found = NULL;
                    for (int   j = 1; (j  <  Vector_len__Param((mtmpl. fn_params ))); j++) {
                        Param   mp = Vector_get__Param((mtmpl. fn_params ), j);
                        if (((((mp. ty )  !=  NULL)  &&  (((mp. ty )-> kind )  ==  TY_NAMED))  &&  __glide_string_eq(((mp. ty )-> name ), tp))) {
                            int   arg_idx = (j  -  1);
                            if ((arg_idx  <  Vector_len__Expr((e-> args )))) {
                                Expr   a = Vector_get__Expr((e-> args ), arg_idx);
                                Type*   at = infer_for_codegen(g, (&a));
                                if ((at  !=  NULL)) {
                                    (found  =  at);
                                    break;
                                }
                            }
                        }
                    }
                    if ((found  ==  NULL)) {
                        (all  =  false);
                        break;
                    }
                    Vector_push__Type(resolved, (*found));
                }
                if ((all  &&  (Vector_len__Type(resolved)  >  0))) {
                    return ty_pointer(ty_generic(gen_name, resolved));
                }
            }
        }
    }
    if (((e-> lhs )  !=  NULL)) {
        Type*   r = try_constrain_from_expr(g, (e-> lhs ), var, gen_name, tparams);
        if ((r  !=  NULL)) {
            return r;
        }
    }
    if (((e-> rhs )  !=  NULL)) {
        Type*   r = try_constrain_from_expr(g, (e-> rhs ), var, gen_name, tparams);
        if ((r  !=  NULL)) {
            return r;
        }
    }
    if (((e-> operand )  !=  NULL)) {
        Type*   r = try_constrain_from_expr(g, (e-> operand ), var, gen_name, tparams);
        if ((r  !=  NULL)) {
            return r;
        }
    }
    if (((e-> args )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
            Expr   a = Vector_get__Expr((e-> args ), i);
            Type*   r = try_constrain_from_expr(g, (&a), var, gen_name, tparams);
            if ((r  !=  NULL)) {
                return r;
            }
        }
    }
    return NULL;
}

Type*   try_constrain_from_stmt (CG*   g, Stmt*   s, const char*   var, const char*   gen_name, Vector__string*   tparams) {
    if ((s  ==  NULL)) {
        return NULL;
    }
    if (((s-> let_value )  !=  NULL)) {
        Type*   r = try_constrain_from_expr(g, (s-> let_value ), var, gen_name, tparams);
        if ((r  !=  NULL)) {
            return r;
        }
    }
    if (((s-> expr_value )  !=  NULL)) {
        Type*   r = try_constrain_from_expr(g, (s-> expr_value ), var, gen_name, tparams);
        if ((r  !=  NULL)) {
            return r;
        }
    }
    if (((s-> cond )  !=  NULL)) {
        Type*   r = try_constrain_from_expr(g, (s-> cond ), var, gen_name, tparams);
        if ((r  !=  NULL)) {
            return r;
        }
    }
    if (((s-> for_step )  !=  NULL)) {
        Type*   r = try_constrain_from_expr(g, (s-> for_step ), var, gen_name, tparams);
        if ((r  !=  NULL)) {
            return r;
        }
    }
    if (((s-> then_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> then_body ), i);
            Type*   r = try_constrain_from_stmt(g, (&b), var, gen_name, tparams);
            if ((r  !=  NULL)) {
                return r;
            }
        }
    }
    if (((s-> else_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> else_body ), i);
            Type*   r = try_constrain_from_stmt(g, (&b), var, gen_name, tparams);
            if ((r  !=  NULL)) {
                return r;
            }
        }
    }
    return NULL;
}

void   populate_scope_from_stmt (CG*   g, Stmt*   s) {
    if ((s  ==  NULL)) {
        return;
    }
    if (((((s-> kind )  ==  ST_LET)  ||  ((s-> kind )  ==  ST_CONST))  &&  ((s-> let_ty )  !=  NULL))) {
        CG_declare(g, (s-> name ), (s-> let_ty ));
    }
    if (((s-> for_init )  !=  NULL)) {
        populate_scope_from_stmt(g, (s-> for_init ));
    }
    if (((s-> then_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> then_body ), i);
            populate_scope_from_stmt(g, (&b));
        }
    }
    if (((s-> else_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> else_body ), i);
            populate_scope_from_stmt(g, (&b));
        }
    }
}

Type*   try_resolve_open_let (CG*   g, Stmt*   s, Vector__Stmt*   body, int   start) {
    if (((s-> let_ty )  !=  NULL)) {
        return NULL;
    }
    if ((((s-> let_value )  ==  NULL)  ||  (((s-> let_value )-> kind )  !=  EX_CALL))) {
        return NULL;
    }
    Expr*   callee = ((s-> let_value )-> lhs );
    if ((callee  ==  NULL)) {
        return NULL;
    }
    const char*   gname = "";
    if (((callee-> kind )  ==  EX_IDENT)) {
        (gname  =  (callee-> str_val ));
    }
    if (((callee-> kind )  ==  EX_PATH)) {
        (gname  =  __glide_string_concat(__glide_string_concat((callee-> str_val ), "_"), (callee-> field )));
    }
    if ((__glide_string_len(gname)  ==  0)) {
        return NULL;
    }
    if ((!HashMap_contains__Stmt((g-> generic_fns ), gname))) {
        return NULL;
    }
    Stmt   template = HashMap_get__Stmt((g-> generic_fns ), gname);
    if ((((template. type_params )  ==  NULL)  ||  (Vector_len__string((template. type_params ))  ==  0))) {
        return NULL;
    }
    if (((template. fn_ret_ty )  ==  NULL)) {
        return NULL;
    }
    Type*   inner = (template. fn_ret_ty );
    while (((inner  !=  NULL)  &&  ((inner-> kind )  ==  TY_POINTER))) {
        (inner  =  (inner-> inner ));
    }
    if (((inner  ==  NULL)  ||  ((inner-> kind )  !=  TY_GENERIC))) {
        return NULL;
    }
    const char*   gen_base = (inner-> name );
    int   n = Vector_len__Stmt(body);
    for (int   j = (start  +  1); (j  <  n); j++) {
        Stmt   s2 = Vector_get__Stmt(body, j);
        Type*   r = try_constrain_from_stmt(g, (&s2), (s-> name ), gen_base, (template. type_params ));
        if ((r  !=  NULL)) {
            return r;
        }
    }
    return NULL;
}

void   infer_let_types (CG*   g, Vector__Stmt*   body) {
    if ((body  ==  NULL)) {
        return;
    }
    for (int   k = 0; (k  <  Vector_len__Stmt(body)); k++) {
        Stmt   sk = Vector_get__Stmt(body, k);
        populate_scope_from_stmt(g, (&sk));
    }
    int   n = Vector_len__Stmt(body);
    for (int   i = 0; (i  <  n); i++) {
        Stmt   s = Vector_get__Stmt(body, i);
        if (((s. kind )  ==  ST_LET)) {
            Type*   resolved = try_resolve_open_let(g, (&s), body, i);
            if ((resolved  !=  NULL)) {
                ((s. let_ty )  =  resolved);
                Vector_set__Stmt(body, i, s);
                CG_declare(g, (s. name ), resolved);
            }
        }
        if (((s. then_body )  !=  NULL)) {
            infer_let_types(g, (s. then_body ));
        }
        if (((s. else_body )  !=  NULL)) {
            infer_let_types(g, (s. else_body ));
        }
    }
}

void   infer_let_types_in_program (CG*   g, Vector__Stmt*   program) {
    int   n = Vector_len__Stmt(program);
    for (int   i = 0; (i  <  n); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if ((((s. kind )  ==  ST_FN)  &&  ((s. fn_body )  !=  NULL))) {
            HashMap_clear__Type((g-> scope ));
            if (((s. fn_params )  !=  NULL)) {
                for (int   k = 0; (k  <  Vector_len__Param((s. fn_params ))); k++) {
                    Param   p = Vector_get__Param((s. fn_params ), k);
                    CG_declare(g, (p. name ), (p. ty ));
                }
            }
            infer_let_types(g, (s. fn_body ));
        }
        if ((((s. kind )  ==  ST_IMPL)  &&  ((s. impl_methods )  !=  NULL))) {
            for (int   j = 0; (j  <  Vector_len__Stmt((s. impl_methods ))); j++) {
                Stmt   m = Vector_get__Stmt((s. impl_methods ), j);
                if (((m. fn_body )  !=  NULL)) {
                    HashMap_clear__Type((g-> scope ));
                    if (((m. fn_params )  !=  NULL)) {
                        for (int   k = 0; (k  <  Vector_len__Param((m. fn_params ))); k++) {
                            Param   p = Vector_get__Param((m. fn_params ), k);
                            CG_declare(g, (p. name ), (p. ty ));
                        }
                    }
                    infer_let_types(g, (m. fn_body ));
                }
            }
        }
    }
    HashMap_clear__Type((g-> scope ));
}

void   emit_fn (CG*   g, Stmt*   s) {
    emit_fn_signature(s);
    printf("%s\n", " {");
    if (__glide_string_eq((s-> name ), "main")) {
        printf("%s\n", "    __glide_args_init(__glide_main_argc, __glide_main_argv);");
    }
    HashMap_clear__Type((g-> scope ));
    Type*   saved_ret = (g-> current_ret_ty );
    ((g-> current_ret_ty )  =  (s-> fn_ret_ty ));
    Vector__Expr*   saved_defer = (g-> defer_stack );
    Vector__Expr*   saved_drop = (g-> auto_drop_stack );
    Vector__Expr*   new_defer = Vector_new__Expr();
    Vector__Expr*   new_drop = Vector_new__Expr();
    ((g-> defer_stack )  =  new_defer);
    ((g-> auto_drop_stack )  =  new_drop);
    if (((s-> fn_params )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Param((s-> fn_params ))); i++) {
            Param   p = Vector_get__Param((s-> fn_params ), i);
            CG_declare(g, (p. name ), (p. ty ));
        }
    }
    if (((s-> fn_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> fn_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> fn_body ), i);
            emit_stmt(g, (&b), 1);
        }
    }
    CG_emit_deferred_reverse(g, 1);
    ((g-> defer_stack )  =  saved_defer);
    ((g-> auto_drop_stack )  =  saved_drop);
    ((g-> current_ret_ty )  =  saved_ret);
    printf("%s\n", "}");
}

void   emit_impl (CG*   g, Stmt*   s) {
    if (((s-> impl_methods )  ==  NULL)) {
        return;
    }
    const char*   prefix = "X";
    if ((((s-> impl_target )  !=  NULL)  &&  (((s-> impl_target )-> kind )  ==  TY_NAMED))) {
        (prefix  =  ((s-> impl_target )-> name ));
    }
    for (int   i = 0; (i  <  Vector_len__Stmt((s-> impl_methods ))); i++) {
        Stmt   m = Vector_get__Stmt((s-> impl_methods ), i);
        const char*   mangled = __glide_string_concat(__glide_string_concat(prefix, "_"), (m. name ));
        const char*   saved_name = (m. name );
        printf("%s %s %s %s", type_to_c((m. fn_ret_ty )), " ", mangled, "(");
        if ((((m. fn_params )  ==  NULL)  ||  (Vector_len__Param((m. fn_params ))  ==  0))) {
            printf("%s", "void");
        } else {
            for (int   j = 0; (j  <  Vector_len__Param((m. fn_params ))); j++) {
                if ((j  >  0)) {
                    printf("%s", ", ");
                }
                Param   p = Vector_get__Param((m. fn_params ), j);
                printf("%s %s %s", type_to_c((p. ty )), " ", (p. name ));
            }
        }
        printf("%s\n", ") {");
        HashMap_clear__Type((g-> scope ));
        Vector__Expr*   saved_defer = (g-> defer_stack );
        Vector__Expr*   saved_drop = (g-> auto_drop_stack );
        Vector__Expr*   nd = Vector_new__Expr();
        Vector__Expr*   nadr = Vector_new__Expr();
        ((g-> defer_stack )  =  nd);
        ((g-> auto_drop_stack )  =  nadr);
        if (((m. fn_params )  !=  NULL)) {
            for (int   j = 0; (j  <  Vector_len__Param((m. fn_params ))); j++) {
                Param   p = Vector_get__Param((m. fn_params ), j);
                CG_declare(g, (p. name ), (p. ty ));
            }
        }
        if (((m. fn_body )  !=  NULL)) {
            for (int   j = 0; (j  <  Vector_len__Stmt((m. fn_body ))); j++) {
                Stmt   b = Vector_get__Stmt((m. fn_body ), j);
                emit_stmt(g, (&b), 1);
            }
        }
        CG_emit_deferred_reverse(g, 1);
        ((g-> defer_stack )  =  saved_defer);
        ((g-> auto_drop_stack )  =  saved_drop);
        printf("%s\n", "}");
        printf("%s\n", "");
    }
}

void   emit_struct (Stmt*   s) {
    printf("%s %s %s\n", "typedef struct ", (s-> name ), " {");
    if (((s-> struct_fields )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Field((s-> struct_fields ))); i++) {
            Field   f = Vector_get__Field((s-> struct_fields ), i);
            printf("%s %s %s %s", "    ", type_to_c((f. ty )), " ", (f. name ));
            printf("%s\n", ";");
        }
    }
    printf("%s %s %s\n", "} ", (s-> name ), ";");
    printf("%s\n", "");
}

void   emit_enum (Stmt*   s) {
    bool   has_payload = false;
    if (((s-> enum_variants )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__EnumVariant((s-> enum_variants ))); i++) {
            EnumVariant   v = Vector_get__EnumVariant((s-> enum_variants ), i);
            if ((((v. fields )  !=  NULL)  &&  (Vector_len__Type((v. fields ))  >  0))) {
                (has_payload  =  true);
            }
        }
    }
    printf("%s %s %s\n", "typedef struct ", (s-> name ), " {");
    printf("%s\n", "    int tag;");
    if (has_payload) {
        printf("%s\n", "    union {");
        if (((s-> enum_variants )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__EnumVariant((s-> enum_variants ))); i++) {
                EnumVariant   v = Vector_get__EnumVariant((s-> enum_variants ), i);
                if ((((v. fields )  ==  NULL)  ||  (Vector_len__Type((v. fields ))  ==  0))) {
                    continue;
                }
                if ((Vector_len__Type((v. fields ))  ==  1)) {
                    Type   t = Vector_get__Type((v. fields ), 0);
                    printf("%s", "        ");
                    printf("%s", type_to_c((&t)));
                    printf("%s", " v_");
                    printf("%s", (v. name ));
                    printf("%s\n", ";");
                } else {
                    printf("%s", "        struct { ");
                    for (int   j = 0; (j  <  Vector_len__Type((v. fields ))); j++) {
                        if ((j  >  0)) {
                            printf("%s", " ");
                        }
                        Type   t = Vector_get__Type((v. fields ), j);
                        printf("%s", type_to_c((&t)));
                        printf("%s", " f");
                        printf("%d", j);
                        printf("%s", ";");
                    }
                    printf("%s", " } v_");
                    printf("%s", (v. name ));
                    printf("%s\n", ";");
                }
            }
        }
        printf("%s\n", "    } data;");
    }
    printf("%s %s %s\n", "} ", (s-> name ), ";");
    if (((s-> enum_variants )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__EnumVariant((s-> enum_variants ))); i++) {
            EnumVariant   v = Vector_get__EnumVariant((s-> enum_variants ), i);
            printf("%s", "#define ");
            printf("%s", (s-> name ));
            printf("%s", "_");
            printf("%s", (v. name ));
            printf("%s", " ");
            printf("%d", i);
            printf("%s\n", "");
        }
    }
    printf("%s\n", "");
}

void   emit_program (Vector__Stmt*   program) {
    printf("%s\n", "#include <stdio.h>");
    printf("%s\n", "#include <stdlib.h>");
    printf("%s\n", "#include <stdbool.h>");
    printf("%s\n", "#include <stdint.h>");
    printf("%s\n", "#include <stddef.h>");
    printf("%s\n", "#include <string.h>");
    printf("%s\n", "");
    emit_stdlib_runtime();
    CG*   g = CG_new();
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        collect_chan_in_stmt(g, (&s));
    }
    emit_chan_runtime(g);
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        collect_result_in_stmt(g, (&s));
    }
    emit_result_runtime(g);
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if (((s. kind )  ==  ST_STRUCT)) {
            if ((((s. type_params )  !=  NULL)  &&  (Vector_len__string((s. type_params ))  >  0))) {
                HashMap_insert__Stmt((g-> generic_structs ), (s. name ), s);
            } else {
                HashMap_insert__Stmt((g-> structs ), (s. name ), s);
            }
        }
        if (((s. kind )  ==  ST_FN)) {
            if ((((s. type_params )  !=  NULL)  &&  (Vector_len__string((s. type_params ))  >  0))) {
                HashMap_insert__Stmt((g-> generic_fns ), (s. name ), s);
            } else {
                HashMap_insert__Stmt((g-> fns ), (s. name ), s);
            }
        }
        if (((s. kind )  ==  ST_ENUM)) {
            HashMap_insert__Stmt((g-> enums ), (s. name ), s);
        }
        if ((((((s. kind )  ==  ST_IMPL)  &&  (((s. type_params )  ==  NULL)  ||  (Vector_len__string((s. type_params ))  ==  0)))  &&  ((s. impl_target )  !=  NULL))  &&  ((s. impl_methods )  !=  NULL))) {
            const char*   prefix = "X";
            if ((((s. impl_target )-> kind )  ==  TY_NAMED)) {
                (prefix  =  ((s. impl_target )-> name ));
            }
            for (int   j = 0; (j  <  Vector_len__Stmt((s. impl_methods ))); j++) {
                Stmt   m = Vector_get__Stmt((s. impl_methods ), j);
                Stmt*   synth = (( Stmt* )calloc(1, sizeof( Stmt )));
                ((*synth)  =  m);
                ((synth-> name )  =  __glide_string_concat(__glide_string_concat(prefix, "_"), (m. name )));
                HashMap_insert__Stmt((g-> fns ), (synth-> name ), (*synth));
            }
        }
        if (((((((s. kind )  ==  ST_IMPL)  &&  ((s. type_params )  !=  NULL))  &&  (Vector_len__string((s. type_params ))  >  0))  &&  ((s. impl_target )  !=  NULL))  &&  ((s. impl_methods )  !=  NULL))) {
            const char*   prefix = "X";
            if ((((s. impl_target )-> kind )  ==  TY_NAMED)) {
                (prefix  =  ((s. impl_target )-> name ));
            }
            if ((((s. impl_target )-> kind )  ==  TY_GENERIC)) {
                (prefix  =  ((s. impl_target )-> name ));
            }
            for (int   j = 0; (j  <  Vector_len__Stmt((s. impl_methods ))); j++) {
                Stmt   m = Vector_get__Stmt((s. impl_methods ), j);
                const char*   mname = __glide_string_concat(__glide_string_concat(prefix, "_"), (m. name ));
                Stmt*   synth = (( Stmt* )calloc(1, sizeof( Stmt )));
                ((*synth)  =  m);
                ((synth-> name )  =  mname);
                Vector__string*   combined = Vector_new__string();
                for (int   k = 0; (k  <  Vector_len__string((s. type_params ))); k++) {
                    Vector_push__string(combined, Vector_get__string((s. type_params ), k));
                }
                if (((m. type_params )  !=  NULL)) {
                    for (int   k = 0; (k  <  Vector_len__string((m. type_params ))); k++) {
                        Vector_push__string(combined, Vector_get__string((m. type_params ), k));
                    }
                }
                ((synth-> type_params )  =  combined);
                HashMap_insert__Stmt((g-> generic_fns ), mname, (*synth));
            }
        }
    }
    infer_let_types_in_program(g, program);
    Vector__Type*   monos = Vector_new__Type();
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        collect_generic_uses_in_stmt((&s), monos);
    }
    for (int   i = 0; (i  <  Vector_len__Type(monos)); i++) {
        Type   t = Vector_get__Type(monos, i);
        const char*   mangled = mangle_generic((t. name ), (t. args ));
        if ((!HashMap_contains__bool((g-> emitted_monos ), mangled))) {
            HashMap_insert__bool((g-> emitted_monos ), mangled, true);
            printf("%s %s %s %s %s\n", "typedef struct ", mangled, " ", mangled, ";");
        }
    }
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if ((((s. kind )  ==  ST_STRUCT)  &&  (((s. type_params )  ==  NULL)  ||  (Vector_len__string((s. type_params ))  ==  0)))) {
            printf("%s %s %s %s %s\n", "typedef struct ", (s. name ), " ", (s. name ), ";");
        }
    }
    printf("%s\n", "");
    for (int   i = 0; (i  <  Vector_len__Type(monos)); i++) {
        Type   t = Vector_get__Type(monos, i);
        emit_struct_mono(g, (&t));
    }
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if ((((s. kind )  ==  ST_STRUCT)  &&  (((s. type_params )  ==  NULL)  ||  (Vector_len__string((s. type_params ))  ==  0)))) {
            emit_struct((&s));
        }
        if (((s. kind )  ==  ST_ENUM)) {
            emit_enum((&s));
        }
    }
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if (((s. kind )  ==  ST_CONST)) {
            emit_top_const(g, (&s));
        }
    }
    printf("%s\n", "");
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if ((((s. kind )  ==  ST_FN)  &&  (((s. type_params )  ==  NULL)  ||  (Vector_len__string((s. type_params ))  ==  0)))) {
            emit_fn_signature((&s));
            printf("%s\n", ";");
        }
    }
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if ((((s. kind )  ==  ST_FN)  &&  (((s. type_params )  ==  NULL)  ||  (Vector_len__string((s. type_params ))  ==  0)))) {
            prescan_stmt(g, (&s));
        }
        if ((((s. kind )  ==  ST_IMPL)  &&  (((s. type_params )  ==  NULL)  ||  (Vector_len__string((s. type_params ))  ==  0)))) {
            if (((s. impl_methods )  !=  NULL)) {
                for (int   j = 0; (j  <  Vector_len__Stmt((s. impl_methods ))); j++) {
                    Stmt   m = Vector_get__Stmt((s. impl_methods ), j);
                    prescan_stmt(g, (&m));
                }
            }
        }
    }
    int   idx = 0;
    while ((idx  <  Vector_len__FnMonoEntry((g-> fn_mono_queue )))) {
        FnMonoEntry   entry = Vector_get__FnMonoEntry((g-> fn_mono_queue ), idx);
        if (HashMap_contains__Stmt((g-> generic_fns ), (entry. name ))) {
            Stmt   template = HashMap_get__Stmt((g-> generic_fns ), (entry. name ));
            HashMap_clear__Type((g-> scope ));
            if (((template. fn_params )  !=  NULL)) {
                for (int   j = 0; (j  <  Vector_len__Param((template. fn_params ))); j++) {
                    Param   p = Vector_get__Param((template. fn_params ), j);
                    Type*   pt = subst_type((p. ty ), (template. type_params ), (entry. args ));
                    CG_declare(g, (p. name ), pt);
                }
            }
            if (((template. fn_body )  !=  NULL)) {
                for (int   j = 0; (j  <  Vector_len__Stmt((template. fn_body ))); j++) {
                    Stmt   b = Vector_get__Stmt((template. fn_body ), j);
                    Stmt*   nb = subst_stmt((&b), (template. type_params ), (entry. args ), (g-> generic_structs ));
                    if ((nb  !=  NULL)) {
                        prescan_stmt(g, nb);
                    }
                }
            }
        }
        (idx  =  (idx  +  1));
    }
    for (int   i = 0; (i  <  Vector_len__FnMonoEntry((g-> fn_mono_queue ))); i++) {
        FnMonoEntry   entry = Vector_get__FnMonoEntry((g-> fn_mono_queue ), i);
        if (HashMap_contains__Stmt((g-> generic_fns ), (entry. name ))) {
            Stmt   template = HashMap_get__Stmt((g-> generic_fns ), (entry. name ));
            const char*   mangled = mangle_generic((entry. name ), (entry. args ));
            emit_mono_forward_decl(g, mangled, (&template), (entry. args ));
        }
    }
    printf("%s\n", "");
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        lift_anons_in_stmt(g, (&s));
    }
    for (int   i = 0; (i  <  Vector_len__AnonFn((g-> anon_fns ))); i++) {
        AnonFn   af = Vector_get__AnonFn((g-> anon_fns ), i);
        printf("%s %s %s %s", type_to_c((af. ret_ty )), " ", (af. name ), "(");
        if ((((af. params )  ==  NULL)  ||  (Vector_len__Param((af. params ))  ==  0))) {
            printf("%s", "void");
        } else {
            for (int   j = 0; (j  <  Vector_len__Param((af. params ))); j++) {
                if ((j  >  0)) {
                    printf("%s", ", ");
                }
                Param   p = Vector_get__Param((af. params ), j);
                printf("%s %s %s", type_to_c((p. ty )), " ", (p. name ));
            }
        }
        printf("%s\n", ");");
    }
    printf("%s\n", "");
    ((g-> spawn_count )  =  0);
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        pre_emit_spawn_stubs_in_stmt(g, (&s));
    }
    ((g-> spawn_count )  =  0);
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if (((((s. kind )  ==  ST_FN)  &&  (((s. type_params )  ==  NULL)  ||  (Vector_len__string((s. type_params ))  ==  0)))  &&  ((s. fn_body )  !=  NULL))) {
            emit_fn(g, (&s));
            printf("%s\n", "");
        }
        if ((((s. kind )  ==  ST_IMPL)  &&  (((s. type_params )  ==  NULL)  ||  (Vector_len__string((s. type_params ))  ==  0)))) {
            emit_impl(g, (&s));
        }
    }
    while ((Vector_len__FnMonoEntry((g-> fn_mono_queue ))  >  0)) {
        FnMonoEntry   e = Vector_pop__FnMonoEntry((g-> fn_mono_queue ));
        if ((!HashMap_contains__Stmt((g-> generic_fns ), (e. name )))) {
            continue;
        }
        Stmt   template = HashMap_get__Stmt((g-> generic_fns ), (e. name ));
        const char*   mangled = mangle_generic((e. name ), (e. args ));
        Vector__Param*   new_params_v = Vector_new__Param();
        if (((template. fn_params )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Param((template. fn_params ))); i++) {
                Param   p = Vector_get__Param((template. fn_params ), i);
                Param   np = (( Param ){. name  = (p. name ), . ty  = subst_type((p. ty ), (template. type_params ), (e. args ))});
                Vector_push__Param(new_params_v, np);
            }
        }
        Type*   new_ret = subst_type((template. fn_ret_ty ), (template. type_params ), (e. args ));
        Vector__Stmt*   new_body = Vector_new__Stmt();
        if (((template. fn_body )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Stmt((template. fn_body ))); i++) {
                Stmt   b = Vector_get__Stmt((template. fn_body ), i);
                Stmt*   nb = subst_stmt((&b), (template. type_params ), (e. args ), (g-> generic_structs ));
                if ((nb  !=  NULL)) {
                    Vector_push__Stmt(new_body, (*nb));
                }
            }
        }
        printf("%s %s %s %s", type_to_c(new_ret), " ", mangled, "(");
        if ((Vector_len__Param(new_params_v)  ==  0)) {
            printf("%s", "void");
        } else {
            for (int   j = 0; (j  <  Vector_len__Param(new_params_v)); j++) {
                if ((j  >  0)) {
                    printf("%s", ", ");
                }
                Param   p = Vector_get__Param(new_params_v, j);
                printf("%s %s %s", type_to_c((p. ty )), " ", (p. name ));
            }
        }
        printf("%s\n", ") {");
        HashMap_clear__Type((g-> scope ));
        Vector__Expr*   saved_d = (g-> defer_stack );
        Vector__Expr*   saved_dr = (g-> auto_drop_stack );
        Vector__Expr*   nd_q = Vector_new__Expr();
        Vector__Expr*   nadr_q = Vector_new__Expr();
        ((g-> defer_stack )  =  nd_q);
        ((g-> auto_drop_stack )  =  nadr_q);
        for (int   j = 0; (j  <  Vector_len__Param(new_params_v)); j++) {
            Param   p = Vector_get__Param(new_params_v, j);
            CG_declare(g, (p. name ), (p. ty ));
        }
        for (int   j = 0; (j  <  Vector_len__Stmt(new_body)); j++) {
            Stmt   b = Vector_get__Stmt(new_body, j);
            emit_stmt(g, (&b), 1);
        }
        CG_emit_deferred_reverse(g, 1);
        ((g-> defer_stack )  =  saved_d);
        ((g-> auto_drop_stack )  =  saved_dr);
        printf("%s\n", "}");
        printf("%s\n", "");
    }
    for (int   i = 0; (i  <  Vector_len__AnonFn((g-> anon_fns ))); i++) {
        AnonFn   af = Vector_get__AnonFn((g-> anon_fns ), i);
        printf("%s %s %s %s", type_to_c((af. ret_ty )), " ", (af. name ), "(");
        if ((((af. params )  ==  NULL)  ||  (Vector_len__Param((af. params ))  ==  0))) {
            printf("%s", "void");
        } else {
            for (int   j = 0; (j  <  Vector_len__Param((af. params ))); j++) {
                if ((j  >  0)) {
                    printf("%s", ", ");
                }
                Param   p = Vector_get__Param((af. params ), j);
                printf("%s %s %s", type_to_c((p. ty )), " ", (p. name ));
            }
        }
        printf("%s\n", ") {");
        HashMap_clear__Type((g-> scope ));
        Vector__Expr*   saved_d = (g-> defer_stack );
        Vector__Expr*   saved_dr = (g-> auto_drop_stack );
        Vector__Expr*   nd = Vector_new__Expr();
        Vector__Expr*   nadr = Vector_new__Expr();
        ((g-> defer_stack )  =  nd);
        ((g-> auto_drop_stack )  =  nadr);
        if (((af. params )  !=  NULL)) {
            for (int   j = 0; (j  <  Vector_len__Param((af. params ))); j++) {
                Param   p = Vector_get__Param((af. params ), j);
                CG_declare(g, (p. name ), (p. ty ));
            }
        }
        if (((af. body )  !=  NULL)) {
            for (int   j = 0; (j  <  Vector_len__Stmt((af. body ))); j++) {
                Stmt   b = Vector_get__Stmt((af. body ), j);
                emit_stmt(g, (&b), 1);
            }
        }
        CG_emit_deferred_reverse(g, 1);
        ((g-> defer_stack )  =  saved_d);
        ((g-> auto_drop_stack )  =  saved_dr);
        printf("%s\n", "}");
        printf("%s\n", "");
    }
    CG_free(g);
}

void   emit_top_const (CG*   g, Stmt*   s) {
    if (((s-> let_value )  ==  NULL)) {
        return;
    }
    if (((((s-> let_ty )  !=  NULL)  &&  (((s-> let_ty )-> kind )  ==  TY_NAMED))  &&  (((__glide_string_eq(((s-> let_ty )-> name ), "int")  ||  __glide_string_eq(((s-> let_ty )-> name ), "uint"))  ||  __glide_string_eq(((s-> let_ty )-> name ), "i32"))  ||  __glide_string_eq(((s-> let_ty )-> name ), "i64")))) {
        printf("%s %s %s", "#define ", (s-> name ), " ");
        emit_expr(g, (s-> let_value ));
        printf("%s\n", "");
        return;
    }
    printf("%s", "static const ");
    if (((s-> let_ty )  !=  NULL)) {
        printf("%s %s %s", type_to_c((s-> let_ty )), " ", (s-> name ));
    } else {
        printf("%s %s", "int ", (s-> name ));
    }
    printf("%s", " = ");
    emit_expr(g, (s-> let_value ));
    printf("%s\n", ";");
}

void   emit_struct_mono (CG*   g, Type*   t) {
    if (((t  ==  NULL)  ||  ((t-> kind )  !=  TY_GENERIC))) {
        return;
    }
    if ((!HashMap_contains__Stmt((g-> generic_structs ), (t-> name )))) {
        return;
    }
    Stmt   template = HashMap_get__Stmt((g-> generic_structs ), (t-> name ));
    const char*   mangled = mangle_generic((t-> name ), (t-> args ));
    const char*   body_key = __glide_string_concat(mangled, "__body");
    if (HashMap_contains__bool((g-> emitted_monos ), body_key)) {
        return;
    }
    HashMap_insert__bool((g-> emitted_monos ), body_key, true);
    printf("%s %s %s\n", "struct ", mangled, " {");
    if ((((template. struct_fields )  !=  NULL)  &&  ((template. type_params )  !=  NULL))) {
        for (int   i = 0; (i  <  Vector_len__Field((template. struct_fields ))); i++) {
            Field   f = Vector_get__Field((template. struct_fields ), i);
            Type*   sub = subst_type((f. ty ), (template. type_params ), (t-> args ));
            printf("%s %s %s %s", "    ", type_to_c(sub), " ", (f. name ));
            printf("%s\n", ";");
        }
    }
    printf("%s\n", "};");
    printf("%s\n", "");
}

void   collect_generic_uses_in_type (Type*   t, Vector__Type*   out) {
    if ((t  ==  NULL)) {
        return;
    }
    if (((t-> kind )  ==  TY_GENERIC)) {
        if (((t-> args )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Type((t-> args ))); i++) {
                Type   a = Vector_get__Type((t-> args ), i);
                collect_generic_uses_in_type((&a), out);
            }
        }
        Vector_push__Type(out, (*t));
    }
    if ((((((t-> kind )  ==  TY_POINTER)  ||  ((t-> kind )  ==  TY_BORROW))  ||  ((t-> kind )  ==  TY_BORROW_MUT))  ||  ((t-> kind )  ==  TY_SLICE))) {
        collect_generic_uses_in_type((t-> inner ), out);
    }
}

void   collect_generic_uses_in_stmt (Stmt*   s, Vector__Type*   out) {
    if ((s  ==  NULL)) {
        return;
    }
    bool   is_generic = (((s-> type_params )  !=  NULL)  &&  (Vector_len__string((s-> type_params ))  >  0));
    if (is_generic) {
        return;
    }
    if (((s-> let_ty )  !=  NULL)) {
        collect_generic_uses_in_type((s-> let_ty ), out);
    }
    if (((s-> fn_ret_ty )  !=  NULL)) {
        collect_generic_uses_in_type((s-> fn_ret_ty ), out);
    }
    if (((s-> fn_params )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Param((s-> fn_params ))); i++) {
            Param   p = Vector_get__Param((s-> fn_params ), i);
            collect_generic_uses_in_type((p. ty ), out);
        }
    }
    if (((s-> struct_fields )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Field((s-> struct_fields ))); i++) {
            Field   f = Vector_get__Field((s-> struct_fields ), i);
            collect_generic_uses_in_type((f. ty ), out);
        }
    }
    if (((s-> fn_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> fn_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> fn_body ), i);
            collect_generic_uses_in_stmt((&b), out);
        }
    }
    if (((s-> then_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> then_body ), i);
            collect_generic_uses_in_stmt((&b), out);
        }
    }
    if (((s-> else_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> else_body ), i);
            collect_generic_uses_in_stmt((&b), out);
        }
    }
    if (((s-> impl_methods )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> impl_methods ))); i++) {
            Stmt   m = Vector_get__Stmt((s-> impl_methods ), i);
            collect_generic_uses_in_stmt((&m), out);
        }
    }
}

JsonValue*   json_null (void) {
    JsonValue*   v = (( JsonValue* )calloc(1, sizeof( JsonValue )));
    ((v-> kind )  =  JSON_NULL);
    return v;
}

JsonValue*   json_bool (bool   b) {
    JsonValue*   v = (( JsonValue* )calloc(1, sizeof( JsonValue )));
    ((v-> kind )  =  JSON_BOOL);
    ((v-> b )  =  b);
    return v;
}

JsonValue*   json_int (int   n) {
    JsonValue*   v = (( JsonValue* )calloc(1, sizeof( JsonValue )));
    ((v-> kind )  =  JSON_INT);
    ((v-> i )  =  n);
    return v;
}

JsonValue*   json_string (const char*   s) {
    JsonValue*   v = (( JsonValue* )calloc(1, sizeof( JsonValue )));
    ((v-> kind )  =  JSON_STRING);
    ((v-> s )  =  s);
    return v;
}

JsonValue*   json_array (void) {
    JsonValue*   v = (( JsonValue* )calloc(1, sizeof( JsonValue )));
    ((v-> kind )  =  JSON_ARRAY);
    Vector__JsonValue*   arr = Vector_new__JsonValue();
    ((v-> arr )  =  arr);
    return v;
}

JsonValue*   json_object (void) {
    JsonValue*   v = (( JsonValue* )calloc(1, sizeof( JsonValue )));
    ((v-> kind )  =  JSON_OBJECT);
    Vector__string*   keys = Vector_new__string();
    Vector__JsonValue*   vals = Vector_new__JsonValue();
    ((v-> obj_keys )  =  keys);
    ((v-> obj_vals )  =  vals);
    return v;
}

void   json_arr_push (JsonValue*   arr, JsonValue*   v) {
    if (((arr  ==  NULL)  ||  ((arr-> kind )  !=  JSON_ARRAY))) {
        return;
    }
    Vector_push__JsonValue((arr-> arr ), (*v));
}

void   json_obj_set (JsonValue*   obj, const char*   key, JsonValue*   v) {
    if (((obj  ==  NULL)  ||  ((obj-> kind )  !=  JSON_OBJECT))) {
        return;
    }
    Vector_push__string((obj-> obj_keys ), key);
    Vector_push__JsonValue((obj-> obj_vals ), (*v));
}

JsonValue*   json_get (JsonValue*   v, const char*   key) {
    if (((v  ==  NULL)  ||  ((v-> kind )  !=  JSON_OBJECT))) {
        return NULL;
    }
    for (int   i = 0; (i  <  Vector_len__string((v-> obj_keys ))); i++) {
        if (__glide_string_eq(Vector_get__string((v-> obj_keys ), i), key)) {
            JsonValue   val = Vector_get__JsonValue((v-> obj_vals ), i);
            JsonValue*   p = (( JsonValue* )malloc(sizeof( JsonValue )));
            ((*p)  =  val);
            return p;
        }
    }
    return NULL;
}

JsonValue*   json_at (JsonValue*   v, int   idx) {
    if (((v  ==  NULL)  ||  ((v-> kind )  !=  JSON_ARRAY))) {
        return NULL;
    }
    if (((idx  <  0)  ||  (idx  >=  Vector_len__JsonValue((v-> arr ))))) {
        return NULL;
    }
    JsonValue   val = Vector_get__JsonValue((v-> arr ), idx);
    JsonValue*   p = (( JsonValue* )malloc(sizeof( JsonValue )));
    ((*p)  =  val);
    return p;
}

const char*   json_as_string (JsonValue*   v) {
    if (((v  ==  NULL)  ||  ((v-> kind )  !=  JSON_STRING))) {
        return "";
    }
    return (v-> s );
}

int   json_as_int (JsonValue*   v) {
    if ((v  ==  NULL)) {
        return 0;
    }
    if (((v-> kind )  ==  JSON_INT)) {
        return (v-> i );
    }
    return 0;
}

bool   json_as_bool (JsonValue*   v) {
    if (((v  ==  NULL)  ||  ((v-> kind )  !=  JSON_BOOL))) {
        return false;
    }
    return (v-> b );
}

bool   json_is_null (JsonValue*   v) {
    if ((v  ==  NULL)) {
        return true;
    }
    return ((v-> kind )  ==  JSON_NULL);
}

int   jp_peek (JsonParser*   p) {
    if (((p-> pos )  >=  (p-> n ))) {
        return (-1);
    }
    return __glide_char_to_int(__glide_string_at((p-> src ), (p-> pos )));
}

int   jp_advance (JsonParser*   p) {
    int   c = jp_peek(p);
    ((p-> pos )  =  ((p-> pos )  +  1));
    return c;
}

void   jp_skip_ws (JsonParser*   p) {
    while (((p-> pos )  <  (p-> n ))) {
        int   c = jp_peek(p);
        if (((((c  ==  32)  ||  (c  ==  9))  ||  (c  ==  10))  ||  (c  ==  13))) {
            ((p-> pos )  =  ((p-> pos )  +  1));
        } else {
            break;
        }
    }
}

JsonValue*   jp_value (JsonParser*   p) {
    jp_skip_ws(p);
    int   c = jp_peek(p);
    if ((c  ==  123)) {
        return jp_object(p);
    }
    if ((c  ==  91)) {
        return jp_array(p);
    }
    if ((c  ==  34)) {
        return jp_string_value(p);
    }
    if (((c  ==  116)  ||  (c  ==  102))) {
        return jp_bool(p);
    }
    if ((c  ==  110)) {
        return jp_null(p);
    }
    if (((c  ==  45)  ||  ((c  >=  48)  &&  (c  <=  57)))) {
        return jp_number(p);
    }
    return NULL;
}

JsonValue*   jp_object (JsonParser*   p) {
    JsonValue*   obj = json_object();
    ((p-> pos )  =  ((p-> pos )  +  1));
    jp_skip_ws(p);
    if ((jp_peek(p)  ==  125)) {
        ((p-> pos )  =  ((p-> pos )  +  1));
        return obj;
    }
    while (((p-> pos )  <  (p-> n ))) {
        jp_skip_ws(p);
        JsonValue*   k = jp_string_value(p);
        if ((k  ==  NULL)) {
            return obj;
        }
        jp_skip_ws(p);
        if ((jp_peek(p)  ==  58)) {
            ((p-> pos )  =  ((p-> pos )  +  1));
        }
        JsonValue*   v = jp_value(p);
        if ((v  ==  NULL)) {
            (v  =  json_null());
        }
        Vector_push__string((obj-> obj_keys ), (k-> s ));
        Vector_push__JsonValue((obj-> obj_vals ), (*v));
        jp_skip_ws(p);
        int   c = jp_peek(p);
        if ((c  ==  44)) {
            ((p-> pos )  =  ((p-> pos )  +  1));
            continue;
        }
        if ((c  ==  125)) {
            ((p-> pos )  =  ((p-> pos )  +  1));
            break;
        }
        break;
    }
    return obj;
}

JsonValue*   jp_array (JsonParser*   p) {
    JsonValue*   arr = json_array();
    ((p-> pos )  =  ((p-> pos )  +  1));
    jp_skip_ws(p);
    if ((jp_peek(p)  ==  93)) {
        ((p-> pos )  =  ((p-> pos )  +  1));
        return arr;
    }
    while (((p-> pos )  <  (p-> n ))) {
        JsonValue*   v = jp_value(p);
        if ((v  ==  NULL)) {
            return arr;
        }
        Vector_push__JsonValue((arr-> arr ), (*v));
        jp_skip_ws(p);
        int   c = jp_peek(p);
        if ((c  ==  44)) {
            ((p-> pos )  =  ((p-> pos )  +  1));
            continue;
        }
        if ((c  ==  93)) {
            ((p-> pos )  =  ((p-> pos )  +  1));
            break;
        }
        break;
    }
    return arr;
}

JsonValue*   jp_string_value (JsonParser*   p) {
    if ((jp_peek(p)  !=  34)) {
        return NULL;
    }
    ((p-> pos )  =  ((p-> pos )  +  1));
    int   start = (p-> pos );
    bool   has_escape = false;
    while (((p-> pos )  <  (p-> n ))) {
        int   c = jp_peek(p);
        if ((c  ==  34)) {
            break;
        }
        if ((c  ==  92)) {
            (has_escape  =  true);
            ((p-> pos )  =  ((p-> pos )  +  1));
            if (((p-> pos )  <  (p-> n ))) {
                ((p-> pos )  =  ((p-> pos )  +  1));
            }
            continue;
        }
        ((p-> pos )  =  ((p-> pos )  +  1));
    }
    const char*   raw = __glide_string_substring((p-> src ), start, (p-> pos ));
    if ((jp_peek(p)  ==  34)) {
        ((p-> pos )  =  ((p-> pos )  +  1));
    }
    if ((!has_escape)) {
        return json_string(raw);
    }
    return json_string(jp_unescape(raw));
}

const char*   jp_unescape (const char*   raw) {
    const char*   out = "";
    int   n = __glide_string_len(raw);
    int   i = 0;
    while ((i  <  n)) {
        int   c = __glide_char_to_int(__glide_string_at(raw, i));
        if ((c  !=  92)) {
            (out  =  __glide_string_concat(out, __glide_string_substring(raw, i, (i  +  1))));
            (i  =  (i  +  1));
            continue;
        }
        (i  =  (i  +  1));
        if ((i  >=  n)) {
            break;
        }
        int   e = __glide_char_to_int(__glide_string_at(raw, i));
        (i  =  (i  +  1));
        if ((e  ==  110)) {
            (out  =  __glide_string_concat(out, "\n"));
            continue;
        }
        if ((e  ==  116)) {
            (out  =  __glide_string_concat(out, "\t"));
            continue;
        }
        if ((e  ==  114)) {
            (out  =  __glide_string_concat(out, "\r"));
            continue;
        }
        if ((e  ==  34)) {
            (out  =  __glide_string_concat(out, "\""));
            continue;
        }
        if ((e  ==  92)) {
            (out  =  __glide_string_concat(out, "\\"));
            continue;
        }
        if ((e  ==  47)) {
            (out  =  __glide_string_concat(out, "/"));
            continue;
        }
        if (((e  ==  117)  &&  ((i  +  4)  <=  n))) {
            (i  =  (i  +  4));
            (out  =  __glide_string_concat(out, "?"));
            continue;
        }
    }
    return out;
}

JsonValue*   jp_bool (JsonParser*   p) {
    int   c = jp_peek(p);
    if ((c  ==  116)) {
        ((p-> pos )  =  ((p-> pos )  +  4));
        return json_bool(true);
    }
    ((p-> pos )  =  ((p-> pos )  +  5));
    return json_bool(false);
}

JsonValue*   jp_null (JsonParser*   p) {
    ((p-> pos )  =  ((p-> pos )  +  4));
    return json_null();
}

JsonValue*   jp_number (JsonParser*   p) {
    bool   neg = false;
    if ((jp_peek(p)  ==  45)) {
        (neg  =  true);
        ((p-> pos )  =  ((p-> pos )  +  1));
    }
    int   n = 0;
    while (((p-> pos )  <  (p-> n ))) {
        int   c = jp_peek(p);
        if (((c  <  48)  ||  (c  >  57))) {
            break;
        }
        (n  =  ((n  *  10)  +  (c  -  48)));
        ((p-> pos )  =  ((p-> pos )  +  1));
    }
    if ((jp_peek(p)  ==  46)) {
        ((p-> pos )  =  ((p-> pos )  +  1));
        while (((p-> pos )  <  (p-> n ))) {
            int   c = jp_peek(p);
            if (((c  <  48)  ||  (c  >  57))) {
                break;
            }
            ((p-> pos )  =  ((p-> pos )  +  1));
        }
    }
    if (neg) {
        (n  =  (-n));
    }
    return json_int(n);
}

JsonValue*   json_parse (const char*   src) {
    JsonParser   p = (( JsonParser ){. src  = src, . pos  = 0, . n  = __glide_string_len(src)});
    return jp_value((&p));
}

const char*   json_escape (const char*   s) {
    const char*   out = "\"";
    int   n = __glide_string_len(s);
    for (int   i = 0; (i  <  n); i++) {
        int   c = __glide_char_to_int(__glide_string_at(s, i));
        if ((c  ==  34)) {
            (out  =  __glide_string_concat(out, "\\\""));
        } else {
            if ((c  ==  92)) {
                (out  =  __glide_string_concat(out, "\\\\"));
            } else {
                if ((c  ==  10)) {
                    (out  =  __glide_string_concat(out, "\\n"));
                } else {
                    if ((c  ==  13)) {
                        (out  =  __glide_string_concat(out, "\\r"));
                    } else {
                        if ((c  ==  9)) {
                            (out  =  __glide_string_concat(out, "\\t"));
                        } else {
                            if ((c  <  32)) {
                                (out  =  __glide_string_concat(out, "?"));
                            } else {
                                (out  =  __glide_string_concat(out, __glide_string_substring(s, i, (i  +  1))));
                            }
                        }
                    }
                }
            }
        }
    }
    return __glide_string_concat(out, "\"");
}

const char*   json_emit (JsonValue*   v) {
    if ((v  ==  NULL)) {
        return "null";
    }
    if (((v-> kind )  ==  JSON_NULL)) {
        return "null";
    }
    if (((v-> kind )  ==  JSON_BOOL)) {
        if ((v-> b )) {
            return "true";
        }
        return "false";
    }
    if (((v-> kind )  ==  JSON_INT)) {
        return int_to_str((v-> i ));
    }
    if (((v-> kind )  ==  JSON_STRING)) {
        return json_escape((v-> s ));
    }
    if (((v-> kind )  ==  JSON_ARRAY)) {
        const char*   out = "[";
        int   n = Vector_len__JsonValue((v-> arr ));
        for (int   i = 0; (i  <  n); i++) {
            if ((i  >  0)) {
                (out  =  __glide_string_concat(out, ","));
            }
            JsonValue   e = Vector_get__JsonValue((v-> arr ), i);
            (out  =  __glide_string_concat(out, json_emit((&e))));
        }
        return __glide_string_concat(out, "]");
    }
    if (((v-> kind )  ==  JSON_OBJECT)) {
        const char*   out = "{";
        int   n = Vector_len__string((v-> obj_keys ));
        for (int   i = 0; (i  <  n); i++) {
            if ((i  >  0)) {
                (out  =  __glide_string_concat(out, ","));
            }
            (out  =  __glide_string_concat(out, json_escape(Vector_get__string((v-> obj_keys ), i))));
            (out  =  __glide_string_concat(out, ":"));
            JsonValue   val = Vector_get__JsonValue((v-> obj_vals ), i);
            (out  =  __glide_string_concat(out, json_emit((&val))));
        }
        return __glide_string_concat(out, "}");
    }
    return "null";
}

const char*   fmt_indent (int   depth) {
    const char*   s = "";
    for (int   i = 0; (i  <  depth); i++) {
        (s  =  __glide_string_concat(s, FMT_INDENT));
    }
    return s;
}

const char*   fmt_type (Type*   t) {
    if ((t  ==  NULL)) {
        return "";
    }
    if (((t-> kind )  ==  TY_NAMED)) {
        return (t-> name );
    }
    if (((t-> kind )  ==  TY_POINTER)) {
        return __glide_string_concat("*", fmt_type((t-> inner )));
    }
    if (((t-> kind )  ==  TY_BORROW)) {
        return __glide_string_concat("&", fmt_type((t-> inner )));
    }
    if (((t-> kind )  ==  TY_BORROW_MUT)) {
        return __glide_string_concat("&mut ", fmt_type((t-> inner )));
    }
    if (((t-> kind )  ==  TY_SLICE)) {
        return __glide_string_concat("[]", fmt_type((t-> inner )));
    }
    if (((t-> kind )  ==  TY_RESULT)) {
        return __glide_string_concat("!", fmt_type((t-> inner )));
    }
    if (((t-> kind )  ==  TY_GENERIC)) {
        const char*   out = __glide_string_concat((t-> name ), "<");
        if (((t-> args )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Type((t-> args ))); i++) {
                if ((i  >  0)) {
                    (out  =  __glide_string_concat(out, ", "));
                }
                Type   a = Vector_get__Type((t-> args ), i);
                (out  =  __glide_string_concat(out, fmt_type((&a))));
            }
        }
        return __glide_string_concat(out, ">");
    }
    if (((t-> kind )  ==  TY_FNPTR)) {
        const char*   out = "fn(";
        if (((t-> args )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Type((t-> args ))); i++) {
                if ((i  >  0)) {
                    (out  =  __glide_string_concat(out, ", "));
                }
                Type   a = Vector_get__Type((t-> args ), i);
                (out  =  __glide_string_concat(out, fmt_type((&a))));
            }
        }
        (out  =  __glide_string_concat(out, ")"));
        if (((t-> fnptr_ret )  !=  NULL)) {
            (out  =  __glide_string_concat(__glide_string_concat(out, " -> "), fmt_type((t-> fnptr_ret ))));
        }
        return out;
    }
    return "?";
}

const char*   fmt_char_lit (int   n) {
    if ((n  ==  39)) {
        return "'\\''";
    }
    if ((n  ==  92)) {
        return "'\\\\'";
    }
    if ((n  ==  10)) {
        return "'\\n'";
    }
    if ((n  ==  9)) {
        return "'\\t'";
    }
    if ((n  ==  13)) {
        return "'\\r'";
    }
    if ((n  ==  0)) {
        return "'\\0'";
    }
    const char*   hex = "0123456789abcdef";
    int   high = ((n  /  16)  %  16);
    if ((high  <  0)) {
        (high  =  0);
    }
    int   low = (n  %  16);
    if ((low  <  0)) {
        (low  =  0);
    }
    return __glide_string_concat(__glide_string_concat(__glide_string_concat("'\\x", __glide_string_substring(hex, high, (high  +  1))), __glide_string_substring(hex, low, (low  +  1))), "'");
}

const char*   fmt_binop (int   op) {
    if ((op  ==  OP_ADD)) {
        return "+";
    }
    if ((op  ==  OP_SUB)) {
        return "-";
    }
    if ((op  ==  OP_MUL)) {
        return "*";
    }
    if ((op  ==  OP_DIV)) {
        return "/";
    }
    if ((op  ==  OP_MOD)) {
        return "%";
    }
    if ((op  ==  OP_EQ)) {
        return "==";
    }
    if ((op  ==  OP_NE)) {
        return "!=";
    }
    if ((op  ==  OP_LT)) {
        return "<";
    }
    if ((op  ==  OP_LE)) {
        return "<=";
    }
    if ((op  ==  OP_GT)) {
        return ">";
    }
    if ((op  ==  OP_GE)) {
        return ">=";
    }
    if ((op  ==  OP_AND)) {
        return "&&";
    }
    if ((op  ==  OP_OR)) {
        return "||";
    }
    if ((op  ==  OP_BIT_AND)) {
        return "&";
    }
    if ((op  ==  OP_BIT_OR)) {
        return "|";
    }
    if ((op  ==  OP_BIT_XOR)) {
        return "^";
    }
    if ((op  ==  OP_SHL)) {
        return "<<";
    }
    if ((op  ==  OP_SHR)) {
        return ">>";
    }
    if ((op  ==  OP_ASSIGN)) {
        return "=";
    }
    if ((op  ==  OP_PLUS_ASSIGN)) {
        return "+=";
    }
    if ((op  ==  OP_MINUS_ASSIGN)) {
        return "-=";
    }
    if ((op  ==  OP_STAR_ASSIGN)) {
        return "*=";
    }
    if ((op  ==  OP_SLASH_ASSIGN)) {
        return "/=";
    }
    return "?";
}

const char*   fmt_unop (int   op) {
    if ((op  ==  UN_NEG)) {
        return "-";
    }
    if ((op  ==  UN_NOT)) {
        return "!";
    }
    if ((op  ==  UN_DEREF)) {
        return "*";
    }
    if ((op  ==  UN_ADDR)) {
        return "&";
    }
    if ((op  ==  UN_ADDR_MUT)) {
        return "&mut ";
    }
    if ((op  ==  UN_BIT_NOT)) {
        return "~";
    }
    return "";
}

const char*   fmt_expr (Expr*   e) {
    if ((e  ==  NULL)) {
        return "";
    }
    if (((e-> kind )  ==  EX_INT)) {
        return int_to_str((e-> int_val ));
    }
    if (((e-> kind )  ==  EX_FLOAT)) {
        return int_to_str((e-> int_val ));
    }
    if (((e-> kind )  ==  EX_STRING)) {
        return __glide_string_concat(__glide_string_concat("\"", (e-> str_val )), "\"");
    }
    if (((e-> kind )  ==  EX_BOOL)) {
        if ((e-> bool_val )) {
            return "true";
        }
        return "false";
    }
    if (((e-> kind )  ==  EX_NULL)) {
        return "null";
    }
    if (((e-> kind )  ==  EX_IDENT)) {
        return (e-> str_val );
    }
    if (((e-> kind )  ==  EX_CHAR)) {
        return fmt_char_lit((e-> int_val ));
    }
    if (((e-> kind )  ==  EX_BINARY)) {
        return __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("(", fmt_expr((e-> lhs ))), " "), fmt_binop((e-> op_code ))), " "), fmt_expr((e-> rhs ))), ")");
    }
    if (((e-> kind )  ==  EX_UNARY)) {
        if (((e-> op_code )  ==  UN_TRY)) {
            return __glide_string_concat(fmt_expr((e-> operand )), "?");
        }
        return __glide_string_concat(__glide_string_concat(__glide_string_concat("(", fmt_unop((e-> op_code ))), fmt_expr((e-> operand ))), ")");
    }
    if (((e-> kind )  ==  EX_ASSIGN)) {
        return __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(fmt_expr((e-> lhs )), " "), fmt_binop((e-> op_code ))), " "), fmt_expr((e-> rhs )));
    }
    if (((e-> kind )  ==  EX_CALL)) {
        const char*   out = __glide_string_concat(fmt_expr((e-> lhs )), "(");
        if (((e-> args )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
                if ((i  >  0)) {
                    (out  =  __glide_string_concat(out, ", "));
                }
                Expr   a = Vector_get__Expr((e-> args ), i);
                (out  =  __glide_string_concat(out, fmt_expr((&a))));
            }
        }
        return __glide_string_concat(out, ")");
    }
    if (((e-> kind )  ==  EX_MEMBER)) {
        return __glide_string_concat(__glide_string_concat(fmt_expr((e-> lhs )), "."), (e-> field ));
    }
    if (((e-> kind )  ==  EX_INDEX)) {
        return __glide_string_concat(__glide_string_concat(__glide_string_concat(fmt_expr((e-> lhs )), "["), fmt_expr((e-> rhs ))), "]");
    }
    if (((e-> kind )  ==  EX_CAST)) {
        return __glide_string_concat(__glide_string_concat(fmt_expr((e-> lhs )), " as "), fmt_type((e-> cast_to )));
    }
    if (((e-> kind )  ==  EX_PATH)) {
        return __glide_string_concat(__glide_string_concat((e-> str_val ), "::"), (e-> field ));
    }
    if (((e-> kind )  ==  EX_POSTINC)) {
        return __glide_string_concat(fmt_expr((e-> lhs )), "++");
    }
    if (((e-> kind )  ==  EX_POSTDEC)) {
        return __glide_string_concat(fmt_expr((e-> lhs )), "--");
    }
    if (((e-> kind )  ==  EX_MACRO)) {
        const char*   out = __glide_string_concat((e-> str_val ), "!(");
        if (((e-> args )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
                if ((i  >  0)) {
                    (out  =  __glide_string_concat(out, ", "));
                }
                Expr   a = Vector_get__Expr((e-> args ), i);
                (out  =  __glide_string_concat(out, fmt_expr((&a))));
            }
        }
        return __glide_string_concat(out, ")");
    }
    if (((e-> kind )  ==  EX_SIZEOF)) {
        return __glide_string_concat(__glide_string_concat("sizeof(", fmt_type((e-> cast_to ))), ")");
    }
    if (((e-> kind )  ==  EX_NEW)) {
        return fmt_struct_lit(e, "new ");
    }
    if (((e-> kind )  ==  EX_STRUCT_LIT)) {
        return fmt_struct_lit(e, "");
    }
    if (((e-> kind )  ==  EX_FNEXPR)) {
        const char*   out = "fn(";
        if (((e-> fn_expr_params )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Param((e-> fn_expr_params ))); i++) {
                if ((i  >  0)) {
                    (out  =  __glide_string_concat(out, ", "));
                }
                Param   p = Vector_get__Param((e-> fn_expr_params ), i);
                (out  =  __glide_string_concat(__glide_string_concat(__glide_string_concat(out, (p. name )), ": "), fmt_type((p. ty ))));
            }
        }
        (out  =  __glide_string_concat(out, ")"));
        if (((e-> cast_to )  !=  NULL)) {
            (out  =  __glide_string_concat(__glide_string_concat(out, " -> "), fmt_type((e-> cast_to ))));
        }
        (out  =  __glide_string_concat(out, " { "));
        if (((e-> fn_expr_body )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Stmt((e-> fn_expr_body ))); i++) {
                if ((i  >  0)) {
                    (out  =  __glide_string_concat(out, " "));
                }
                Stmt   b = Vector_get__Stmt((e-> fn_expr_body ), i);
                (out  =  __glide_string_concat(out, fmt_stmt_inline((&b))));
            }
        }
        return __glide_string_concat(out, " }");
    }
    return "/* fmt: unknown expr */";
}

const char*   fmt_struct_lit (Expr*   e, const char*   prefix) {
    const char*   out = __glide_string_concat(__glide_string_concat(prefix, (e-> str_val )), " {");
    if (((((e-> field_names )  !=  NULL)  &&  ((e-> args )  !=  NULL))  &&  (Vector_len__Expr((e-> args ))  >  0))) {
        (out  =  __glide_string_concat(out, " "));
        for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
            if ((i  >  0)) {
                (out  =  __glide_string_concat(out, ", "));
            }
            (out  =  __glide_string_concat(__glide_string_concat(out, Vector_get__string((e-> field_names ), i)), ": "));
            Expr   v = Vector_get__Expr((e-> args ), i);
            (out  =  __glide_string_concat(out, fmt_expr((&v))));
        }
        (out  =  __glide_string_concat(out, " "));
    }
    return __glide_string_concat(out, "}");
}

const char*   fmt_stmt_inline (Stmt*   s) {
    if ((s  ==  NULL)) {
        return "";
    }
    if (((s-> kind )  ==  ST_RETURN)) {
        if (((s-> expr_value )  !=  NULL)) {
            return __glide_string_concat(__glide_string_concat("return ", fmt_expr((s-> expr_value ))), ";");
        }
        return "return;";
    }
    if (((s-> kind )  ==  ST_EXPR)) {
        return __glide_string_concat(fmt_expr((s-> expr_value )), ";");
    }
    if ((((s-> kind )  ==  ST_LET)  ||  ((s-> kind )  ==  ST_CONST))) {
        return fmt_let_const_inline(s);
    }
    return fmt_stmt(s, 0);
}

const char*   fmt_let_const_inline (Stmt*   s) {
    const char*   out = "";
    if (((s-> kind )  ==  ST_CONST)) {
        (out  =  "const ");
    } else {
        (out  =  "let ");
    }
    if ((s-> is_mut )) {
        (out  =  __glide_string_concat(out, "mut "));
    }
    (out  =  __glide_string_concat(out, (s-> name )));
    if ((s-> is_auto_owned )) {
        (out  =  __glide_string_concat(out, "*"));
    }
    if (((s-> let_ty )  !=  NULL)) {
        (out  =  __glide_string_concat(__glide_string_concat(out, ": "), fmt_type((s-> let_ty ))));
    }
    if (((s-> let_value )  !=  NULL)) {
        (out  =  __glide_string_concat(__glide_string_concat(out, " = "), fmt_expr((s-> let_value ))));
    }
    return __glide_string_concat(out, ";");
}

const char*   fmt_stmt (Stmt*   s, int   depth) {
    if ((s  ==  NULL)) {
        return "";
    }
    const char*   pad = fmt_indent(depth);
    if ((((s-> kind )  ==  ST_LET)  ||  ((s-> kind )  ==  ST_CONST))) {
        return __glide_string_concat(__glide_string_concat(pad, fmt_let_const_inline(s)), "\n");
    }
    if (((s-> kind )  ==  ST_RETURN)) {
        if (((s-> expr_value )  !=  NULL)) {
            return __glide_string_concat(__glide_string_concat(__glide_string_concat(pad, "return "), fmt_expr((s-> expr_value ))), ";\n");
        }
        return __glide_string_concat(pad, "return;\n");
    }
    if (((s-> kind )  ==  ST_EXPR)) {
        return __glide_string_concat(__glide_string_concat(pad, fmt_expr((s-> expr_value ))), ";\n");
    }
    if (((s-> kind )  ==  ST_BREAK)) {
        return __glide_string_concat(pad, "break;\n");
    }
    if (((s-> kind )  ==  ST_CONTINUE)) {
        return __glide_string_concat(pad, "continue;\n");
    }
    if (((s-> kind )  ==  ST_DEFER)) {
        return __glide_string_concat(__glide_string_concat(__glide_string_concat(pad, "defer "), fmt_expr((s-> expr_value ))), ";\n");
    }
    if (((s-> kind )  ==  ST_SPAWN)) {
        return __glide_string_concat(__glide_string_concat(__glide_string_concat(pad, "spawn "), fmt_expr((s-> expr_value ))), ";\n");
    }
    if (((s-> kind )  ==  ST_IMPORT)) {
        return __glide_string_concat(__glide_string_concat(__glide_string_concat(pad, "import "), (s-> import_path )), ";\n");
    }
    if (((s-> kind )  ==  ST_IF)) {
        const char*   out = __glide_string_concat(__glide_string_concat(__glide_string_concat(pad, "if "), fmt_expr((s-> cond ))), " {\n");
        if (((s-> then_body )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
                Stmt   b = Vector_get__Stmt((s-> then_body ), i);
                (out  =  __glide_string_concat(out, fmt_stmt((&b), (depth  +  1))));
            }
        }
        if (((s-> else_body )  !=  NULL)) {
            (out  =  __glide_string_concat(__glide_string_concat(out, pad), "} else {\n"));
            for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
                Stmt   b = Vector_get__Stmt((s-> else_body ), i);
                (out  =  __glide_string_concat(out, fmt_stmt((&b), (depth  +  1))));
            }
        }
        return __glide_string_concat(__glide_string_concat(out, pad), "}\n");
    }
    if (((s-> kind )  ==  ST_WHILE)) {
        const char*   out = __glide_string_concat(__glide_string_concat(__glide_string_concat(pad, "while "), fmt_expr((s-> cond ))), " {\n");
        if (((s-> then_body )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
                Stmt   b = Vector_get__Stmt((s-> then_body ), i);
                (out  =  __glide_string_concat(out, fmt_stmt((&b), (depth  +  1))));
            }
        }
        return __glide_string_concat(__glide_string_concat(out, pad), "}\n");
    }
    if (((s-> kind )  ==  ST_FOR)) {
        const char*   out = __glide_string_concat(pad, "for ");
        if (((s-> for_init )  !=  NULL)) {
            (out  =  __glide_string_concat(out, fmt_for_init((s-> for_init ))));
        }
        (out  =  __glide_string_concat(out, "; "));
        if (((s-> cond )  !=  NULL)) {
            (out  =  __glide_string_concat(out, fmt_expr((s-> cond ))));
        }
        (out  =  __glide_string_concat(out, "; "));
        if (((s-> for_step )  !=  NULL)) {
            (out  =  __glide_string_concat(out, fmt_expr((s-> for_step ))));
        }
        (out  =  __glide_string_concat(out, " {\n"));
        if (((s-> then_body )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
                Stmt   b = Vector_get__Stmt((s-> then_body ), i);
                (out  =  __glide_string_concat(out, fmt_stmt((&b), (depth  +  1))));
            }
        }
        return __glide_string_concat(__glide_string_concat(out, pad), "}\n");
    }
    if (((s-> kind )  ==  ST_BLOCK)) {
        const char*   out = __glide_string_concat(pad, "{\n");
        if (((s-> then_body )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
                Stmt   b = Vector_get__Stmt((s-> then_body ), i);
                (out  =  __glide_string_concat(out, fmt_stmt((&b), (depth  +  1))));
            }
        }
        return __glide_string_concat(__glide_string_concat(out, pad), "}\n");
    }
    if (((s-> kind )  ==  ST_FN)) {
        return fmt_fn(s, depth);
    }
    if (((s-> kind )  ==  ST_STRUCT)) {
        return fmt_struct(s, depth);
    }
    if (((s-> kind )  ==  ST_ENUM)) {
        return fmt_enum(s, depth);
    }
    if (((s-> kind )  ==  ST_IMPL)) {
        return fmt_impl(s, depth);
    }
    if (((s-> kind )  ==  ST_MATCH)) {
        return fmt_match(s, depth);
    }
    return __glide_string_concat(pad, "/* fmt: unknown stmt */\n");
}

const char*   fmt_for_init (Stmt*   s) {
    if ((s  ==  NULL)) {
        return "";
    }
    if (((s-> kind )  ==  ST_LET)) {
        return __glide_string_substring(fmt_let_const_inline(s), 0, (__glide_string_len(fmt_let_const_inline(s))  -  1));
    }
    if (((s-> kind )  ==  ST_EXPR)) {
        return fmt_expr((s-> expr_value ));
    }
    return "";
}

const char*   fmt_type_params (Stmt*   s) {
    if ((((s-> type_params )  ==  NULL)  ||  (Vector_len__string((s-> type_params ))  ==  0))) {
        return "";
    }
    const char*   out = "<";
    for (int   i = 0; (i  <  Vector_len__string((s-> type_params ))); i++) {
        if ((i  >  0)) {
            (out  =  __glide_string_concat(out, ", "));
        }
        (out  =  __glide_string_concat(out, Vector_get__string((s-> type_params ), i)));
    }
    return __glide_string_concat(out, ">");
}

const char*   fmt_fn (Stmt*   s, int   depth) {
    const char*   pad = fmt_indent(depth);
    const char*   head = pad;
    if ((s-> is_pub )) {
        (head  =  __glide_string_concat(head, "pub "));
    }
    (head  =  __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(head, "fn "), (s-> name )), fmt_type_params(s)), "("));
    if (((s-> fn_params )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Param((s-> fn_params ))); i++) {
            if ((i  >  0)) {
                (head  =  __glide_string_concat(head, ", "));
            }
            Param   p = Vector_get__Param((s-> fn_params ), i);
            (head  =  __glide_string_concat(__glide_string_concat(__glide_string_concat(head, (p. name )), ": "), fmt_type((p. ty ))));
        }
    }
    (head  =  __glide_string_concat(head, ")"));
    if (((s-> fn_ret_ty )  !=  NULL)) {
        (head  =  __glide_string_concat(__glide_string_concat(head, " -> "), fmt_type((s-> fn_ret_ty ))));
    }
    if (((s-> fn_body )  ==  NULL)) {
        return __glide_string_concat(head, ";\n");
    }
    const char*   out = __glide_string_concat(head, " {\n");
    for (int   i = 0; (i  <  Vector_len__Stmt((s-> fn_body ))); i++) {
        Stmt   b = Vector_get__Stmt((s-> fn_body ), i);
        (out  =  __glide_string_concat(out, fmt_stmt((&b), (depth  +  1))));
    }
    return __glide_string_concat(__glide_string_concat(out, pad), "}\n");
}

const char*   fmt_struct (Stmt*   s, int   depth) {
    const char*   pad = fmt_indent(depth);
    const char*   head = pad;
    if ((s-> is_pub )) {
        (head  =  __glide_string_concat(head, "pub "));
    }
    (head  =  __glide_string_concat(__glide_string_concat(__glide_string_concat(head, "struct "), (s-> name )), fmt_type_params(s)));
    if ((((s-> struct_fields )  ==  NULL)  ||  (Vector_len__Field((s-> struct_fields ))  ==  0))) {
        return __glide_string_concat(head, " {}\n");
    }
    const char*   out = __glide_string_concat(head, " {\n");
    const char*   inner = fmt_indent((depth  +  1));
    for (int   i = 0; (i  <  Vector_len__Field((s-> struct_fields ))); i++) {
        Field   f = Vector_get__Field((s-> struct_fields ), i);
        (out  =  __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(out, inner), (f. name )), ": "), fmt_type((f. ty ))), ",\n"));
    }
    return __glide_string_concat(__glide_string_concat(out, pad), "}\n");
}

const char*   fmt_enum (Stmt*   s, int   depth) {
    const char*   pad = fmt_indent(depth);
    const char*   head = pad;
    if ((s-> is_pub )) {
        (head  =  __glide_string_concat(head, "pub "));
    }
    (head  =  __glide_string_concat(__glide_string_concat(head, "enum "), (s-> name )));
    if ((((s-> enum_variants )  ==  NULL)  ||  (Vector_len__EnumVariant((s-> enum_variants ))  ==  0))) {
        return __glide_string_concat(head, " {}\n");
    }
    const char*   out = __glide_string_concat(head, " {\n");
    const char*   inner = fmt_indent((depth  +  1));
    for (int   i = 0; (i  <  Vector_len__EnumVariant((s-> enum_variants ))); i++) {
        EnumVariant   v = Vector_get__EnumVariant((s-> enum_variants ), i);
        (out  =  __glide_string_concat(__glide_string_concat(out, inner), (v. name )));
        if ((((v. fields )  !=  NULL)  &&  (Vector_len__Type((v. fields ))  >  0))) {
            (out  =  __glide_string_concat(out, "("));
            for (int   j = 0; (j  <  Vector_len__Type((v. fields ))); j++) {
                if ((j  >  0)) {
                    (out  =  __glide_string_concat(out, ", "));
                }
                Type   ty = Vector_get__Type((v. fields ), j);
                (out  =  __glide_string_concat(out, fmt_type((&ty))));
            }
            (out  =  __glide_string_concat(out, ")"));
        }
        (out  =  __glide_string_concat(out, ",\n"));
    }
    return __glide_string_concat(__glide_string_concat(out, pad), "}\n");
}

const char*   fmt_impl (Stmt*   s, int   depth) {
    const char*   pad = fmt_indent(depth);
    const char*   head = __glide_string_concat(pad, "impl");
    (head  =  __glide_string_concat(head, fmt_type_params(s)));
    (head  =  __glide_string_concat(__glide_string_concat(head, " "), fmt_type((s-> impl_target ))));
    if ((((s-> impl_methods )  ==  NULL)  ||  (Vector_len__Stmt((s-> impl_methods ))  ==  0))) {
        return __glide_string_concat(head, " {}\n");
    }
    const char*   out = __glide_string_concat(head, " {\n");
    for (int   i = 0; (i  <  Vector_len__Stmt((s-> impl_methods ))); i++) {
        if ((i  >  0)) {
            (out  =  __glide_string_concat(out, "\n"));
        }
        Stmt   m = Vector_get__Stmt((s-> impl_methods ), i);
        (out  =  __glide_string_concat(out, fmt_fn((&m), (depth  +  1))));
    }
    return __glide_string_concat(__glide_string_concat(out, pad), "}\n");
}

const char*   fmt_match (Stmt*   s, int   depth) {
    const char*   pad = fmt_indent(depth);
    const char*   out = __glide_string_concat(__glide_string_concat(__glide_string_concat(pad, "match "), fmt_expr((s-> scrutinee ))), " {\n");
    const char*   inner = fmt_indent((depth  +  1));
    if (((s-> arms )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__MatchArm((s-> arms ))); i++) {
            MatchArm   a = Vector_get__MatchArm((s-> arms ), i);
            (out  =  __glide_string_concat(__glide_string_concat(out, inner), (a. variant )));
            if ((((a. bindings )  !=  NULL)  &&  (Vector_len__string((a. bindings ))  >  0))) {
                (out  =  __glide_string_concat(out, "("));
                for (int   j = 0; (j  <  Vector_len__string((a. bindings ))); j++) {
                    if ((j  >  0)) {
                        (out  =  __glide_string_concat(out, ", "));
                    }
                    (out  =  __glide_string_concat(out, Vector_get__string((a. bindings ), j)));
                }
                (out  =  __glide_string_concat(out, ")"));
            }
            (out  =  __glide_string_concat(out, " => {\n"));
            if (((a. body )  !=  NULL)) {
                for (int   j = 0; (j  <  Vector_len__Stmt((a. body ))); j++) {
                    Stmt   b = Vector_get__Stmt((a. body ), j);
                    (out  =  __glide_string_concat(out, fmt_stmt((&b), (depth  +  2))));
                }
            }
            (out  =  __glide_string_concat(__glide_string_concat(out, inner), "}\n"));
        }
    }
    return __glide_string_concat(__glide_string_concat(out, pad), "}\n");
}

const char*   fmt_program (Vector__Stmt*   stmts) {
    const char*   out = "";
    int   prev_kind = (-1);
    for (int   i = 0; (i  <  Vector_len__Stmt(stmts)); i++) {
        Stmt   s = Vector_get__Stmt(stmts, i);
        if (((i  >  0)  &&  needs_blank_line(prev_kind, (s. kind )))) {
            (out  =  __glide_string_concat(out, "\n"));
        }
        (out  =  __glide_string_concat(out, fmt_stmt((&s), 0)));
        (prev_kind  =  (s. kind ));
    }
    return out;
}

bool   needs_blank_line (int   prev, int   curr) {
    if (((prev  ==  ST_IMPORT)  &&  (curr  ==  ST_IMPORT))) {
        return false;
    }
    if (((prev  ==  ST_CONST)  &&  (curr  ==  ST_CONST))) {
        return false;
    }
    return true;
}

LspState*   lsp_state_new (void) {
    LspState*   s = (( LspState* )malloc(sizeof( LspState )));
    HashMap__LspDoc*   m = HashMap_new__LspDoc();
    ((s-> docs )  =  m);
    ((s-> shutdown_requested )  =  false);
    return s;
}

const char*   lsp_read_message (void) {
    int   content_length = 0;
    while (true) {
        const char*   line = __glide_read_line();
        if (__glide_string_eq(line, "")) {
            return "";
        }
        int   n = __glide_string_len(line);
        int   end = n;
        while ((end  >  0)) {
            int   c = __glide_char_to_int(__glide_string_at(line, (end  -  1)));
            if (((c  ==  10)  ||  (c  ==  13))) {
                (end  =  (end  -  1));
            } else {
                break;
            }
        }
        if ((end  ==  0)) {
            break;
        }
        const char*   trimmed = __glide_string_substring(line, 0, end);
        if ((__glide_string_len(trimmed)  >  16)) {
            const char*   prefix = __glide_string_substring(trimmed, 0, 16);
            if (__glide_string_eq(prefix, "Content-Length: ")) {
                (content_length  =  parse_int_str(__glide_string_substring(trimmed, 16, __glide_string_len(trimmed))));
            }
        }
    }
    if ((content_length  <=  0)) {
        return "";
    }
    return __glide_read_bytes(content_length);
}

int   parse_int_str (const char*   s) {
    int   n = 0;
    int   len = __glide_string_len(s);
    for (int   i = 0; (i  <  len); i++) {
        int   c = __glide_char_to_int(__glide_string_at(s, i));
        if (((c  <  48)  ||  (c  >  57))) {
            break;
        }
        (n  =  ((n  *  10)  +  (c  -  48)));
    }
    return n;
}

void   lsp_send (const char*   payload) {
    const char*   header = __glide_string_concat(__glide_string_concat("Content-Length: ", int_to_str(__glide_string_len(payload))), "\r\n\r\n");
    __glide_write_str(header);
    __glide_write_str(payload);
    __glide_flush_stdout();
}

void   lsp_send_response (JsonValue*   id, JsonValue*   result) {
    JsonValue*   resp = json_object();
    json_obj_set(resp, "jsonrpc", json_string("2.0"));
    if ((id  !=  NULL)) {
        json_obj_set(resp, "id", id);
    }
    json_obj_set(resp, "result", result);
    lsp_send(json_emit(resp));
}

void   lsp_send_notification (const char*   method, JsonValue*   params) {
    JsonValue*   n = json_object();
    json_obj_set(n, "jsonrpc", json_string("2.0"));
    json_obj_set(n, "method", json_string(method));
    json_obj_set(n, "params", params);
    lsp_send(json_emit(n));
}

void   handle_initialize (JsonValue*   req) {
    JsonValue*   id = json_get(req, "id");
    JsonValue*   result = json_object();
    JsonValue*   caps = json_object();
    json_obj_set(caps, "textDocumentSync", json_int(1));
    json_obj_set(caps, "hoverProvider", json_bool(true));
    json_obj_set(caps, "documentSymbolProvider", json_bool(true));
    json_obj_set(caps, "definitionProvider", json_bool(true));
    json_obj_set(caps, "referencesProvider", json_bool(true));
    json_obj_set(caps, "documentHighlightProvider", json_bool(true));
    json_obj_set(caps, "renameProvider", json_bool(true));
    json_obj_set(caps, "documentFormattingProvider", json_bool(true));
    JsonValue*   comp = json_object();
    JsonValue*   trig = json_array();
    json_arr_push(trig, json_string("."));
    json_arr_push(trig, json_string(":"));
    json_obj_set(comp, "triggerCharacters", trig);
    json_obj_set(caps, "completionProvider", comp);
    json_obj_set(result, "capabilities", caps);
    JsonValue*   info = json_object();
    json_obj_set(info, "name", json_string("glide-lsp"));
    json_obj_set(info, "version", json_string("0.1.0"));
    json_obj_set(result, "serverInfo", info);
    lsp_send_response(id, result);
}

void   handle_shutdown (JsonValue*   req, LspState*   state) {
    ((state-> shutdown_requested )  =  true);
    JsonValue*   id = json_get(req, "id");
    lsp_send_response(id, json_null());
}

JsonValue*   diag_to_json (DiagEntry*   d) {
    JsonValue*   dj = json_object();
    JsonValue*   range = json_object();
    JsonValue*   start = json_object();
    JsonValue*   endp = json_object();
    int   line0 = ((d-> line )  -  1);
    if ((line0  <  0)) {
        (line0  =  0);
    }
    int   col0 = ((d-> col )  -  1);
    if ((col0  <  0)) {
        (col0  =  0);
    }
    int   endl = ((d-> end_line )  -  1);
    if ((endl  <  0)) {
        (endl  =  0);
    }
    int   endc = ((d-> end_col )  -  1);
    if ((endc  <  0)) {
        (endc  =  0);
    }
    json_obj_set(start, "line", json_int(line0));
    json_obj_set(start, "character", json_int(col0));
    json_obj_set(endp, "line", json_int(endl));
    json_obj_set(endp, "character", json_int(endc));
    json_obj_set(range, "start", start);
    json_obj_set(range, "end", endp);
    json_obj_set(dj, "range", range);
    json_obj_set(dj, "severity", json_int((d-> severity )));
    if ((!__glide_string_eq((d-> code ), ""))) {
        json_obj_set(dj, "code", json_string((d-> code )));
    }
    json_obj_set(dj, "source", json_string("glide"));
    json_obj_set(dj, "message", json_string((d-> message )));
    return dj;
}

const char*   uri_to_path (const char*   uri) {
    int   n = __glide_string_len(uri);
    if ((n  <  7)) {
        return uri;
    }
    if ((!__glide_string_eq(__glide_string_substring(uri, 0, 7), "file://"))) {
        return uri;
    }
    const char*   rest = __glide_string_substring(uri, 7, n);
    int   rn = __glide_string_len(rest);
    if ((((rn  >  2)  &&  (__glide_char_to_int(__glide_string_at(rest, 0))  ==  47))  &&  (__glide_char_to_int(__glide_string_at(rest, 2))  ==  58))) {
        return __glide_string_substring(rest, 1, rn);
    }
    return rest;
}

const char*   find_stdlib_root (void) {
    if ((!__glide_string_eq(read_file("stdlib/vector.glide"), ""))) {
        return "";
    }
    const char*   exe_dir = __glide_exe_dir();
    if ((!__glide_string_eq(exe_dir, ""))) {
        const char*   probe = __glide_string_concat(exe_dir, "/stdlib/vector.glide");
        if ((!__glide_string_eq(read_file(probe), ""))) {
            return __glide_string_concat(exe_dir, "/");
        }
    }
    return "";
}

void   run_analysis_and_publish (const char*   uri, const char*   text, LspState*   state) {
    const char*   path = uri_to_path(uri);
    Vector__Stmt*   stmts = Vector_new__Stmt();
    HashMap__bool*   loaded = HashMap_new__bool();
    const char*   prefix = find_stdlib_root();
    load_into(stmts, __glide_string_concat(prefix, "stdlib/vector.glide"), loaded);
    load_into(stmts, __glide_string_concat(prefix, "stdlib/hashmap.glide"), loaded);
    load_into_str(stmts, text, path, loaded);
    Typer*   t = Typer_new();
    check_program(t, stmts);
    run_extra_analyses(t, stmts);
    if (HashMap_contains__LspDoc((state-> docs ), uri)) {
        LspDoc   d = HashMap_get__LspDoc((state-> docs ), uri);
        ((d. stmts )  =  stmts);
        HashMap_insert__LspDoc((state-> docs ), uri, d);
    }
    JsonValue*   arr = json_array();
    for (int   i = 0; (i  <  Vector_len__DiagEntry((t-> diagnostics ))); i++) {
        DiagEntry   de = Vector_get__DiagEntry((t-> diagnostics ), i);
        if ((__glide_string_eq((de. origin ), path)  ||  __glide_string_eq((de. origin ), ""))) {
            json_arr_push(arr, diag_to_json((&de)));
        }
    }
    JsonValue*   params = json_object();
    json_obj_set(params, "uri", json_string(uri));
    json_obj_set(params, "diagnostics", arr);
    lsp_send_notification("textDocument/publishDiagnostics", params);
    Typer_free(t);
}

void   handle_did_open (JsonValue*   req, LspState*   state) {
    JsonValue*   params = json_get(req, "params");
    JsonValue*   td = json_get(params, "textDocument");
    const char*   uri = json_as_string(json_get(td, "uri"));
    const char*   text = json_as_string(json_get(td, "text"));
    int   version = json_as_int(json_get(td, "version"));
    Vector__Stmt*   empty_stmts = Vector_new__Stmt();
    LspDoc   doc = (( LspDoc ){. uri  = uri, . text  = text, . version  = version, . stmts  = empty_stmts});
    HashMap_insert__LspDoc((state-> docs ), uri, doc);
    run_analysis_and_publish(uri, text, state);
}

void   handle_did_change (JsonValue*   req, LspState*   state) {
    JsonValue*   params = json_get(req, "params");
    JsonValue*   td = json_get(params, "textDocument");
    const char*   uri = json_as_string(json_get(td, "uri"));
    int   version = json_as_int(json_get(td, "version"));
    JsonValue*   changes = json_get(params, "contentChanges");
    JsonValue*   first = json_at(changes, 0);
    const char*   text = json_as_string(json_get(first, "text"));
    if (__glide_string_eq(text, "")) {
        return;
    }
    Vector__Stmt*   empty_stmts = Vector_new__Stmt();
    LspDoc   doc = (( LspDoc ){. uri  = uri, . text  = text, . version  = version, . stmts  = empty_stmts});
    HashMap_insert__LspDoc((state-> docs ), uri, doc);
    run_analysis_and_publish(uri, text, state);
}

void   handle_did_close (JsonValue*   req, LspState*   state) {
    JsonValue*   params = json_get(req, "params");
    JsonValue*   td = json_get(params, "textDocument");
    const char*   uri = json_as_string(json_get(td, "uri"));
    HashMap_remove__LspDoc((state-> docs ), uri);
    JsonValue*   p = json_object();
    json_obj_set(p, "uri", json_string(uri));
    json_obj_set(p, "diagnostics", json_array());
    lsp_send_notification("textDocument/publishDiagnostics", p);
}

void   run_extra_analyses (Typer*   t, Vector__Stmt*   stmts) {
    analysis_unused_vars(t, stmts);
    analysis_unused_params(t, stmts);
    analysis_unused_fn(t, stmts);
    analysis_unnecessary_mut(t, stmts);
    analysis_arena_not_freed(t, stmts);
    analysis_addr_of_temporary(t, stmts);
    analysis_dead_code(t, stmts);
    analysis_missing_return(t, stmts);
    analysis_large_return(t, stmts);
}

const char*   type_to_string_pretty (Type*   t) {
    if ((t  ==  NULL)) {
        return "";
    }
    return type_to_string(t);
}

const char*   fn_signature (Stmt*   s) {
    const char*   sig = __glide_string_concat(__glide_string_concat("fn ", (s-> name )), "(");
    if (((s-> fn_params )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Param((s-> fn_params ))); i++) {
            if ((i  >  0)) {
                (sig  =  __glide_string_concat(sig, ", "));
            }
            Param   p = Vector_get__Param((s-> fn_params ), i);
            (sig  =  __glide_string_concat(__glide_string_concat(__glide_string_concat(sig, (p. name )), ": "), type_to_string_pretty((p. ty ))));
        }
    }
    (sig  =  __glide_string_concat(sig, ")"));
    if (((s-> fn_ret_ty )  !=  NULL)) {
        (sig  =  __glide_string_concat(__glide_string_concat(sig, " -> "), type_to_string_pretty((s-> fn_ret_ty ))));
    }
    return sig;
}

Stmt*   find_top_decl (Vector__Stmt*   stmts, const char*   name) {
    if ((stmts  ==  NULL)) {
        return NULL;
    }
    for (int   i = 0; (i  <  Vector_len__Stmt(stmts)); i++) {
        Stmt   s = Vector_get__Stmt(stmts, i);
        if (((((((s. kind )  ==  ST_FN)  ||  ((s. kind )  ==  ST_STRUCT))  ||  ((s. kind )  ==  ST_ENUM))  ||  ((s. kind )  ==  ST_CONST))  &&  __glide_string_eq((s. name ), name))) {
            Stmt*   p = (( Stmt* )malloc(sizeof( Stmt )));
            ((*p)  =  s);
            return p;
        }
    }
    return NULL;
}

const char*   word_at (const char*   text, int   line0, int   col0) {
    Lexer*   lex = Lexer_new(text);
    while (true) {
        Token   tok = Lexer_next_token(lex);
        if (((tok. kind )  ==  TOK_EOF)) {
            return "";
        }
        if ((((tok. kind )  !=  TOK_IDENT)  &&  ((tok. kind )  !=  TOK_KEYWORD))) {
            continue;
        }
        int   tline = ((tok. line )  -  1);
        int   tcol = ((tok. column )  -  1);
        int   len = __glide_string_len((tok. lexeme ));
        if ((((tline  ==  line0)  &&  (col0  >=  tcol))  &&  (col0  <=  (tcol  +  len)))) {
            return (tok. lexeme );
        }
    }
    return "";
}

void   handle_hover (JsonValue*   req, LspState*   state) {
    JsonValue*   id = json_get(req, "id");
    JsonValue*   params = json_get(req, "params");
    JsonValue*   td = json_get(params, "textDocument");
    const char*   uri = json_as_string(json_get(td, "uri"));
    JsonValue*   pos = json_get(params, "position");
    int   line0 = json_as_int(json_get(pos, "line"));
    int   col0 = json_as_int(json_get(pos, "character"));
    if ((!HashMap_contains__LspDoc((state-> docs ), uri))) {
        lsp_send_response(id, json_null());
        return;
    }
    LspDoc   doc = HashMap_get__LspDoc((state-> docs ), uri);
    const char*   word = word_at((doc. text ), line0, col0);
    if (__glide_string_eq(word, "")) {
        lsp_send_response(id, json_null());
        return;
    }
    Stmt*   decl = find_top_decl((doc. stmts ), word);
    const char*   content = "";
    if ((decl  !=  NULL)) {
        if (((decl-> kind )  ==  ST_FN)) {
            (content  =  __glide_string_concat(__glide_string_concat("```glide\n", fn_signature(decl)), "\n```"));
        } else {
            if (((decl-> kind )  ==  ST_STRUCT)) {
                (content  =  __glide_string_concat(__glide_string_concat("```glide\nstruct ", (decl-> name )), "\n```"));
            } else {
                if (((decl-> kind )  ==  ST_ENUM)) {
                    (content  =  __glide_string_concat(__glide_string_concat("```glide\nenum ", (decl-> name )), "\n```"));
                } else {
                    if (((decl-> kind )  ==  ST_CONST)) {
                        (content  =  __glide_string_concat("```glide\nconst ", (decl-> name )));
                        if (((decl-> let_ty )  !=  NULL)) {
                            (content  =  __glide_string_concat(__glide_string_concat(content, ": "), type_to_string_pretty((decl-> let_ty ))));
                        }
                        (content  =  __glide_string_concat(content, "\n```"));
                    }
                }
            }
        }
    }
    if (__glide_string_eq(content, "")) {
        if ((((((((((((((((((((__glide_string_eq(word, "int")  ||  __glide_string_eq(word, "uint"))  ||  __glide_string_eq(word, "long"))  ||  __glide_string_eq(word, "ulong"))  ||  __glide_string_eq(word, "float"))  ||  __glide_string_eq(word, "bool"))  ||  __glide_string_eq(word, "string"))  ||  __glide_string_eq(word, "char"))  ||  __glide_string_eq(word, "usize"))  ||  __glide_string_eq(word, "isize"))  ||  __glide_string_eq(word, "i8"))  ||  __glide_string_eq(word, "i16"))  ||  __glide_string_eq(word, "i32"))  ||  __glide_string_eq(word, "i64"))  ||  __glide_string_eq(word, "u8"))  ||  __glide_string_eq(word, "u16"))  ||  __glide_string_eq(word, "u32"))  ||  __glide_string_eq(word, "u64"))  ||  __glide_string_eq(word, "f32"))  ||  __glide_string_eq(word, "f64"))) {
            (content  =  __glide_string_concat(__glide_string_concat("```glide\n// built-in type: ", word), "\n```"));
        }
    }
    if (__glide_string_eq(content, "")) {
        lsp_send_response(id, json_null());
        return;
    }
    JsonValue*   result = json_object();
    JsonValue*   contents = json_object();
    json_obj_set(contents, "kind", json_string("markdown"));
    json_obj_set(contents, "value", json_string(content));
    json_obj_set(result, "contents", contents);
    lsp_send_response(id, result);
}

int   symbol_kind_for (Stmt*   s) {
    if (((s-> kind )  ==  ST_FN)) {
        return 12;
    }
    if (((s-> kind )  ==  ST_STRUCT)) {
        return 22;
    }
    if (((s-> kind )  ==  ST_ENUM)) {
        return 10;
    }
    if (((s-> kind )  ==  ST_CONST)) {
        return 14;
    }
    return 13;
}

JsonValue*   position_to_json (int   line1, int   col1) {
    JsonValue*   p = json_object();
    int   l = (line1  -  1);
    if ((l  <  0)) {
        (l  =  0);
    }
    int   c = (col1  -  1);
    if ((c  <  0)) {
        (c  =  0);
    }
    json_obj_set(p, "line", json_int(l));
    json_obj_set(p, "character", json_int(c));
    return p;
}

JsonValue*   range_for_stmt (Stmt*   s) {
    JsonValue*   r = json_object();
    json_obj_set(r, "start", position_to_json((s-> line ), (s-> column )));
    json_obj_set(r, "end", position_to_json((s-> line ), ((s-> column )  +  1)));
    return r;
}

void   handle_document_symbol (JsonValue*   req, LspState*   state) {
    JsonValue*   id = json_get(req, "id");
    JsonValue*   params = json_get(req, "params");
    JsonValue*   td = json_get(params, "textDocument");
    const char*   uri = json_as_string(json_get(td, "uri"));
    if ((!HashMap_contains__LspDoc((state-> docs ), uri))) {
        lsp_send_response(id, json_array());
        return;
    }
    LspDoc   doc = HashMap_get__LspDoc((state-> docs ), uri);
    JsonValue*   arr = json_array();
    if (((doc. stmts )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((doc. stmts ))); i++) {
            Stmt   s = Vector_get__Stmt((doc. stmts ), i);
            if ((((((s. kind )  !=  ST_FN)  &&  ((s. kind )  !=  ST_STRUCT))  &&  ((s. kind )  !=  ST_ENUM))  &&  ((s. kind )  !=  ST_CONST))) {
                continue;
            }
            JsonValue*   sym = json_object();
            json_obj_set(sym, "name", json_string((s. name )));
            const char*   detail = "";
            if (((s. kind )  ==  ST_FN)) {
                (detail  =  fn_signature((&s)));
            }
            json_obj_set(sym, "detail", json_string(detail));
            json_obj_set(sym, "kind", json_int(symbol_kind_for((&s))));
            json_obj_set(sym, "range", range_for_stmt((&s)));
            json_obj_set(sym, "selectionRange", range_for_stmt((&s)));
            json_arr_push(arr, sym);
        }
    }
    lsp_send_response(id, arr);
}

void   handle_definition (JsonValue*   req, LspState*   state) {
    JsonValue*   id = json_get(req, "id");
    JsonValue*   params = json_get(req, "params");
    JsonValue*   td = json_get(params, "textDocument");
    const char*   uri = json_as_string(json_get(td, "uri"));
    JsonValue*   pos = json_get(params, "position");
    int   line0 = json_as_int(json_get(pos, "line"));
    int   col0 = json_as_int(json_get(pos, "character"));
    if ((!HashMap_contains__LspDoc((state-> docs ), uri))) {
        lsp_send_response(id, json_null());
        return;
    }
    LspDoc   doc = HashMap_get__LspDoc((state-> docs ), uri);
    const char*   word = word_at((doc. text ), line0, col0);
    if (__glide_string_eq(word, "")) {
        lsp_send_response(id, json_null());
        return;
    }
    Stmt*   decl = find_top_decl((doc. stmts ), word);
    if ((decl  ==  NULL)) {
        lsp_send_response(id, json_null());
        return;
    }
    JsonValue*   loc = json_object();
    json_obj_set(loc, "uri", json_string(uri));
    json_obj_set(loc, "range", range_for_stmt(decl));
    lsp_send_response(id, loc);
}

int   ci_kind_for (Stmt*   s) {
    if (((s-> kind )  ==  ST_FN)) {
        return 3;
    }
    if (((s-> kind )  ==  ST_STRUCT)) {
        return 8;
    }
    if (((s-> kind )  ==  ST_ENUM)) {
        return 13;
    }
    if (((s-> kind )  ==  ST_CONST)) {
        return 21;
    }
    return 6;
}

Stmt*   fn_containing (Vector__Stmt*   stmts, int   line0) {
    if ((stmts  ==  NULL)) {
        return NULL;
    }
    Stmt*   best = NULL;
    int   best_line = (-1);
    for (int   i = 0; (i  <  Vector_len__Stmt(stmts)); i++) {
        Stmt   s = Vector_get__Stmt(stmts, i);
        if ((((s. kind )  !=  ST_FN)  ||  ((s. fn_body )  ==  NULL))) {
            continue;
        }
        int   start = ((s. line )  -  1);
        if (((start  <=  line0)  &&  (start  >  best_line))) {
            (best_line  =  start);
            Stmt*   p = (( Stmt* )malloc(sizeof( Stmt )));
            ((*p)  =  s);
            (best  =  p);
        }
    }
    return best;
}

void   collect_locals (Vector__Stmt*   body, int   before_line, Vector__Stmt*   out) {
    if ((body  ==  NULL)) {
        return;
    }
    for (int   i = 0; (i  <  Vector_len__Stmt(body)); i++) {
        Stmt   s = Vector_get__Stmt(body, i);
        if ((((s. kind )  ==  ST_LET)  &&  (((s. line )  -  1)  <  before_line))) {
            Vector_push__Stmt(out, s);
        }
        if ((((s. kind )  ==  ST_FOR)  &&  ((s. for_init )  !=  NULL))) {
            collect_locals_stmt((s. for_init ), before_line, out);
        }
        if (((s. then_body )  !=  NULL)) {
            collect_locals((s. then_body ), before_line, out);
        }
        if (((s. else_body )  !=  NULL)) {
            collect_locals((s. else_body ), before_line, out);
        }
    }
}

void   collect_locals_stmt (Stmt*   s, int   before_line, Vector__Stmt*   out) {
    if ((s  ==  NULL)) {
        return;
    }
    if ((((s-> kind )  ==  ST_LET)  &&  (((s-> line )  -  1)  <  before_line))) {
        Vector_push__Stmt(out, (*s));
    }
}

JsonValue*   completion_item (const char*   label, int   kind, const char*   detail) {
    JsonValue*   it = json_object();
    json_obj_set(it, "label", json_string(label));
    json_obj_set(it, "kind", json_int(kind));
    if ((!__glide_string_eq(detail, ""))) {
        json_obj_set(it, "detail", json_string(detail));
    }
    return it;
}

void   handle_completion (JsonValue*   req, LspState*   state) {
    JsonValue*   id = json_get(req, "id");
    JsonValue*   params = json_get(req, "params");
    JsonValue*   td = json_get(params, "textDocument");
    const char*   uri = json_as_string(json_get(td, "uri"));
    JsonValue*   pos = json_get(params, "position");
    int   line0 = json_as_int(json_get(pos, "line"));
    int   _col0 = json_as_int(json_get(pos, "character"));
    if ((!HashMap_contains__LspDoc((state-> docs ), uri))) {
        lsp_send_response(id, json_array());
        return;
    }
    LspDoc   doc = HashMap_get__LspDoc((state-> docs ), uri);
    JsonValue*   items = json_array();
    HashMap__bool*   seen = HashMap_new__bool();
    Stmt*   host = fn_containing((doc. stmts ), line0);
    if ((host  !=  NULL)) {
        if (((host-> fn_params )  !=  NULL)) {
            for (int   i = 0; (i  <  Vector_len__Param((host-> fn_params ))); i++) {
                Param   p = Vector_get__Param((host-> fn_params ), i);
                if ((!HashMap_contains__bool(seen, (p. name )))) {
                    HashMap_insert__bool(seen, (p. name ), true);
                    const char*   detail = __glide_string_concat("param: ", type_to_string_pretty((p. ty )));
                    json_arr_push(items, completion_item((p. name ), 6, detail));
                }
            }
        }
        Vector__Stmt*   locals = Vector_new__Stmt();
        collect_locals((host-> fn_body ), line0, locals);
        for (int   i = 0; (i  <  Vector_len__Stmt(locals)); i++) {
            Stmt   s = Vector_get__Stmt(locals, i);
            if (HashMap_contains__bool(seen, (s. name ))) {
                continue;
            }
            HashMap_insert__bool(seen, (s. name ), true);
            const char*   detail = "local";
            if (((s. let_ty )  !=  NULL)) {
                (detail  =  __glide_string_concat(__glide_string_concat(__glide_string_concat("let ", (s. name )), ": "), type_to_string_pretty((s. let_ty ))));
            }
            json_arr_push(items, completion_item((s. name ), 6, detail));
        }
    }
    if (((doc. stmts )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((doc. stmts ))); i++) {
            Stmt   s = Vector_get__Stmt((doc. stmts ), i);
            if ((((((s. kind )  !=  ST_FN)  &&  ((s. kind )  !=  ST_STRUCT))  &&  ((s. kind )  !=  ST_ENUM))  &&  ((s. kind )  !=  ST_CONST))) {
                continue;
            }
            if (HashMap_contains__bool(seen, (s. name ))) {
                continue;
            }
            HashMap_insert__bool(seen, (s. name ), true);
            const char*   detail = "";
            if (((s. kind )  ==  ST_FN)) {
                (detail  =  fn_signature((&s)));
            } else {
                if (((s. kind )  ==  ST_STRUCT)) {
                    (detail  =  __glide_string_concat("struct ", (s. name )));
                } else {
                    if (((s. kind )  ==  ST_ENUM)) {
                        (detail  =  __glide_string_concat("enum ", (s. name )));
                    } else {
                        if (((s. kind )  ==  ST_CONST)) {
                            (detail  =  __glide_string_concat("const ", (s. name )));
                            if (((s. let_ty )  !=  NULL)) {
                                (detail  =  __glide_string_concat(__glide_string_concat(detail, ": "), type_to_string_pretty((s. let_ty ))));
                            }
                        }
                    }
                }
            }
            json_arr_push(items, completion_item((s. name ), ci_kind_for((&s)), detail));
        }
    }
    Vector__string*   kws = Vector_new__string();
    Vector_push__string(kws, "let");
    Vector_push__string(kws, "const");
    Vector_push__string(kws, "mut");
    Vector_push__string(kws, "fn");
    Vector_push__string(kws, "struct");
    Vector_push__string(kws, "enum");
    Vector_push__string(kws, "impl");
    Vector_push__string(kws, "interface");
    Vector_push__string(kws, "if");
    Vector_push__string(kws, "else");
    Vector_push__string(kws, "while");
    Vector_push__string(kws, "for");
    Vector_push__string(kws, "return");
    Vector_push__string(kws, "break");
    Vector_push__string(kws, "continue");
    Vector_push__string(kws, "match");
    Vector_push__string(kws, "defer");
    Vector_push__string(kws, "spawn");
    Vector_push__string(kws, "import");
    Vector_push__string(kws, "pub");
    Vector_push__string(kws, "extern");
    Vector_push__string(kws, "true");
    Vector_push__string(kws, "false");
    Vector_push__string(kws, "null");
    Vector_push__string(kws, "as");
    Vector_push__string(kws, "type");
    Vector_push__string(kws, "move");
    Vector_push__string(kws, "chan");
    for (int   i = 0; (i  <  Vector_len__string(kws)); i++) {
        const char*   k = Vector_get__string(kws, i);
        if (HashMap_contains__bool(seen, k)) {
            continue;
        }
        HashMap_insert__bool(seen, k, true);
        json_arr_push(items, completion_item(k, 14, ""));
    }
    HashMap_free__bool(seen);
    lsp_send_response(id, items);
}

void   collect_uses_in_expr (Expr*   e, const char*   name, Vector__UseSite*   out) {
    if ((e  ==  NULL)) {
        return;
    }
    if ((((e-> kind )  ==  EX_IDENT)  &&  __glide_string_eq((e-> str_val ), name))) {
        UseSite   u = (( UseSite ){. line  = (e-> line ), . col  = (e-> column )});
        Vector_push__UseSite(out, u);
    }
    if ((((((e-> kind )  ==  EX_CALL)  &&  ((e-> lhs )  !=  NULL))  &&  (((e-> lhs )-> kind )  ==  EX_PATH))  &&  __glide_string_eq(((e-> lhs )-> str_val ), name))) {
        UseSite   u = (( UseSite ){. line  = (e-> line ), . col  = (e-> column )});
        Vector_push__UseSite(out, u);
    }
    if (((e-> lhs )  !=  NULL)) {
        collect_uses_in_expr((e-> lhs ), name, out);
    }
    if (((e-> rhs )  !=  NULL)) {
        collect_uses_in_expr((e-> rhs ), name, out);
    }
    if (((e-> operand )  !=  NULL)) {
        collect_uses_in_expr((e-> operand ), name, out);
    }
    if (((e-> args )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
            Expr   a = Vector_get__Expr((e-> args ), i);
            collect_uses_in_expr((&a), name, out);
        }
    }
}

void   collect_uses_in_stmt (Stmt*   s, const char*   name, Vector__UseSite*   out) {
    if ((s  ==  NULL)) {
        return;
    }
    if (((((((s-> kind )  ==  ST_FN)  ||  ((s-> kind )  ==  ST_STRUCT))  ||  ((s-> kind )  ==  ST_ENUM))  ||  ((s-> kind )  ==  ST_CONST))  &&  __glide_string_eq((s-> name ), name))) {
        UseSite   u = (( UseSite ){. line  = (s-> line ), . col  = (s-> column )});
        Vector_push__UseSite(out, u);
    }
    if ((((s-> kind )  ==  ST_LET)  &&  __glide_string_eq((s-> name ), name))) {
        UseSite   u = (( UseSite ){. line  = (s-> line ), . col  = (s-> column )});
        Vector_push__UseSite(out, u);
    }
    if (((s-> let_value )  !=  NULL)) {
        collect_uses_in_expr((s-> let_value ), name, out);
    }
    if (((s-> expr_value )  !=  NULL)) {
        collect_uses_in_expr((s-> expr_value ), name, out);
    }
    if (((s-> cond )  !=  NULL)) {
        collect_uses_in_expr((s-> cond ), name, out);
    }
    if (((s-> for_step )  !=  NULL)) {
        collect_uses_in_expr((s-> for_step ), name, out);
    }
    if (((s-> for_init )  !=  NULL)) {
        collect_uses_in_stmt((s-> for_init ), name, out);
    }
    if (((s-> then_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> then_body ), i);
            collect_uses_in_stmt((&b), name, out);
        }
    }
    if (((s-> else_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> else_body ), i);
            collect_uses_in_stmt((&b), name, out);
        }
    }
    if (((s-> fn_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> fn_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> fn_body ), i);
            collect_uses_in_stmt((&b), name, out);
        }
    }
    if (((s-> impl_methods )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> impl_methods ))); i++) {
            Stmt   m = Vector_get__Stmt((s-> impl_methods ), i);
            collect_uses_in_stmt((&m), name, out);
        }
    }
}

JsonValue*   use_to_range (UseSite*   u, int   name_len) {
    JsonValue*   r = json_object();
    int   l = ((u-> line )  -  1);
    if ((l  <  0)) {
        (l  =  0);
    }
    int   c = ((u-> col )  -  1);
    if ((c  <  0)) {
        (c  =  0);
    }
    JsonValue*   s = json_object();
    json_obj_set(s, "line", json_int(l));
    json_obj_set(s, "character", json_int(c));
    JsonValue*   e = json_object();
    json_obj_set(e, "line", json_int(l));
    json_obj_set(e, "character", json_int((c  +  name_len)));
    json_obj_set(r, "start", s);
    json_obj_set(r, "end", e);
    return r;
}

void   handle_references (JsonValue*   req, LspState*   state) {
    JsonValue*   id = json_get(req, "id");
    JsonValue*   params = json_get(req, "params");
    JsonValue*   td = json_get(params, "textDocument");
    const char*   uri = json_as_string(json_get(td, "uri"));
    JsonValue*   pos = json_get(params, "position");
    int   line0 = json_as_int(json_get(pos, "line"));
    int   col0 = json_as_int(json_get(pos, "character"));
    if ((!HashMap_contains__LspDoc((state-> docs ), uri))) {
        lsp_send_response(id, json_array());
        return;
    }
    LspDoc   doc = HashMap_get__LspDoc((state-> docs ), uri);
    const char*   word = word_at((doc. text ), line0, col0);
    if (__glide_string_eq(word, "")) {
        lsp_send_response(id, json_array());
        return;
    }
    Vector__UseSite*   uses = Vector_new__UseSite();
    if (((doc. stmts )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((doc. stmts ))); i++) {
            Stmt   s = Vector_get__Stmt((doc. stmts ), i);
            collect_uses_in_stmt((&s), word, uses);
        }
    }
    JsonValue*   arr = json_array();
    for (int   i = 0; (i  <  Vector_len__UseSite(uses)); i++) {
        UseSite   u = Vector_get__UseSite(uses, i);
        JsonValue*   loc = json_object();
        json_obj_set(loc, "uri", json_string(uri));
        json_obj_set(loc, "range", use_to_range((&u), __glide_string_len(word)));
        json_arr_push(arr, loc);
    }
    lsp_send_response(id, arr);
}

void   handle_document_highlight (JsonValue*   req, LspState*   state) {
    JsonValue*   id = json_get(req, "id");
    JsonValue*   params = json_get(req, "params");
    JsonValue*   td = json_get(params, "textDocument");
    const char*   uri = json_as_string(json_get(td, "uri"));
    JsonValue*   pos = json_get(params, "position");
    int   line0 = json_as_int(json_get(pos, "line"));
    int   col0 = json_as_int(json_get(pos, "character"));
    if ((!HashMap_contains__LspDoc((state-> docs ), uri))) {
        lsp_send_response(id, json_array());
        return;
    }
    LspDoc   doc = HashMap_get__LspDoc((state-> docs ), uri);
    const char*   word = word_at((doc. text ), line0, col0);
    if (__glide_string_eq(word, "")) {
        lsp_send_response(id, json_array());
        return;
    }
    Vector__UseSite*   uses = Vector_new__UseSite();
    if (((doc. stmts )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((doc. stmts ))); i++) {
            Stmt   s = Vector_get__Stmt((doc. stmts ), i);
            collect_uses_in_stmt((&s), word, uses);
        }
    }
    JsonValue*   arr = json_array();
    for (int   i = 0; (i  <  Vector_len__UseSite(uses)); i++) {
        UseSite   u = Vector_get__UseSite(uses, i);
        JsonValue*   h = json_object();
        json_obj_set(h, "range", use_to_range((&u), __glide_string_len(word)));
        json_obj_set(h, "kind", json_int(1));
        json_arr_push(arr, h);
    }
    lsp_send_response(id, arr);
}

void   handle_rename (JsonValue*   req, LspState*   state) {
    JsonValue*   id = json_get(req, "id");
    JsonValue*   params = json_get(req, "params");
    JsonValue*   td = json_get(params, "textDocument");
    const char*   uri = json_as_string(json_get(td, "uri"));
    JsonValue*   pos = json_get(params, "position");
    int   line0 = json_as_int(json_get(pos, "line"));
    int   col0 = json_as_int(json_get(pos, "character"));
    const char*   new_name = json_as_string(json_get(params, "newName"));
    if (((!HashMap_contains__LspDoc((state-> docs ), uri))  ||  __glide_string_eq(new_name, ""))) {
        lsp_send_response(id, json_null());
        return;
    }
    LspDoc   doc = HashMap_get__LspDoc((state-> docs ), uri);
    const char*   word = word_at((doc. text ), line0, col0);
    if (__glide_string_eq(word, "")) {
        lsp_send_response(id, json_null());
        return;
    }
    Vector__UseSite*   uses = Vector_new__UseSite();
    if (((doc. stmts )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((doc. stmts ))); i++) {
            Stmt   s = Vector_get__Stmt((doc. stmts ), i);
            collect_uses_in_stmt((&s), word, uses);
        }
    }
    JsonValue*   edits = json_array();
    for (int   i = 0; (i  <  Vector_len__UseSite(uses)); i++) {
        UseSite   u = Vector_get__UseSite(uses, i);
        JsonValue*   edit = json_object();
        json_obj_set(edit, "range", use_to_range((&u), __glide_string_len(word)));
        json_obj_set(edit, "newText", json_string(new_name));
        json_arr_push(edits, edit);
    }
    JsonValue*   changes = json_object();
    json_obj_set(changes, uri, edits);
    JsonValue*   result = json_object();
    json_obj_set(result, "changes", changes);
    lsp_send_response(id, result);
}

int   count_lines (const char*   s) {
    int   n = 0;
    int   len = __glide_string_len(s);
    for (int   i = 0; (i  <  len); i++) {
        if ((__glide_char_to_int(__glide_string_at(s, i))  ==  10)) {
            (n  =  (n  +  1));
        }
    }
    return n;
}

void   handle_formatting (JsonValue*   req, LspState*   state) {
    JsonValue*   id = json_get(req, "id");
    JsonValue*   params = json_get(req, "params");
    JsonValue*   td = json_get(params, "textDocument");
    const char*   uri = json_as_string(json_get(td, "uri"));
    if ((!HashMap_contains__LspDoc((state-> docs ), uri))) {
        lsp_send_response(id, json_array());
        return;
    }
    LspDoc   doc = HashMap_get__LspDoc((state-> docs ), uri);
    Lexer*   lex = Lexer_new((doc. text ));
    Parser*   p = Parser_new(lex);
    Vector__Stmt*   parsed = parse_program(p);
    const char*   formatted = fmt_program(parsed);
    int   lines = (count_lines((doc. text ))  +  2);
    JsonValue*   edit = json_object();
    JsonValue*   range = json_object();
    JsonValue*   start = json_object();
    JsonValue*   endp = json_object();
    json_obj_set(start, "line", json_int(0));
    json_obj_set(start, "character", json_int(0));
    json_obj_set(endp, "line", json_int(lines));
    json_obj_set(endp, "character", json_int(0));
    json_obj_set(range, "start", start);
    json_obj_set(range, "end", endp);
    json_obj_set(edit, "range", range);
    json_obj_set(edit, "newText", json_string(formatted));
    JsonValue*   arr = json_array();
    json_arr_push(arr, edit);
    lsp_send_response(id, arr);
}

void   analysis_unused_vars (Typer*   t, Vector__Stmt*   program) {
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if ((((s. kind )  !=  ST_FN)  ||  ((s. fn_body )  ==  NULL))) {
            continue;
        }
        ((t-> current_origin )  =  (s. origin ));
        check_unused_in_body(t, (s. fn_body ));
    }
}

void   check_unused_in_body (Typer*   t, Vector__Stmt*   body) {
    int   n = Vector_len__Stmt(body);
    for (int   i = 0; (i  <  n); i++) {
        Stmt   s = Vector_get__Stmt(body, i);
        if (((s. kind )  ==  ST_LET)) {
            const char*   name = (s. name );
            if (((__glide_string_len(name)  >  0)  &&  (__glide_char_to_int(__glide_string_at(name, 0))  ==  95))) {
                continue;
            }
            bool   used = false;
            for (int   j = (i  +  1); (j  <  n); j++) {
                Stmt   s2 = Vector_get__Stmt(body, j);
                if (stmt_uses_name((&s2), name)) {
                    (used  =  true);
                    break;
                }
            }
            if ((!used)) {
                Typer_warn(t, (s. line ), (s. column ), "unused-var", __glide_string_concat(__glide_string_concat("unused local `", name), "` (prefix with `_` to silence)"));
            }
        }
        if (((s. then_body )  !=  NULL)) {
            check_unused_in_body(t, (s. then_body ));
        }
        if (((s. else_body )  !=  NULL)) {
            check_unused_in_body(t, (s. else_body ));
        }
    }
}

bool   stmt_uses_name (Stmt*   s, const char*   name) {
    if ((s  ==  NULL)) {
        return false;
    }
    if ((((s-> let_value )  !=  NULL)  &&  expr_uses_name((s-> let_value ), name))) {
        return true;
    }
    if ((((s-> expr_value )  !=  NULL)  &&  expr_uses_name((s-> expr_value ), name))) {
        return true;
    }
    if ((((s-> cond )  !=  NULL)  &&  expr_uses_name((s-> cond ), name))) {
        return true;
    }
    if ((((s-> for_step )  !=  NULL)  &&  expr_uses_name((s-> for_step ), name))) {
        return true;
    }
    if ((((s-> for_init )  !=  NULL)  &&  stmt_uses_name((s-> for_init ), name))) {
        return true;
    }
    if (((s-> then_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> then_body ), i);
            if (stmt_uses_name((&b), name)) {
                return true;
            }
        }
    }
    if (((s-> else_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> else_body ), i);
            if (stmt_uses_name((&b), name)) {
                return true;
            }
        }
    }
    return false;
}

bool   expr_uses_name (Expr*   e, const char*   name) {
    if ((e  ==  NULL)) {
        return false;
    }
    if ((((e-> kind )  ==  EX_IDENT)  &&  __glide_string_eq((e-> str_val ), name))) {
        return true;
    }
    if ((((e-> lhs )  !=  NULL)  &&  expr_uses_name((e-> lhs ), name))) {
        return true;
    }
    if ((((e-> rhs )  !=  NULL)  &&  expr_uses_name((e-> rhs ), name))) {
        return true;
    }
    if ((((e-> operand )  !=  NULL)  &&  expr_uses_name((e-> operand ), name))) {
        return true;
    }
    if (((e-> args )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
            Expr   a = Vector_get__Expr((e-> args ), i);
            if (expr_uses_name((&a), name)) {
                return true;
            }
        }
    }
    return false;
}

void   analysis_arena_not_freed (Typer*   t, Vector__Stmt*   program) {
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if ((((s. kind )  !=  ST_FN)  ||  ((s. fn_body )  ==  NULL))) {
            continue;
        }
        ((t-> current_origin )  =  (s. origin ));
        check_arena_in_body(t, (s. fn_body ));
    }
}

void   check_arena_in_body (Typer*   t, Vector__Stmt*   body) {
    int   n = Vector_len__Stmt(body);
    for (int   i = 0; (i  <  n); i++) {
        Stmt   s = Vector_get__Stmt(body, i);
        if ((((s. kind )  ==  ST_LET)  &&  (!(s. is_auto_owned )))) {
            Expr*   v = (s. let_value );
            if (((((((v  !=  NULL)  &&  ((v-> kind )  ==  EX_CALL))  &&  ((v-> lhs )  !=  NULL))  &&  (((v-> lhs )-> kind )  ==  EX_PATH))  &&  __glide_string_eq(((v-> lhs )-> str_val ), "Arena"))  &&  __glide_string_eq(((v-> lhs )-> field ), "new"))) {
                if ((!arena_is_freed_in_body((s. name ), body, (i  +  1)))) {
                    Typer_warn(t, (s. line ), (s. column ), "arena-not-freed", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("arena `", (s. name )), "` is never freed (use `let "), (s. name )), "* = Arena::new(...)` or add `defer "), (s. name )), ".free()`)"));
                }
            }
        }
        if (((s. then_body )  !=  NULL)) {
            check_arena_in_body(t, (s. then_body ));
        }
        if (((s. else_body )  !=  NULL)) {
            check_arena_in_body(t, (s. else_body ));
        }
    }
}

bool   arena_is_freed_in_body (const char*   name, Vector__Stmt*   body, int   start) {
    int   n = Vector_len__Stmt(body);
    for (int   i = start; (i  <  n); i++) {
        Stmt   s = Vector_get__Stmt(body, i);
        if (stmt_calls_method((&s), name, "free")) {
            return true;
        }
    }
    return false;
}

bool   stmt_calls_method (Stmt*   s, const char*   var, const char*   method) {
    if ((s  ==  NULL)) {
        return false;
    }
    if ((((s-> expr_value )  !=  NULL)  &&  expr_calls_method((s-> expr_value ), var, method))) {
        return true;
    }
    if ((((s-> let_value )  !=  NULL)  &&  expr_calls_method((s-> let_value ), var, method))) {
        return true;
    }
    if ((((s-> cond )  !=  NULL)  &&  expr_calls_method((s-> cond ), var, method))) {
        return true;
    }
    if ((((s-> for_step )  !=  NULL)  &&  expr_calls_method((s-> for_step ), var, method))) {
        return true;
    }
    if (((s-> then_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> then_body ), i);
            if (stmt_calls_method((&b), var, method)) {
                return true;
            }
        }
    }
    if (((s-> else_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> else_body ), i);
            if (stmt_calls_method((&b), var, method)) {
                return true;
            }
        }
    }
    return false;
}

bool   expr_calls_method (Expr*   e, const char*   var, const char*   method) {
    if ((e  ==  NULL)) {
        return false;
    }
    if (((((((((e-> kind )  ==  EX_CALL)  &&  ((e-> lhs )  !=  NULL))  &&  (((e-> lhs )-> kind )  ==  EX_MEMBER))  &&  (((e-> lhs )-> lhs )  !=  NULL))  &&  ((((e-> lhs )-> lhs )-> kind )  ==  EX_IDENT))  &&  __glide_string_eq((((e-> lhs )-> lhs )-> str_val ), var))  &&  __glide_string_eq(((e-> lhs )-> field ), method))) {
        return true;
    }
    if ((((e-> lhs )  !=  NULL)  &&  expr_calls_method((e-> lhs ), var, method))) {
        return true;
    }
    if ((((e-> rhs )  !=  NULL)  &&  expr_calls_method((e-> rhs ), var, method))) {
        return true;
    }
    if ((((e-> operand )  !=  NULL)  &&  expr_calls_method((e-> operand ), var, method))) {
        return true;
    }
    if (((e-> args )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
            Expr   a = Vector_get__Expr((e-> args ), i);
            if (expr_calls_method((&a), var, method)) {
                return true;
            }
        }
    }
    return false;
}

void   analysis_addr_of_temporary (Typer*   t, Vector__Stmt*   program) {
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if ((((s. kind )  ==  ST_FN)  &&  ((s. fn_body )  !=  NULL))) {
            ((t-> current_origin )  =  (s. origin ));
            check_addr_temp_body(t, (s. fn_body ));
        }
        if ((((s. kind )  ==  ST_IMPL)  &&  ((s. impl_methods )  !=  NULL))) {
            ((t-> current_origin )  =  (s. origin ));
            for (int   j = 0; (j  <  Vector_len__Stmt((s. impl_methods ))); j++) {
                Stmt   m = Vector_get__Stmt((s. impl_methods ), j);
                if (((m. fn_body )  !=  NULL)) {
                    check_addr_temp_body(t, (m. fn_body ));
                }
            }
        }
    }
}

void   check_addr_temp_body (Typer*   t, Vector__Stmt*   body) {
    for (int   i = 0; (i  <  Vector_len__Stmt(body)); i++) {
        Stmt   s = Vector_get__Stmt(body, i);
        if ((((s. kind )  ==  ST_RETURN)  &&  ((s. expr_value )  !=  NULL))) {
            Expr*   e = (s. expr_value );
            if ((((((e-> kind )  ==  EX_UNARY)  &&  (((e-> op_code )  ==  UN_ADDR)  ||  ((e-> op_code )  ==  UN_ADDR_MUT)))  &&  ((e-> operand )  !=  NULL))  &&  ((((e-> operand )-> kind )  ==  EX_STRUCT_LIT)  ||  (((e-> operand )-> kind )  ==  EX_NEW)))) {
                Typer_err_code(t, (s. line ), (s. column ), "addr-of-temporary", "cannot return address of a temporary value (would dangle); use `new T { ... }` to heap-allocate or return by value");
            }
        }
        if (((s. then_body )  !=  NULL)) {
            check_addr_temp_body(t, (s. then_body ));
        }
        if (((s. else_body )  !=  NULL)) {
            check_addr_temp_body(t, (s. else_body ));
        }
    }
}

void   analysis_dead_code (Typer*   t, Vector__Stmt*   program) {
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if ((((s. kind )  ==  ST_FN)  &&  ((s. fn_body )  !=  NULL))) {
            ((t-> current_origin )  =  (s. origin ));
            check_dead_code_body(t, (s. fn_body ));
        }
        if ((((s. kind )  ==  ST_IMPL)  &&  ((s. impl_methods )  !=  NULL))) {
            ((t-> current_origin )  =  (s. origin ));
            for (int   j = 0; (j  <  Vector_len__Stmt((s. impl_methods ))); j++) {
                Stmt   m = Vector_get__Stmt((s. impl_methods ), j);
                if (((m. fn_body )  !=  NULL)) {
                    check_dead_code_body(t, (m. fn_body ));
                }
            }
        }
    }
}

void   check_dead_code_body (Typer*   t, Vector__Stmt*   body) {
    int   n = Vector_len__Stmt(body);
    bool   terminated = false;
    for (int   i = 0; (i  <  n); i++) {
        Stmt   s = Vector_get__Stmt(body, i);
        if (terminated) {
            Typer_warn(t, (s. line ), (s. column ), "dead-code", "unreachable code after return / break / continue");
            return;
        }
        if (((((s. kind )  ==  ST_RETURN)  ||  ((s. kind )  ==  ST_BREAK))  ||  ((s. kind )  ==  ST_CONTINUE))) {
            (terminated  =  true);
        }
        if (((s. then_body )  !=  NULL)) {
            check_dead_code_body(t, (s. then_body ));
        }
        if (((s. else_body )  !=  NULL)) {
            check_dead_code_body(t, (s. else_body ));
        }
    }
}

void   analysis_unused_fn (Typer*   t, Vector__Stmt*   program) {
    HashMap__bool*   called = HashMap_new__bool();
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if ((((s. kind )  ==  ST_FN)  &&  ((s. fn_body )  !=  NULL))) {
            collect_calls_body((s. fn_body ), called);
        }
        if ((((s. kind )  ==  ST_IMPL)  &&  ((s. impl_methods )  !=  NULL))) {
            for (int   j = 0; (j  <  Vector_len__Stmt((s. impl_methods ))); j++) {
                Stmt   m = Vector_get__Stmt((s. impl_methods ), j);
                if (((m. fn_body )  !=  NULL)) {
                    collect_calls_body((m. fn_body ), called);
                }
            }
        }
    }
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if (((s. kind )  !=  ST_FN)) {
            continue;
        }
        if (((s. fn_body )  ==  NULL)) {
            continue;
        }
        if ((s. is_pub )) {
            continue;
        }
        if (__glide_string_eq((s. name ), "main")) {
            continue;
        }
        if ((((s. type_params )  !=  NULL)  &&  (Vector_len__string((s. type_params ))  >  0))) {
            continue;
        }
        if (HashMap_contains__bool(called, (s. name ))) {
            continue;
        }
        ((t-> current_origin )  =  (s. origin ));
        Typer_warn(t, (s. line ), (s. column ), "unused-fn", __glide_string_concat(__glide_string_concat("function `", (s. name )), "` is never called"));
    }
    HashMap_free__bool(called);
}

void   collect_calls_body (Vector__Stmt*   body, HashMap__bool*   out) {
    for (int   i = 0; (i  <  Vector_len__Stmt(body)); i++) {
        Stmt   s = Vector_get__Stmt(body, i);
        collect_calls_stmt((&s), out);
    }
}

void   collect_calls_stmt (Stmt*   s, HashMap__bool*   out) {
    if ((s  ==  NULL)) {
        return;
    }
    if (((s-> let_value )  !=  NULL)) {
        collect_calls_expr((s-> let_value ), out);
    }
    if (((s-> expr_value )  !=  NULL)) {
        collect_calls_expr((s-> expr_value ), out);
    }
    if (((s-> cond )  !=  NULL)) {
        collect_calls_expr((s-> cond ), out);
    }
    if (((s-> for_step )  !=  NULL)) {
        collect_calls_expr((s-> for_step ), out);
    }
    if (((s-> for_init )  !=  NULL)) {
        collect_calls_stmt((s-> for_init ), out);
    }
    if (((s-> then_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> then_body ), i);
            collect_calls_stmt((&b), out);
        }
    }
    if (((s-> else_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> else_body ), i);
            collect_calls_stmt((&b), out);
        }
    }
}

void   collect_calls_expr (Expr*   e, HashMap__bool*   out) {
    if ((e  ==  NULL)) {
        return;
    }
    if ((((e-> kind )  ==  EX_CALL)  &&  ((e-> lhs )  !=  NULL))) {
        if ((((e-> lhs )-> kind )  ==  EX_IDENT)) {
            HashMap_insert__bool(out, ((e-> lhs )-> str_val ), true);
        }
        if ((((e-> lhs )-> kind )  ==  EX_PATH)) {
            HashMap_insert__bool(out, __glide_string_concat(__glide_string_concat(((e-> lhs )-> str_val ), "_"), ((e-> lhs )-> field )), true);
            HashMap_insert__bool(out, ((e-> lhs )-> field ), true);
        }
        if ((((e-> lhs )-> kind )  ==  EX_MEMBER)) {
            HashMap_insert__bool(out, ((e-> lhs )-> field ), true);
        }
    }
    if (((e-> lhs )  !=  NULL)) {
        collect_calls_expr((e-> lhs ), out);
    }
    if (((e-> rhs )  !=  NULL)) {
        collect_calls_expr((e-> rhs ), out);
    }
    if (((e-> operand )  !=  NULL)) {
        collect_calls_expr((e-> operand ), out);
    }
    if (((e-> args )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
            Expr   a = Vector_get__Expr((e-> args ), i);
            collect_calls_expr((&a), out);
        }
    }
}

void   analysis_unnecessary_mut (Typer*   t, Vector__Stmt*   program) {
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if ((((s. kind )  ==  ST_FN)  &&  ((s. fn_body )  !=  NULL))) {
            ((t-> current_origin )  =  (s. origin ));
            check_mut_body(t, (s. fn_body ));
        }
        if ((((s. kind )  ==  ST_IMPL)  &&  ((s. impl_methods )  !=  NULL))) {
            ((t-> current_origin )  =  (s. origin ));
            for (int   j = 0; (j  <  Vector_len__Stmt((s. impl_methods ))); j++) {
                Stmt   m = Vector_get__Stmt((s. impl_methods ), j);
                if (((m. fn_body )  !=  NULL)) {
                    check_mut_body(t, (m. fn_body ));
                }
            }
        }
    }
}

void   check_mut_body (Typer*   t, Vector__Stmt*   body) {
    int   n = Vector_len__Stmt(body);
    for (int   i = 0; (i  <  n); i++) {
        Stmt   s = Vector_get__Stmt(body, i);
        if ((((s. kind )  ==  ST_LET)  &&  (s. is_mut ))) {
            bool   reassigned = false;
            for (int   j = (i  +  1); (j  <  n); j++) {
                Stmt   s2 = Vector_get__Stmt(body, j);
                if (stmt_reassigns((&s2), (s. name ))) {
                    (reassigned  =  true);
                    break;
                }
            }
            if ((!reassigned)) {
                Typer_warn(t, (s. line ), (s. column ), "unnecessary-mut", __glide_string_concat(__glide_string_concat("`mut` on `", (s. name )), "` is unnecessary; binding is never reassigned"));
            }
        }
        if (((s. then_body )  !=  NULL)) {
            check_mut_body(t, (s. then_body ));
        }
        if (((s. else_body )  !=  NULL)) {
            check_mut_body(t, (s. else_body ));
        }
    }
}

bool   stmt_reassigns (Stmt*   s, const char*   name) {
    if ((s  ==  NULL)) {
        return false;
    }
    if ((((s-> expr_value )  !=  NULL)  &&  expr_reassigns((s-> expr_value ), name))) {
        return true;
    }
    if ((((s-> let_value )  !=  NULL)  &&  expr_reassigns((s-> let_value ), name))) {
        return true;
    }
    if ((((s-> cond )  !=  NULL)  &&  expr_reassigns((s-> cond ), name))) {
        return true;
    }
    if ((((s-> for_step )  !=  NULL)  &&  expr_reassigns((s-> for_step ), name))) {
        return true;
    }
    if (((s-> then_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> then_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> then_body ), i);
            if (stmt_reassigns((&b), name)) {
                return true;
            }
        }
    }
    if (((s-> else_body )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> else_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> else_body ), i);
            if (stmt_reassigns((&b), name)) {
                return true;
            }
        }
    }
    return false;
}

bool   expr_reassigns (Expr*   e, const char*   name) {
    if ((e  ==  NULL)) {
        return false;
    }
    if ((((((e-> kind )  ==  EX_ASSIGN)  &&  ((e-> lhs )  !=  NULL))  &&  (((e-> lhs )-> kind )  ==  EX_IDENT))  &&  __glide_string_eq(((e-> lhs )-> str_val ), name))) {
        return true;
    }
    if (((((((e-> kind )  ==  EX_POSTINC)  ||  ((e-> kind )  ==  EX_POSTDEC))  &&  ((e-> lhs )  !=  NULL))  &&  (((e-> lhs )-> kind )  ==  EX_IDENT))  &&  __glide_string_eq(((e-> lhs )-> str_val ), name))) {
        return true;
    }
    if ((((e-> lhs )  !=  NULL)  &&  expr_reassigns((e-> lhs ), name))) {
        return true;
    }
    if ((((e-> rhs )  !=  NULL)  &&  expr_reassigns((e-> rhs ), name))) {
        return true;
    }
    if ((((e-> operand )  !=  NULL)  &&  expr_reassigns((e-> operand ), name))) {
        return true;
    }
    if (((e-> args )  !=  NULL)) {
        for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
            Expr   a = Vector_get__Expr((e-> args ), i);
            if (expr_reassigns((&a), name)) {
                return true;
            }
        }
    }
    return false;
}

void   analysis_missing_return (Typer*   t, Vector__Stmt*   program) {
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if (((s. kind )  !=  ST_FN)) {
            continue;
        }
        if (((s. fn_body )  ==  NULL)) {
            continue;
        }
        if (((s. fn_ret_ty )  ==  NULL)) {
            continue;
        }
        if ((!body_always_returns((s. fn_body )))) {
            ((t-> current_origin )  =  (s. origin ));
            Typer_warn(t, (s. line ), (s. column ), "missing-return", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("function `", (s. name )), "` declared `-> "), type_to_string((s. fn_ret_ty ))), "` may exit without returning a value"));
        }
    }
}

bool   body_always_returns (Vector__Stmt*   body) {
    int   n = Vector_len__Stmt(body);
    if ((n  ==  0)) {
        return false;
    }
    Stmt   last = Vector_get__Stmt(body, (n  -  1));
    return stmt_terminates((&last));
}

bool   stmt_terminates (Stmt*   s) {
    if ((s  ==  NULL)) {
        return false;
    }
    if (((s-> kind )  ==  ST_RETURN)) {
        return true;
    }
    if ((((s-> kind )  ==  ST_BREAK)  ||  ((s-> kind )  ==  ST_CONTINUE))) {
        return true;
    }
    if (((s-> kind )  ==  ST_BLOCK)) {
        if (((s-> then_body )  !=  NULL)) {
            return body_always_returns((s-> then_body ));
        }
        return false;
    }
    if (((s-> kind )  ==  ST_IF)) {
        if ((((s-> then_body )  !=  NULL)  &&  ((s-> else_body )  !=  NULL))) {
            return (body_always_returns((s-> then_body ))  &&  body_always_returns((s-> else_body )));
        }
        return false;
    }
    if (((s-> kind )  ==  ST_MATCH)) {
        if (((s-> arms )  ==  NULL)) {
            return false;
        }
        bool   all = true;
        bool   has_default = false;
        for (int   i = 0; (i  <  Vector_len__MatchArm((s-> arms ))); i++) {
            MatchArm   a = Vector_get__MatchArm((s-> arms ), i);
            if (__glide_string_eq((a. variant ), "_")) {
                (has_default  =  true);
            }
            if ((((a. body )  ==  NULL)  ||  (!body_always_returns((a. body ))))) {
                (all  =  false);
                break;
            }
        }
        return (all  &&  has_default);
    }
    return false;
}

void   analysis_unused_params (Typer*   t, Vector__Stmt*   program) {
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if (((s. kind )  ==  ST_FN)) {
            check_unused_params_fn(t, (&s));
        }
        if ((((s. kind )  ==  ST_IMPL)  &&  ((s. impl_methods )  !=  NULL))) {
            for (int   j = 0; (j  <  Vector_len__Stmt((s. impl_methods ))); j++) {
                Stmt   m = Vector_get__Stmt((s. impl_methods ), j);
                check_unused_params_fn(t, (&m));
            }
        }
    }
}

void   check_unused_params_fn (Typer*   t, Stmt*   fnstmt) {
    if ((fnstmt  ==  NULL)) {
        return;
    }
    if (((fnstmt-> fn_body )  ==  NULL)) {
        return;
    }
    if (((fnstmt-> fn_params )  ==  NULL)) {
        return;
    }
    for (int   i = 0; (i  <  Vector_len__Param((fnstmt-> fn_params ))); i++) {
        Param   p = Vector_get__Param((fnstmt-> fn_params ), i);
        if (__glide_string_eq((p. name ), "self")) {
            continue;
        }
        if (((__glide_string_len((p. name ))  >  0)  &&  (__glide_char_to_int(__glide_string_at((p. name ), 0))  ==  95))) {
            continue;
        }
        bool   used = false;
        for (int   j = 0; (j  <  Vector_len__Stmt((fnstmt-> fn_body ))); j++) {
            Stmt   b = Vector_get__Stmt((fnstmt-> fn_body ), j);
            if (stmt_uses_name((&b), (p. name ))) {
                (used  =  true);
                break;
            }
        }
        if ((!used)) {
            ((t-> current_origin )  =  (fnstmt-> origin ));
            Typer_warn(t, (fnstmt-> line ), (fnstmt-> column ), "unused-param", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("parameter `", (p. name )), "` of `"), (fnstmt-> name )), "` is never used (prefix with `_` to silence)"));
        }
    }
}

int   type_size_bytes (Type*   t, HashMap__Stmt*   structs) {
    if ((t  ==  NULL)) {
        return 0;
    }
    if (((((t-> kind )  ==  TY_POINTER)  ||  ((t-> kind )  ==  TY_BORROW))  ||  ((t-> kind )  ==  TY_BORROW_MUT))) {
        return 8;
    }
    if (((t-> kind )  ==  TY_FNPTR)) {
        return 8;
    }
    if (((t-> kind )  ==  TY_SLICE)) {
        return 16;
    }
    if (((t-> kind )  ==  TY_RESULT)) {
        return (type_size_bytes((t-> inner ), structs)  +  16);
    }
    if (((t-> kind )  ==  TY_GENERIC)) {
        return 16;
    }
    if (((t-> kind )  ==  TY_NAMED)) {
        const char*   n = (t-> name );
        if ((((__glide_string_eq(n, "char")  ||  __glide_string_eq(n, "bool"))  ||  __glide_string_eq(n, "i8"))  ||  __glide_string_eq(n, "u8"))) {
            return 1;
        }
        if ((__glide_string_eq(n, "i16")  ||  __glide_string_eq(n, "u16"))) {
            return 2;
        }
        if (((((__glide_string_eq(n, "int")  ||  __glide_string_eq(n, "uint"))  ||  __glide_string_eq(n, "i32"))  ||  __glide_string_eq(n, "u32"))  ||  __glide_string_eq(n, "f32"))) {
            return 4;
        }
        if ((((((((__glide_string_eq(n, "long")  ||  __glide_string_eq(n, "ulong"))  ||  __glide_string_eq(n, "i64"))  ||  __glide_string_eq(n, "u64"))  ||  __glide_string_eq(n, "usize"))  ||  __glide_string_eq(n, "isize"))  ||  __glide_string_eq(n, "float"))  ||  __glide_string_eq(n, "f64"))) {
            return 8;
        }
        if (__glide_string_eq(n, "string")) {
            return 8;
        }
        if (__glide_string_eq(n, "void")) {
            return 0;
        }
        if (HashMap_contains__Stmt(structs, n)) {
            Stmt   sd = HashMap_get__Stmt(structs, n);
            int   sz = 0;
            if (((sd. struct_fields )  !=  NULL)) {
                for (int   i = 0; (i  <  Vector_len__Field((sd. struct_fields ))); i++) {
                    Field   f = Vector_get__Field((sd. struct_fields ), i);
                    (sz  =  (sz  +  type_size_bytes((f. ty ), structs)));
                }
            }
            return sz;
        }
    }
    return 0;
}

void   analysis_large_return (Typer*   t, Vector__Stmt*   program) {
    HashMap__Stmt*   structs = HashMap_new__Stmt();
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if (((s. kind )  ==  ST_STRUCT)) {
            HashMap_insert__Stmt(structs, (s. name ), s);
        }
    }
    int   threshold = 64;
    for (int   i = 0; (i  <  Vector_len__Stmt(program)); i++) {
        Stmt   s = Vector_get__Stmt(program, i);
        if (((s. kind )  !=  ST_FN)) {
            continue;
        }
        if (((s. fn_body )  ==  NULL)) {
            continue;
        }
        Type*   ret = (s. fn_ret_ty );
        if ((ret  ==  NULL)) {
            continue;
        }
        if ((((((ret-> kind )  ==  TY_POINTER)  ||  ((ret-> kind )  ==  TY_BORROW))  ||  ((ret-> kind )  ==  TY_BORROW_MUT))  ||  ((ret-> kind )  ==  TY_FNPTR))) {
            continue;
        }
        int   sz = type_size_bytes(ret, structs);
        if ((sz  >=  threshold)) {
            ((t-> current_origin )  =  (s. origin ));
            Typer_push_diag(t, (s. line ), (s. column ), 3, "large-return", __glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat(__glide_string_concat("fn `", (s. name )), "` returns "), int_to_str(sz)), " bytes by value ("), type_to_string(ret)), "); for hot paths consider an out-param `out: *"), type_to_string(ret)), "` or returning `*"), type_to_string(ret)), "`"));
        }
    }
    HashMap_free__Stmt(structs);
}

int   lsp_main (void) {
    __glide_set_binary_io();
    LspState*   state = lsp_state_new();
    while (true) {
        const char*   raw = lsp_read_message();
        if (__glide_string_eq(raw, "")) {
            break;
        }
        JsonValue*   req = json_parse(raw);
        if ((req  ==  NULL)) {
            continue;
        }
        const char*   method = json_as_string(json_get(req, "method"));
        if (__glide_string_eq(method, "initialize")) {
            handle_initialize(req);
            continue;
        }
        if (__glide_string_eq(method, "initialized")) {
            continue;
        }
        if (__glide_string_eq(method, "shutdown")) {
            handle_shutdown(req, state);
            continue;
        }
        if (__glide_string_eq(method, "exit")) {
            break;
        }
        if (__glide_string_eq(method, "textDocument/didOpen")) {
            handle_did_open(req, state);
            continue;
        }
        if (__glide_string_eq(method, "textDocument/didChange")) {
            handle_did_change(req, state);
            continue;
        }
        if (__glide_string_eq(method, "textDocument/didClose")) {
            handle_did_close(req, state);
            continue;
        }
        if (__glide_string_eq(method, "textDocument/hover")) {
            handle_hover(req, state);
            continue;
        }
        if (__glide_string_eq(method, "textDocument/documentSymbol")) {
            handle_document_symbol(req, state);
            continue;
        }
        if (__glide_string_eq(method, "textDocument/definition")) {
            handle_definition(req, state);
            continue;
        }
        if (__glide_string_eq(method, "textDocument/completion")) {
            handle_completion(req, state);
            continue;
        }
        if (__glide_string_eq(method, "textDocument/references")) {
            handle_references(req, state);
            continue;
        }
        if (__glide_string_eq(method, "textDocument/documentHighlight")) {
            handle_document_highlight(req, state);
            continue;
        }
        if (__glide_string_eq(method, "textDocument/rename")) {
            handle_rename(req, state);
            continue;
        }
        if (__glide_string_eq(method, "textDocument/formatting")) {
            handle_formatting(req, state);
            continue;
        }
        JsonValue*   id = json_get(req, "id");
        if ((id  !=  NULL)) {
            lsp_send_response(id, json_null());
        }
    }
    return 0;
}

void   print_indent (int   n) {
    for (int   i = 0; (i  <  n); i++) {
        printf("%s", "  ");
    }
}

void   print_type (Type*   t) {
    if ((t  ==  NULL)) {
        printf("%s", "<none>");
        return;
    }
    if (((t-> kind )  ==  TY_POINTER)) {
        printf("%s", "*");
        print_type((t-> inner ));
        return;
    }
    if (((t-> kind )  ==  TY_NAMED)) {
        printf("%s", (t-> name ));
        return;
    }
    printf("%s", "<ty?>");
}

const char*   op_name (int   op) {
    if ((op  ==  OP_ADD)) {
        return "+";
    }
    if ((op  ==  OP_SUB)) {
        return "-";
    }
    if ((op  ==  OP_MUL)) {
        return "*";
    }
    if ((op  ==  OP_DIV)) {
        return "/";
    }
    if ((op  ==  OP_MOD)) {
        return "%";
    }
    if ((op  ==  OP_LT)) {
        return "<";
    }
    if ((op  ==  OP_GT)) {
        return ">";
    }
    return "?";
}

void   print_expr (Expr*   e, int   indent) {
    if ((e  ==  NULL)) {
        print_indent(indent);
        printf("%s\n", "<null>");
        return;
    }
    print_indent(indent);
    if (((e-> kind )  ==  EX_INT)) {
        printf("%s %d\n", "Int", (e-> int_val ));
        return;
    }
    if (((e-> kind )  ==  EX_STRING)) {
        printf("%s %s %s\n", "String \"", (e-> str_val ), "\"");
        return;
    }
    if (((e-> kind )  ==  EX_BOOL)) {
        printf("%s %s\n", "Bool", __glide_bool_to_string((e-> bool_val )));
        return;
    }
    if (((e-> kind )  ==  EX_IDENT)) {
        printf("%s %s\n", "Ident", (e-> str_val ));
        return;
    }
    if (((e-> kind )  ==  EX_BINARY)) {
        printf("%s %s\n", "Binary", op_name((e-> op_code )));
        print_expr((e-> lhs ), (indent  +  1));
        print_expr((e-> rhs ), (indent  +  1));
        return;
    }
    if (((e-> kind )  ==  EX_CALL)) {
        printf("%s\n", "Call");
        print_expr((e-> lhs ), (indent  +  1));
        print_indent((indent  +  1));
        printf("%s %d\n", "args:", Vector_len__Expr((e-> args )));
        for (int   i = 0; (i  <  Vector_len__Expr((e-> args ))); i++) {
            Expr   a = Vector_get__Expr((e-> args ), i);
            print_expr((&a), (indent  +  2));
        }
        return;
    }
    printf("%s %d %s\n", "<expr kind ", (e-> kind ), ">");
}

void   print_stmt (Stmt*   s, int   indent) {
    if ((s  ==  NULL)) {
        print_indent(indent);
        printf("%s\n", "<null stmt>");
        return;
    }
    print_indent(indent);
    if (((s-> kind )  ==  ST_FN)) {
        printf("%s %s %s", "Fn ", (s-> name ), " (");
        for (int   i = 0; (i  <  Vector_len__Param((s-> fn_params ))); i++) {
            if ((i  >  0)) {
                printf("%s", ", ");
            }
            Param   prm = Vector_get__Param((s-> fn_params ), i);
            printf("%s %s", (prm. name ), ": ");
            print_type((prm. ty ));
        }
        printf("%s", ") -> ");
        print_type((s-> fn_ret_ty ));
        printf("%s\n", "");
        for (int   i = 0; (i  <  Vector_len__Stmt((s-> fn_body ))); i++) {
            Stmt   b = Vector_get__Stmt((s-> fn_body ), i);
            print_stmt((&b), (indent  +  1));
        }
        return;
    }
    if (((s-> kind )  ==  ST_LET)) {
        printf("%s %s %s", "Let ", (s-> name ), ": ");
        print_type((s-> let_ty ));
        printf("%s\n", "");
        if (((s-> let_value )  !=  NULL)) {
            print_expr((s-> let_value ), (indent  +  1));
        }
        return;
    }
    if (((s-> kind )  ==  ST_RETURN)) {
        printf("%s\n", "Return");
        if (((s-> expr_value )  !=  NULL)) {
            print_expr((s-> expr_value ), (indent  +  1));
        }
        return;
    }
    if (((s-> kind )  ==  ST_EXPR)) {
        printf("%s\n", "Expr");
        print_expr((s-> expr_value ), (indent  +  1));
        return;
    }
    printf("%s %d %s\n", "<stmt kind ", (s-> kind ), ">");
}

void   load_into (Vector__Stmt*   stmts, const char*   path, HashMap__bool*   loaded) {
    if (HashMap_contains__bool(loaded, path)) {
        return;
    }
    HashMap_insert__bool(loaded, path, true);
    const char*   src = read_file(path);
    if (__glide_string_eq(src, "")) {
        return;
    }
    load_into_str(stmts, src, path, loaded);
}

void   load_into_str (Vector__Stmt*   stmts, const char*   src, const char*   origin, HashMap__bool*   loaded) {
    Lexer*   lex = Lexer_new(src);
    Parser*   p = Parser_new(lex);
    Vector__Stmt*   parsed = parse_program(p);
    const char*   dir = parent_dir(origin);
    for (int   i = 0; (i  <  Vector_len__Stmt(parsed)); i++) {
        Stmt   s = Vector_get__Stmt(parsed, i);
        if (((s. kind )  ==  ST_IMPORT)) {
            const char*   unq = strip_quotes((s. import_path ));
            const char*   resolved = resolve_import(dir, unq);
            load_into(stmts, resolved, loaded);
        } else {
            ((s. origin )  =  origin);
            Vector_push__Stmt(stmts, s);
        }
    }
}

const char*   strip_quotes (const char*   s) {
    int   n = __glide_string_len(s);
    if ((n  <  2)) {
        return s;
    }
    if ((__glide_char_to_int(__glide_string_at(s, 0))  !=  34)) {
        return s;
    }
    return __glide_string_substring(s, 1, (n  -  1));
}

const char*   parent_dir (const char*   path) {
    int   n = __glide_string_len(path);
    int   last = (-1);
    for (int   i = 0; (i  <  n); i++) {
        int   c = __glide_char_to_int(__glide_string_at(path, i));
        if (((c  ==  47)  ||  (c  ==  92))) {
            (last  =  i);
        }
    }
    if ((last  <  0)) {
        return "";
    }
    return __glide_string_substring(path, 0, last);
}

const char*   resolve_import (const char*   base, const char*   ipath) {
    if (__glide_string_eq(base, "")) {
        return ipath;
    }
    return __glide_string_concat(__glide_string_concat(base, "/"), ipath);
}

void   print_usage (void) {
    printf("%s\n", "Glide compiler");
    printf("%s\n", "usage:");
    printf("%s\n", "  glide build <file> [-o <out>] [--target=<triple>]");
    printf("%s\n", "  glide run <file>");
    printf("%s\n", "  glide emit <file>          (print generated C to stdout)");
    printf("%s\n", "  glide check <file>         (parse + type-check, no codegen)");
    printf("%s\n", "  glide fmt <file> [--write] (pretty-print to stdout, or rewrite in place)");
    printf("%s\n", "  glide lsp                  (language server on stdio)");
}

bool   parse_program_into (Vector__Stmt*   stmts, const char*   path) {
    const char*   src = read_file(path);
    if (__glide_string_eq(src, "")) {
        printf("%s %s\n", "could not read or empty file:", path);
        return false;
    }
    HashMap__bool*   loaded = HashMap_new__bool();
    const char*   v_path = "stdlib/vector.glide";
    const char*   h_path = "stdlib/hashmap.glide";
    if (__glide_string_eq(read_file(v_path), "")) {
        const char*   exe_dir = __glide_exe_dir();
        if ((!__glide_string_eq(exe_dir, ""))) {
            (v_path  =  __glide_string_concat(exe_dir, "/stdlib/vector.glide"));
            (h_path  =  __glide_string_concat(exe_dir, "/stdlib/hashmap.glide"));
        }
    }
    load_into(stmts, v_path, loaded);
    load_into(stmts, h_path, loaded);
    load_into_str(stmts, src, path, loaded);
    return true;
}

const char*   pick_cc (void) {
    const char*   exe_dir = __glide_exe_dir();
    if ((!__glide_string_eq(exe_dir, ""))) {
        const char*   zig = __glide_string_concat(exe_dir, "/runtime/zig/zig");
        if ((__glide_is_windows()  !=  0)) {
            (zig  =  __glide_string_concat(zig, ".exe"));
        }
        if (__glide_file_exists(zig)) {
            return __glide_string_concat(__glide_string_concat("\"", zig), "\" cc");
        }
    }
    const char*   env = __glide_getenv("CC");
    if ((!__glide_string_eq(env, ""))) {
        return env;
    }
    return "cc";
}

int   run_build (const char*   src_path, const char*   out_path, const char*   target, bool   then_run) {
    Vector__Stmt*   stmts = Vector_new__Stmt();
    if ((!parse_program_into(stmts, src_path))) {
        return 1;
    }
    const char*   tmp_c = __glide_string_concat(out_path, ".__glide.c");
    int   saved = __glide_redirect_to(tmp_c);
    if ((saved  <  0)) {
        printf("%s %s\n", "could not open temp file:", tmp_c);
        return 1;
    }
    emit_program(stmts);
    __glide_restore_stdout(saved);
    const char*   cc = pick_cc();
    const char*   cmd = cc;
    (cmd  =  __glide_string_concat(__glide_string_concat(__glide_string_concat(cmd, " \""), tmp_c), "\""));
    (cmd  =  __glide_string_concat(__glide_string_concat(__glide_string_concat(cmd, " -o \""), out_path), "\""));
    (cmd  =  __glide_string_concat(cmd, " -O2 -w -lpthread -lm"));
    if ((!__glide_string_eq(target, ""))) {
        (cmd  =  __glide_string_concat(__glide_string_concat(cmd, " --target="), target));
    }
    if ((__glide_is_windows()  !=  0)) {
        (cmd  =  __glide_string_concat(__glide_string_concat("\"", cmd), "\""));
    }
    int   rc = __glide_shell(cmd);
    if ((rc  !=  0)) {
        printf("%s %d %s\n", "compilation failed (cc exit", rc, ")");
        printf("%s %s\n", "preserved C source at:", tmp_c);
        return rc;
    }
    if (then_run) {
        const char*   prefix = "./";
        if ((__glide_is_windows()  !=  0)) {
            (prefix  =  ".\\");
        }
        const char*   run_cmd = __glide_string_concat(__glide_string_concat(__glide_string_concat("\"", prefix), out_path), "\"");
        if ((__glide_is_windows()  !=  0)) {
            (run_cmd  =  __glide_string_concat(__glide_string_concat("\"", run_cmd), "\""));
        }
        return __glide_shell(run_cmd);
    }
    return 0;
}

int main(int __glide_main_argc, char** __glide_main_argv) {
    __glide_args_init(__glide_main_argc, __glide_main_argv);
    int   argc = args_count();
    if ((argc  <  2)) {
        print_usage();
        return 1;
    }
    const char*   cmd = args_at(1);
    if (__glide_string_eq(cmd, "lsp")) {
        return lsp_main();
    }
    int   mode = MODE_EMIT;
    int   src_arg_idx = 2;
    if (__glide_string_eq(cmd, "build")) {
        (mode  =  MODE_BUILD);
    } else {
        if (__glide_string_eq(cmd, "run")) {
            (mode  =  MODE_RUN);
        } else {
            if (__glide_string_eq(cmd, "emit")) {
                (mode  =  MODE_EMIT);
            } else {
                if (__glide_string_eq(cmd, "check")) {
                    (mode  =  MODE_CHECK);
                } else {
                    if (__glide_string_eq(cmd, "fmt")) {
                        (mode  =  MODE_FMT);
                    } else {
                        (src_arg_idx  =  1);
                        for (int   i = 0; (i  <  argc); i++) {
                            if (__glide_string_eq(args_at(i), "--emit")) {
                                (mode  =  MODE_EMIT);
                                (src_arg_idx  =  (i  +  1));
                            }
                        }
                    }
                }
            }
        }
    }
    if ((src_arg_idx  >=  argc)) {
        print_usage();
        return 1;
    }
    const char*   src_path = args_at(src_arg_idx);
    const char*   out_path = "";
    const char*   target = "";
    bool   write_in_place = false;
    for (int   i = (src_arg_idx  +  1); (i  <  argc); i++) {
        const char*   a = args_at(i);
        if ((__glide_string_eq(a, "-o")  &&  ((i  +  1)  <  argc))) {
            (out_path  =  args_at((i  +  1)));
        }
        if (__glide_string_eq(a, "--write")) {
            (write_in_place  =  true);
        }
        if ((__glide_string_len(a)  >  9)) {
            const char*   prefix = __glide_string_substring(a, 0, 9);
            if (__glide_string_eq(prefix, "--target=")) {
                (target  =  __glide_string_substring(a, 9, __glide_string_len(a)));
            }
        }
    }
    if ((mode  ==  MODE_BUILD)) {
        if (__glide_string_eq(out_path, "")) {
            if ((__glide_is_windows()  !=  0)) {
                (out_path  =  "a.exe");
            } else {
                (out_path  =  "a.out");
            }
        }
        return run_build(src_path, out_path, target, false);
    }
    if ((mode  ==  MODE_RUN)) {
        if (__glide_string_eq(out_path, "")) {
            if ((__glide_is_windows()  !=  0)) {
                (out_path  =  "__glide_run_tmp.exe");
            } else {
                (out_path  =  "__glide_run_tmp");
            }
        }
        return run_build(src_path, out_path, target, true);
    }
    if ((mode  ==  MODE_FMT)) {
        const char*   src = read_file(src_path);
        if (__glide_string_eq(src, "")) {
            printf("%s %s\n", "could not read or empty file:", src_path);
            return 1;
        }
        Lexer*   lex = Lexer_new(src);
        Parser*   p = Parser_new(lex);
        Vector__Stmt*   parsed = parse_program(p);
        const char*   formatted = fmt_program(parsed);
        if (write_in_place) {
            if ((!write_file(src_path, formatted))) {
                printf("%s %s\n", "could not write:", src_path);
                return 1;
            }
            return 0;
        }
        __glide_write_str(formatted);
        __glide_flush_stdout();
        return 0;
    }
    Vector__Stmt*   stmts = Vector_new__Stmt();
    if ((!parse_program_into(stmts, src_path))) {
        return 1;
    }
    if ((mode  ==  MODE_CHECK)) {
        Typer*   t = Typer_new();
        check_program(t, stmts);
        run_extra_analyses(t, stmts);
        int   user_errors = 0;
        for (int   i = 0; (i  <  Vector_len__DiagEntry((t-> diagnostics ))); i++) {
            DiagEntry   d = Vector_get__DiagEntry((t-> diagnostics ), i);
            const char*   path = (d. origin );
            if (__glide_string_eq(path, "")) {
                (path  =  src_path);
            }
            bool   want_all = false;
            for (int   k = 0; (k  <  argc); k++) {
                if (__glide_string_eq(args_at(k), "--all")) {
                    (want_all  =  true);
                }
            }
            if (((!want_all)  &&  (!__glide_string_eq(path, src_path)))) {
                continue;
            }
            const char*   tag = "error";
            if (((d. severity )  ==  2)) {
                (tag  =  "warning");
            }
            if (((d. severity )  ==  3)) {
                (tag  =  "info");
            }
            if (((d. severity )  ==  4)) {
                (tag  =  "hint");
            }
            printf("%s %s %d %s %d %s %s", path, ":", (d. line ), ":", (d. col ), ": ", tag);
            if ((!__glide_string_eq((d. code ), ""))) {
                printf("%s %s %s", " [", (d. code ), "]");
            }
            printf("%s %s\n", ": ", (d. message ));
            if (((d. severity )  ==  1)) {
                (user_errors  =  (user_errors  +  1));
            }
        }
        if ((user_errors  >  0)) {
            printf("%d %s\n", user_errors, "error(s)");
            Typer_free(t);
            return 1;
        }
        printf("%s\n", "ok");
        Typer_free(t);
        return 0;
    }
    emit_program(stmts);
    return 0;
}

int   HashMap_hash_key__LspDoc (HashMap__LspDoc*   self, const char*   k) {
    int   h = 0;
    int   n = __glide_string_len(k);
    for (int   i = 0; (i  <  n); i++) {
        (h  =  ((h  *  31)  +  __glide_char_to_int(__glide_string_at(k, i))));
    }
    if ((h  <  0)) {
        (h  =  (-h));
    }
    return h;
}

int   HashMap_hash_key__Stmt (HashMap__Stmt*   self, const char*   k) {
    int   h = 0;
    int   n = __glide_string_len(k);
    for (int   i = 0; (i  <  n); i++) {
        (h  =  ((h  *  31)  +  __glide_char_to_int(__glide_string_at(k, i))));
    }
    if ((h  <  0)) {
        (h  =  (-h));
    }
    return h;
}

int   HashMap_hash_key__bool (HashMap__bool*   self, const char*   k) {
    int   h = 0;
    int   n = __glide_string_len(k);
    for (int   i = 0; (i  <  n); i++) {
        (h  =  ((h  *  31)  +  __glide_char_to_int(__glide_string_at(k, i))));
    }
    if ((h  <  0)) {
        (h  =  (-h));
    }
    return h;
}

int   HashMap_hash_key__FnSig (HashMap__FnSig*   self, const char*   k) {
    int   h = 0;
    int   n = __glide_string_len(k);
    for (int   i = 0; (i  <  n); i++) {
        (h  =  ((h  *  31)  +  __glide_char_to_int(__glide_string_at(k, i))));
    }
    if ((h  <  0)) {
        (h  =  (-h));
    }
    return h;
}

int   HashMap_hash_key__Type (HashMap__Type*   self, const char*   k) {
    int   h = 0;
    int   n = __glide_string_len(k);
    for (int   i = 0; (i  <  n); i++) {
        (h  =  ((h  *  31)  +  __glide_char_to_int(__glide_string_at(k, i))));
    }
    if ((h  <  0)) {
        (h  =  (-h));
    }
    return h;
}

void   HashMap_resize__LspDoc (HashMap__LspDoc*   self, int   new_cap) {
    const char**   old_keys = (self-> keys );
    LspDoc*   old_values = (self-> values );
    bool*   old_occupied = (self-> occupied );
    int   old_cap = (self-> cap );
    ((self-> keys )  =  (( const char** )malloc((new_cap  *  sizeof( const char* )))));
    ((self-> values )  =  (( LspDoc* )malloc((new_cap  *  sizeof( LspDoc )))));
    ((self-> occupied )  =  (( bool* )malloc((new_cap  *  sizeof( bool )))));
    ((self-> cap )  =  new_cap);
    ((self-> len )  =  0);
    for (int   i = 0; (i  <  new_cap); i++) {
        ((self-> occupied )[i]  =  false);
    }
    for (int   i = 0; (i  <  old_cap); i++) {
        if (old_occupied[i]) {
            HashMap_insert__LspDoc(self, old_keys[i], old_values[i]);
        }
    }
    if ((old_cap  >  0)) {
        free((( void* )old_keys));
        free((( void* )old_values));
        free((( void* )old_occupied));
    }
}

int   HashMap_slot__LspDoc (HashMap__LspDoc*   self, const char*   k) {
    if (((self-> cap )  ==  0)) {
        return (-1);
    }
    int   mask = ((self-> cap )  -  1);
    int   i = (HashMap_hash_key__LspDoc(self, k)  &  mask);
    while ((self-> occupied )[i]) {
        if (__glide_string_eq((self-> keys )[i], k)) {
            return i;
        }
        (i  =  ((i  +  1)  &  mask));
    }
    return i;
}

void   HashMap_resize__Stmt (HashMap__Stmt*   self, int   new_cap) {
    const char**   old_keys = (self-> keys );
    Stmt*   old_values = (self-> values );
    bool*   old_occupied = (self-> occupied );
    int   old_cap = (self-> cap );
    ((self-> keys )  =  (( const char** )malloc((new_cap  *  sizeof( const char* )))));
    ((self-> values )  =  (( Stmt* )malloc((new_cap  *  sizeof( Stmt )))));
    ((self-> occupied )  =  (( bool* )malloc((new_cap  *  sizeof( bool )))));
    ((self-> cap )  =  new_cap);
    ((self-> len )  =  0);
    for (int   i = 0; (i  <  new_cap); i++) {
        ((self-> occupied )[i]  =  false);
    }
    for (int   i = 0; (i  <  old_cap); i++) {
        if (old_occupied[i]) {
            HashMap_insert__Stmt(self, old_keys[i], old_values[i]);
        }
    }
    if ((old_cap  >  0)) {
        free((( void* )old_keys));
        free((( void* )old_values));
        free((( void* )old_occupied));
    }
}

int   HashMap_slot__Stmt (HashMap__Stmt*   self, const char*   k) {
    if (((self-> cap )  ==  0)) {
        return (-1);
    }
    int   mask = ((self-> cap )  -  1);
    int   i = (HashMap_hash_key__Stmt(self, k)  &  mask);
    while ((self-> occupied )[i]) {
        if (__glide_string_eq((self-> keys )[i], k)) {
            return i;
        }
        (i  =  ((i  +  1)  &  mask));
    }
    return i;
}

int   HashMap_slot__bool (HashMap__bool*   self, const char*   k) {
    if (((self-> cap )  ==  0)) {
        return (-1);
    }
    int   mask = ((self-> cap )  -  1);
    int   i = (HashMap_hash_key__bool(self, k)  &  mask);
    while ((self-> occupied )[i]) {
        if (__glide_string_eq((self-> keys )[i], k)) {
            return i;
        }
        (i  =  ((i  +  1)  &  mask));
    }
    return i;
}

void   HashMap_resize__bool (HashMap__bool*   self, int   new_cap) {
    const char**   old_keys = (self-> keys );
    bool*   old_values = (self-> values );
    bool*   old_occupied = (self-> occupied );
    int   old_cap = (self-> cap );
    ((self-> keys )  =  (( const char** )malloc((new_cap  *  sizeof( const char* )))));
    ((self-> values )  =  (( bool* )malloc((new_cap  *  sizeof( bool )))));
    ((self-> occupied )  =  (( bool* )malloc((new_cap  *  sizeof( bool )))));
    ((self-> cap )  =  new_cap);
    ((self-> len )  =  0);
    for (int   i = 0; (i  <  new_cap); i++) {
        ((self-> occupied )[i]  =  false);
    }
    for (int   i = 0; (i  <  old_cap); i++) {
        if (old_occupied[i]) {
            HashMap_insert__bool(self, old_keys[i], old_values[i]);
        }
    }
    if ((old_cap  >  0)) {
        free((( void* )old_keys));
        free((( void* )old_values));
        free((( void* )old_occupied));
    }
}

int   HashMap_slot__FnSig (HashMap__FnSig*   self, const char*   k) {
    if (((self-> cap )  ==  0)) {
        return (-1);
    }
    int   mask = ((self-> cap )  -  1);
    int   i = (HashMap_hash_key__FnSig(self, k)  &  mask);
    while ((self-> occupied )[i]) {
        if (__glide_string_eq((self-> keys )[i], k)) {
            return i;
        }
        (i  =  ((i  +  1)  &  mask));
    }
    return i;
}

void   HashMap_resize__FnSig (HashMap__FnSig*   self, int   new_cap) {
    const char**   old_keys = (self-> keys );
    FnSig*   old_values = (self-> values );
    bool*   old_occupied = (self-> occupied );
    int   old_cap = (self-> cap );
    ((self-> keys )  =  (( const char** )malloc((new_cap  *  sizeof( const char* )))));
    ((self-> values )  =  (( FnSig* )malloc((new_cap  *  sizeof( FnSig )))));
    ((self-> occupied )  =  (( bool* )malloc((new_cap  *  sizeof( bool )))));
    ((self-> cap )  =  new_cap);
    ((self-> len )  =  0);
    for (int   i = 0; (i  <  new_cap); i++) {
        ((self-> occupied )[i]  =  false);
    }
    for (int   i = 0; (i  <  old_cap); i++) {
        if (old_occupied[i]) {
            HashMap_insert__FnSig(self, old_keys[i], old_values[i]);
        }
    }
    if ((old_cap  >  0)) {
        free((( void* )old_keys));
        free((( void* )old_values));
        free((( void* )old_occupied));
    }
}

int   HashMap_slot__Type (HashMap__Type*   self, const char*   k) {
    if (((self-> cap )  ==  0)) {
        return (-1);
    }
    int   mask = ((self-> cap )  -  1);
    int   i = (HashMap_hash_key__Type(self, k)  &  mask);
    while ((self-> occupied )[i]) {
        if (__glide_string_eq((self-> keys )[i], k)) {
            return i;
        }
        (i  =  ((i  +  1)  &  mask));
    }
    return i;
}

void   HashMap_resize__Type (HashMap__Type*   self, int   new_cap) {
    const char**   old_keys = (self-> keys );
    Type*   old_values = (self-> values );
    bool*   old_occupied = (self-> occupied );
    int   old_cap = (self-> cap );
    ((self-> keys )  =  (( const char** )malloc((new_cap  *  sizeof( const char* )))));
    ((self-> values )  =  (( Type* )malloc((new_cap  *  sizeof( Type )))));
    ((self-> occupied )  =  (( bool* )malloc((new_cap  *  sizeof( bool )))));
    ((self-> cap )  =  new_cap);
    ((self-> len )  =  0);
    for (int   i = 0; (i  <  new_cap); i++) {
        ((self-> occupied )[i]  =  false);
    }
    for (int   i = 0; (i  <  old_cap); i++) {
        if (old_occupied[i]) {
            HashMap_insert__Type(self, old_keys[i], old_values[i]);
        }
    }
    if ((old_cap  >  0)) {
        free((( void* )old_keys));
        free((( void* )old_values));
        free((( void* )old_occupied));
    }
}

UseSite   Vector_get__UseSite (Vector__UseSite*   self, int   i) {
    return (self-> data )[i];
}

int   Vector_len__UseSite (Vector__UseSite*   self) {
    return (self-> len );
}

Vector__UseSite*   Vector_new__UseSite (void) {
    Vector__UseSite*   v = (( Vector__UseSite* )malloc(sizeof( Vector__UseSite )));
    ((v-> data )  =  NULL);
    ((v-> len )  =  0);
    ((v-> cap )  =  0);
    return v;
}

void   Vector_push__UseSite (Vector__UseSite*   self, UseSite   x) {
    if (((self-> len )  ==  (self-> cap ))) {
        int   new_cap = 4;
        if (((self-> cap )  >  0)) {
            (new_cap  =  ((self-> cap )  *  2));
        }
        UseSite*   new_data = (( UseSite* )malloc((new_cap  *  sizeof( UseSite ))));
        for (int   i = 0; (i  <  (self-> len )); i++) {
            (new_data[i]  =  (self-> data )[i]);
        }
        if (((self-> cap )  >  0)) {
            free((( void* )(self-> data )));
        }
        ((self-> data )  =  new_data);
        ((self-> cap )  =  new_cap);
    }
    ((self-> data )[(self-> len )]  =  x);
    ((self-> len )  =  ((self-> len )  +  1));
}

bool   HashMap_remove__LspDoc (HashMap__LspDoc*   self, const char*   k) {
    int   i = HashMap_slot__LspDoc(self, k);
    if ((i  <  0)) {
        return false;
    }
    if ((!(self-> occupied )[i])) {
        return false;
    }
    if ((!__glide_string_eq((self-> keys )[i], k))) {
        return false;
    }
    ((self-> occupied )[i]  =  false);
    ((self-> len )  =  ((self-> len )  -  1));
    int   mask = ((self-> cap )  -  1);
    int   j = ((i  +  1)  &  mask);
    while ((self-> occupied )[j]) {
        const char*   rk = (self-> keys )[j];
        LspDoc   rv = (self-> values )[j];
        ((self-> occupied )[j]  =  false);
        ((self-> len )  =  ((self-> len )  -  1));
        HashMap_insert__LspDoc(self, rk, rv);
        (j  =  ((j  +  1)  &  mask));
    }
    return true;
}

DiagEntry   Vector_get__DiagEntry (Vector__DiagEntry*   self, int   i) {
    return (self-> data )[i];
}

int   Vector_len__DiagEntry (Vector__DiagEntry*   self) {
    return (self-> len );
}

void   HashMap_insert__LspDoc (HashMap__LspDoc*   self, const char*   k, LspDoc   v) {
    if (((self-> cap )  ==  0)) {
        HashMap_resize__LspDoc(self, 8);
    } else {
        if ((((self-> len )  *  4)  >=  ((self-> cap )  *  3))) {
            HashMap_resize__LspDoc(self, ((self-> cap )  *  2));
        }
    }
    int   i = HashMap_slot__LspDoc(self, k);
    if ((!(self-> occupied )[i])) {
        ((self-> occupied )[i]  =  true);
        ((self-> len )  =  ((self-> len )  +  1));
    }
    ((self-> keys )[i]  =  k);
    ((self-> values )[i]  =  v);
}

LspDoc   HashMap_get__LspDoc (HashMap__LspDoc*   self, const char*   k) {
    int   i = HashMap_slot__LspDoc(self, k);
    return (self-> values )[i];
}

bool   HashMap_contains__LspDoc (HashMap__LspDoc*   self, const char*   k) {
    int   i = HashMap_slot__LspDoc(self, k);
    if ((i  <  0)) {
        return false;
    }
    return ((self-> occupied )[i]  &&  __glide_string_eq((self-> keys )[i], k));
}

HashMap__LspDoc*   HashMap_new__LspDoc (void) {
    HashMap__LspDoc*   m = (( HashMap__LspDoc* )malloc(sizeof( HashMap__LspDoc )));
    ((m-> keys )  =  NULL);
    ((m-> values )  =  NULL);
    ((m-> occupied )  =  NULL);
    ((m-> len )  =  0);
    ((m-> cap )  =  0);
    return m;
}

int   Vector_len__JsonValue (Vector__JsonValue*   self) {
    return (self-> len );
}

JsonValue   Vector_get__JsonValue (Vector__JsonValue*   self, int   i) {
    return (self-> data )[i];
}

void   Vector_push__JsonValue (Vector__JsonValue*   self, JsonValue   x) {
    if (((self-> len )  ==  (self-> cap ))) {
        int   new_cap = 4;
        if (((self-> cap )  >  0)) {
            (new_cap  =  ((self-> cap )  *  2));
        }
        JsonValue*   new_data = (( JsonValue* )malloc((new_cap  *  sizeof( JsonValue ))));
        for (int   i = 0; (i  <  (self-> len )); i++) {
            (new_data[i]  =  (self-> data )[i]);
        }
        if (((self-> cap )  >  0)) {
            free((( void* )(self-> data )));
        }
        ((self-> data )  =  new_data);
        ((self-> cap )  =  new_cap);
    }
    ((self-> data )[(self-> len )]  =  x);
    ((self-> len )  =  ((self-> len )  +  1));
}

Vector__JsonValue*   Vector_new__JsonValue (void) {
    Vector__JsonValue*   v = (( Vector__JsonValue* )malloc(sizeof( Vector__JsonValue )));
    ((v-> data )  =  NULL);
    ((v-> len )  =  0);
    ((v-> cap )  =  0);
    return v;
}

FnMonoEntry   Vector_pop__FnMonoEntry (Vector__FnMonoEntry*   self) {
    ((self-> len )  =  ((self-> len )  -  1));
    return (self-> data )[(self-> len )];
}

FnMonoEntry   Vector_get__FnMonoEntry (Vector__FnMonoEntry*   self, int   i) {
    return (self-> data )[i];
}

int   Vector_len__FnMonoEntry (Vector__FnMonoEntry*   self) {
    return (self-> len );
}

void   HashMap_insert__Stmt (HashMap__Stmt*   self, const char*   k, Stmt   v) {
    if (((self-> cap )  ==  0)) {
        HashMap_resize__Stmt(self, 8);
    } else {
        if ((((self-> len )  *  4)  >=  ((self-> cap )  *  3))) {
            HashMap_resize__Stmt(self, ((self-> cap )  *  2));
        }
    }
    int   i = HashMap_slot__Stmt(self, k);
    if ((!(self-> occupied )[i])) {
        ((self-> occupied )[i]  =  true);
        ((self-> len )  =  ((self-> len )  +  1));
    }
    ((self-> keys )[i]  =  k);
    ((self-> values )[i]  =  v);
}

EnumVariant   Vector_get__EnumVariant (Vector__EnumVariant*   self, int   i) {
    return (self-> data )[i];
}

int   Vector_len__EnumVariant (Vector__EnumVariant*   self) {
    return (self-> len );
}

void   Vector_set__Stmt (Vector__Stmt*   self, int   i, Stmt   x) {
    ((self-> data )[i]  =  x);
}

MatchArm   Vector_get__MatchArm (Vector__MatchArm*   self, int   i) {
    return (self-> data )[i];
}

int   Vector_len__MatchArm (Vector__MatchArm*   self) {
    return (self-> len );
}

AnonFn   Vector_get__AnonFn (Vector__AnonFn*   self, int   i) {
    return (self-> data )[i];
}

int   Vector_len__AnonFn (Vector__AnonFn*   self) {
    return (self-> len );
}

void   HashMap_clear__Type (HashMap__Type*   self) {
    for (int   i = 0; (i  <  (self-> cap )); i++) {
        ((self-> occupied )[i]  =  false);
    }
    ((self-> len )  =  0);
}

void   Vector_push__FnMonoEntry (Vector__FnMonoEntry*   self, FnMonoEntry   x) {
    if (((self-> len )  ==  (self-> cap ))) {
        int   new_cap = 4;
        if (((self-> cap )  >  0)) {
            (new_cap  =  ((self-> cap )  *  2));
        }
        FnMonoEntry*   new_data = (( FnMonoEntry* )malloc((new_cap  *  sizeof( FnMonoEntry ))));
        for (int   i = 0; (i  <  (self-> len )); i++) {
            (new_data[i]  =  (self-> data )[i]);
        }
        if (((self-> cap )  >  0)) {
            free((( void* )(self-> data )));
        }
        ((self-> data )  =  new_data);
        ((self-> cap )  =  new_cap);
    }
    ((self-> data )[(self-> len )]  =  x);
    ((self-> len )  =  ((self-> len )  +  1));
}

void   HashMap_free__Stmt (HashMap__Stmt*   self) {
    if (((self-> cap )  >  0)) {
        free((( void* )(self-> keys )));
        free((( void* )(self-> values )));
        free((( void* )(self-> occupied )));
    }
    free((( void* )self));
}

Expr   Vector_pop__Expr (Vector__Expr*   self) {
    ((self-> len )  =  ((self-> len )  -  1));
    return (self-> data )[(self-> len )];
}

Vector__AnonFn*   Vector_new__AnonFn (void) {
    Vector__AnonFn*   v = (( Vector__AnonFn* )malloc(sizeof( Vector__AnonFn )));
    ((v-> data )  =  NULL);
    ((v-> len )  =  0);
    ((v-> cap )  =  0);
    return v;
}

Vector__FnMonoEntry*   Vector_new__FnMonoEntry (void) {
    Vector__FnMonoEntry*   v = (( Vector__FnMonoEntry* )malloc(sizeof( Vector__FnMonoEntry )));
    ((v-> data )  =  NULL);
    ((v-> len )  =  0);
    ((v-> cap )  =  0);
    return v;
}

HashMap__Stmt*   HashMap_new__Stmt (void) {
    HashMap__Stmt*   m = (( HashMap__Stmt* )malloc(sizeof( HashMap__Stmt )));
    ((m-> keys )  =  NULL);
    ((m-> values )  =  NULL);
    ((m-> occupied )  =  NULL);
    ((m-> len )  =  0);
    ((m-> cap )  =  0);
    return m;
}

Stmt   HashMap_get__Stmt (HashMap__Stmt*   self, const char*   k) {
    int   i = HashMap_slot__Stmt(self, k);
    return (self-> values )[i];
}

bool   HashMap_contains__Stmt (HashMap__Stmt*   self, const char*   k) {
    int   i = HashMap_slot__Stmt(self, k);
    if ((i  <  0)) {
        return false;
    }
    return ((self-> occupied )[i]  &&  __glide_string_eq((self-> keys )[i], k));
}

Type   Vector_get__Type (Vector__Type*   self, int   i) {
    return (self-> data )[i];
}

int   Vector_len__Type (Vector__Type*   self) {
    return (self-> len );
}

void   Vector_push__AnonFn (Vector__AnonFn*   self, AnonFn   x) {
    if (((self-> len )  ==  (self-> cap ))) {
        int   new_cap = 4;
        if (((self-> cap )  >  0)) {
            (new_cap  =  ((self-> cap )  *  2));
        }
        AnonFn*   new_data = (( AnonFn* )malloc((new_cap  *  sizeof( AnonFn ))));
        for (int   i = 0; (i  <  (self-> len )); i++) {
            (new_data[i]  =  (self-> data )[i]);
        }
        if (((self-> cap )  >  0)) {
            free((( void* )(self-> data )));
        }
        ((self-> data )  =  new_data);
        ((self-> cap )  =  new_cap);
    }
    ((self-> data )[(self-> len )]  =  x);
    ((self-> len )  =  ((self-> len )  +  1));
}

FnSig   HashMap_get__FnSig (HashMap__FnSig*   self, const char*   k) {
    int   i = HashMap_slot__FnSig(self, k);
    return (self-> values )[i];
}

bool   HashMap_contains__FnSig (HashMap__FnSig*   self, const char*   k) {
    int   i = HashMap_slot__FnSig(self, k);
    if ((i  <  0)) {
        return false;
    }
    return ((self-> occupied )[i]  &&  __glide_string_eq((self-> keys )[i], k));
}

Type   HashMap_get__Type (HashMap__Type*   self, const char*   k) {
    int   i = HashMap_slot__Type(self, k);
    return (self-> values )[i];
}

bool   HashMap_contains__Type (HashMap__Type*   self, const char*   k) {
    int   i = HashMap_slot__Type(self, k);
    if ((i  <  0)) {
        return false;
    }
    return ((self-> occupied )[i]  &&  __glide_string_eq((self-> keys )[i], k));
}

bool   HashMap_contains__bool (HashMap__bool*   self, const char*   k) {
    int   i = HashMap_slot__bool(self, k);
    if ((i  <  0)) {
        return false;
    }
    return ((self-> occupied )[i]  &&  __glide_string_eq((self-> keys )[i], k));
}

Param   Vector_get__Param (Vector__Param*   self, int   i) {
    return (self-> data )[i];
}

int   Vector_len__Param (Vector__Param*   self) {
    return (self-> len );
}

bool   Vector_get__bool (Vector__bool*   self, int   i) {
    return (self-> data )[i];
}

const char*   Vector_get__string (Vector__string*   self, int   i) {
    return (self-> data )[i];
}

int   Vector_len__string (Vector__string*   self) {
    return (self-> len );
}

void   Vector_push__bool (Vector__bool*   self, bool   x) {
    if (((self-> len )  ==  (self-> cap ))) {
        int   new_cap = 4;
        if (((self-> cap )  >  0)) {
            (new_cap  =  ((self-> cap )  *  2));
        }
        bool*   new_data = (( bool* )malloc((new_cap  *  sizeof( bool ))));
        for (int   i = 0; (i  <  (self-> len )); i++) {
            (new_data[i]  =  (self-> data )[i]);
        }
        if (((self-> cap )  >  0)) {
            free((( void* )(self-> data )));
        }
        ((self-> data )  =  new_data);
        ((self-> cap )  =  new_cap);
    }
    ((self-> data )[(self-> len )]  =  x);
    ((self-> len )  =  ((self-> len )  +  1));
}

Expr   Vector_get__Expr (Vector__Expr*   self, int   i) {
    return (self-> data )[i];
}

int   Vector_len__Expr (Vector__Expr*   self) {
    return (self-> len );
}

Vector__bool*   Vector_new__bool (void) {
    Vector__bool*   v = (( Vector__bool* )malloc(sizeof( Vector__bool )));
    ((v-> data )  =  NULL);
    ((v-> len )  =  0);
    ((v-> cap )  =  0);
    return v;
}

BorrowEvent   Vector_pop__BorrowEvent (Vector__BorrowEvent*   self) {
    ((self-> len )  =  ((self-> len )  -  1));
    return (self-> data )[(self-> len )];
}

void   Vector_push__BorrowEvent (Vector__BorrowEvent*   self, BorrowEvent   x) {
    if (((self-> len )  ==  (self-> cap ))) {
        int   new_cap = 4;
        if (((self-> cap )  >  0)) {
            (new_cap  =  ((self-> cap )  *  2));
        }
        BorrowEvent*   new_data = (( BorrowEvent* )malloc((new_cap  *  sizeof( BorrowEvent ))));
        for (int   i = 0; (i  <  (self-> len )); i++) {
            (new_data[i]  =  (self-> data )[i]);
        }
        if (((self-> cap )  >  0)) {
            free((( void* )(self-> data )));
        }
        ((self-> data )  =  new_data);
        ((self-> cap )  =  new_cap);
    }
    ((self-> data )[(self-> len )]  =  x);
    ((self-> len )  =  ((self-> len )  +  1));
}

BorrowEvent   Vector_get__BorrowEvent (Vector__BorrowEvent*   self, int   i) {
    return (self-> data )[i];
}

int   Vector_len__BorrowEvent (Vector__BorrowEvent*   self) {
    return (self-> len );
}

Field   Vector_get__Field (Vector__Field*   self, int   i) {
    return (self-> data )[i];
}

int   Vector_len__Field (Vector__Field*   self) {
    return (self-> len );
}

void   HashMap_insert__bool (HashMap__bool*   self, const char*   k, bool   v) {
    if (((self-> cap )  ==  0)) {
        HashMap_resize__bool(self, 8);
    } else {
        if ((((self-> len )  *  4)  >=  ((self-> cap )  *  3))) {
            HashMap_resize__bool(self, ((self-> cap )  *  2));
        }
    }
    int   i = HashMap_slot__bool(self, k);
    if ((!(self-> occupied )[i])) {
        ((self-> occupied )[i]  =  true);
        ((self-> len )  =  ((self-> len )  +  1));
    }
    ((self-> keys )[i]  =  k);
    ((self-> values )[i]  =  v);
}

void   HashMap_insert__FnSig (HashMap__FnSig*   self, const char*   k, FnSig   v) {
    if (((self-> cap )  ==  0)) {
        HashMap_resize__FnSig(self, 8);
    } else {
        if ((((self-> len )  *  4)  >=  ((self-> cap )  *  3))) {
            HashMap_resize__FnSig(self, ((self-> cap )  *  2));
        }
    }
    int   i = HashMap_slot__FnSig(self, k);
    if ((!(self-> occupied )[i])) {
        ((self-> occupied )[i]  =  true);
        ((self-> len )  =  ((self-> len )  +  1));
    }
    ((self-> keys )[i]  =  k);
    ((self-> values )[i]  =  v);
}

void   HashMap_insert__Type (HashMap__Type*   self, const char*   k, Type   v) {
    if (((self-> cap )  ==  0)) {
        HashMap_resize__Type(self, 8);
    } else {
        if ((((self-> len )  *  4)  >=  ((self-> cap )  *  3))) {
            HashMap_resize__Type(self, ((self-> cap )  *  2));
        }
    }
    int   i = HashMap_slot__Type(self, k);
    if ((!(self-> occupied )[i])) {
        ((self-> occupied )[i]  =  true);
        ((self-> len )  =  ((self-> len )  +  1));
    }
    ((self-> keys )[i]  =  k);
    ((self-> values )[i]  =  v);
}

void   Vector_push__DiagEntry (Vector__DiagEntry*   self, DiagEntry   x) {
    if (((self-> len )  ==  (self-> cap ))) {
        int   new_cap = 4;
        if (((self-> cap )  >  0)) {
            (new_cap  =  ((self-> cap )  *  2));
        }
        DiagEntry*   new_data = (( DiagEntry* )malloc((new_cap  *  sizeof( DiagEntry ))));
        for (int   i = 0; (i  <  (self-> len )); i++) {
            (new_data[i]  =  (self-> data )[i]);
        }
        if (((self-> cap )  >  0)) {
            free((( void* )(self-> data )));
        }
        ((self-> data )  =  new_data);
        ((self-> cap )  =  new_cap);
    }
    ((self-> data )[(self-> len )]  =  x);
    ((self-> len )  =  ((self-> len )  +  1));
}

void   Vector_free__DiagEntry (Vector__DiagEntry*   self) {
    if (((self-> cap )  >  0)) {
        free((( void* )(self-> data )));
    }
    free((( void* )self));
}

void   Vector_free__BorrowEvent (Vector__BorrowEvent*   self) {
    if (((self-> cap )  >  0)) {
        free((( void* )(self-> data )));
    }
    free((( void* )self));
}

void   HashMap_free__Type (HashMap__Type*   self) {
    if (((self-> cap )  >  0)) {
        free((( void* )(self-> keys )));
        free((( void* )(self-> values )));
        free((( void* )(self-> occupied )));
    }
    free((( void* )self));
}

void   HashMap_free__bool (HashMap__bool*   self) {
    if (((self-> cap )  >  0)) {
        free((( void* )(self-> keys )));
        free((( void* )(self-> values )));
        free((( void* )(self-> occupied )));
    }
    free((( void* )self));
}

void   HashMap_free__FnSig (HashMap__FnSig*   self) {
    if (((self-> cap )  >  0)) {
        free((( void* )(self-> keys )));
        free((( void* )(self-> values )));
        free((( void* )(self-> occupied )));
    }
    free((( void* )self));
}

Vector__DiagEntry*   Vector_new__DiagEntry (void) {
    Vector__DiagEntry*   v = (( Vector__DiagEntry* )malloc(sizeof( Vector__DiagEntry )));
    ((v-> data )  =  NULL);
    ((v-> len )  =  0);
    ((v-> cap )  =  0);
    return v;
}

Vector__BorrowEvent*   Vector_new__BorrowEvent (void) {
    Vector__BorrowEvent*   v = (( Vector__BorrowEvent* )malloc(sizeof( Vector__BorrowEvent )));
    ((v-> data )  =  NULL);
    ((v-> len )  =  0);
    ((v-> cap )  =  0);
    return v;
}

HashMap__Type*   HashMap_new__Type (void) {
    HashMap__Type*   m = (( HashMap__Type* )malloc(sizeof( HashMap__Type )));
    ((m-> keys )  =  NULL);
    ((m-> values )  =  NULL);
    ((m-> occupied )  =  NULL);
    ((m-> len )  =  0);
    ((m-> cap )  =  0);
    return m;
}

HashMap__bool*   HashMap_new__bool (void) {
    HashMap__bool*   m = (( HashMap__bool* )malloc(sizeof( HashMap__bool )));
    ((m-> keys )  =  NULL);
    ((m-> values )  =  NULL);
    ((m-> occupied )  =  NULL);
    ((m-> len )  =  0);
    ((m-> cap )  =  0);
    return m;
}

HashMap__FnSig*   HashMap_new__FnSig (void) {
    HashMap__FnSig*   m = (( HashMap__FnSig* )malloc(sizeof( HashMap__FnSig )));
    ((m-> keys )  =  NULL);
    ((m-> values )  =  NULL);
    ((m-> occupied )  =  NULL);
    ((m-> len )  =  0);
    ((m-> cap )  =  0);
    return m;
}

void   Vector_push__Expr (Vector__Expr*   self, Expr   x) {
    if (((self-> len )  ==  (self-> cap ))) {
        int   new_cap = 4;
        if (((self-> cap )  >  0)) {
            (new_cap  =  ((self-> cap )  *  2));
        }
        Expr*   new_data = (( Expr* )malloc((new_cap  *  sizeof( Expr ))));
        for (int   i = 0; (i  <  (self-> len )); i++) {
            (new_data[i]  =  (self-> data )[i]);
        }
        if (((self-> cap )  >  0)) {
            free((( void* )(self-> data )));
        }
        ((self-> data )  =  new_data);
        ((self-> cap )  =  new_cap);
    }
    ((self-> data )[(self-> len )]  =  x);
    ((self-> len )  =  ((self-> len )  +  1));
}

Vector__Expr*   Vector_new__Expr (void) {
    Vector__Expr*   v = (( Vector__Expr* )malloc(sizeof( Vector__Expr )));
    ((v-> data )  =  NULL);
    ((v-> len )  =  0);
    ((v-> cap )  =  0);
    return v;
}

void   Vector_push__MatchArm (Vector__MatchArm*   self, MatchArm   x) {
    if (((self-> len )  ==  (self-> cap ))) {
        int   new_cap = 4;
        if (((self-> cap )  >  0)) {
            (new_cap  =  ((self-> cap )  *  2));
        }
        MatchArm*   new_data = (( MatchArm* )malloc((new_cap  *  sizeof( MatchArm ))));
        for (int   i = 0; (i  <  (self-> len )); i++) {
            (new_data[i]  =  (self-> data )[i]);
        }
        if (((self-> cap )  >  0)) {
            free((( void* )(self-> data )));
        }
        ((self-> data )  =  new_data);
        ((self-> cap )  =  new_cap);
    }
    ((self-> data )[(self-> len )]  =  x);
    ((self-> len )  =  ((self-> len )  +  1));
}

Stmt   Vector_get__Stmt (Vector__Stmt*   self, int   i) {
    return (self-> data )[i];
}

int   Vector_len__Stmt (Vector__Stmt*   self) {
    return (self-> len );
}

Vector__MatchArm*   Vector_new__MatchArm (void) {
    Vector__MatchArm*   v = (( Vector__MatchArm* )malloc(sizeof( Vector__MatchArm )));
    ((v-> data )  =  NULL);
    ((v-> len )  =  0);
    ((v-> cap )  =  0);
    return v;
}

void   Vector_push__EnumVariant (Vector__EnumVariant*   self, EnumVariant   x) {
    if (((self-> len )  ==  (self-> cap ))) {
        int   new_cap = 4;
        if (((self-> cap )  >  0)) {
            (new_cap  =  ((self-> cap )  *  2));
        }
        EnumVariant*   new_data = (( EnumVariant* )malloc((new_cap  *  sizeof( EnumVariant ))));
        for (int   i = 0; (i  <  (self-> len )); i++) {
            (new_data[i]  =  (self-> data )[i]);
        }
        if (((self-> cap )  >  0)) {
            free((( void* )(self-> data )));
        }
        ((self-> data )  =  new_data);
        ((self-> cap )  =  new_cap);
    }
    ((self-> data )[(self-> len )]  =  x);
    ((self-> len )  =  ((self-> len )  +  1));
}

void   Vector_push__Type (Vector__Type*   self, Type   x) {
    if (((self-> len )  ==  (self-> cap ))) {
        int   new_cap = 4;
        if (((self-> cap )  >  0)) {
            (new_cap  =  ((self-> cap )  *  2));
        }
        Type*   new_data = (( Type* )malloc((new_cap  *  sizeof( Type ))));
        for (int   i = 0; (i  <  (self-> len )); i++) {
            (new_data[i]  =  (self-> data )[i]);
        }
        if (((self-> cap )  >  0)) {
            free((( void* )(self-> data )));
        }
        ((self-> data )  =  new_data);
        ((self-> cap )  =  new_cap);
    }
    ((self-> data )[(self-> len )]  =  x);
    ((self-> len )  =  ((self-> len )  +  1));
}

Vector__Type*   Vector_new__Type (void) {
    Vector__Type*   v = (( Vector__Type* )malloc(sizeof( Vector__Type )));
    ((v-> data )  =  NULL);
    ((v-> len )  =  0);
    ((v-> cap )  =  0);
    return v;
}

Vector__EnumVariant*   Vector_new__EnumVariant (void) {
    Vector__EnumVariant*   v = (( Vector__EnumVariant* )malloc(sizeof( Vector__EnumVariant )));
    ((v-> data )  =  NULL);
    ((v-> len )  =  0);
    ((v-> cap )  =  0);
    return v;
}

void   Vector_push__Field (Vector__Field*   self, Field   x) {
    if (((self-> len )  ==  (self-> cap ))) {
        int   new_cap = 4;
        if (((self-> cap )  >  0)) {
            (new_cap  =  ((self-> cap )  *  2));
        }
        Field*   new_data = (( Field* )malloc((new_cap  *  sizeof( Field ))));
        for (int   i = 0; (i  <  (self-> len )); i++) {
            (new_data[i]  =  (self-> data )[i]);
        }
        if (((self-> cap )  >  0)) {
            free((( void* )(self-> data )));
        }
        ((self-> data )  =  new_data);
        ((self-> cap )  =  new_cap);
    }
    ((self-> data )[(self-> len )]  =  x);
    ((self-> len )  =  ((self-> len )  +  1));
}

Vector__Field*   Vector_new__Field (void) {
    Vector__Field*   v = (( Vector__Field* )malloc(sizeof( Vector__Field )));
    ((v-> data )  =  NULL);
    ((v-> len )  =  0);
    ((v-> cap )  =  0);
    return v;
}

void   Vector_push__string (Vector__string*   self, const char*   x) {
    if (((self-> len )  ==  (self-> cap ))) {
        int   new_cap = 4;
        if (((self-> cap )  >  0)) {
            (new_cap  =  ((self-> cap )  *  2));
        }
        const char**   new_data = (( const char** )malloc((new_cap  *  sizeof( const char* ))));
        for (int   i = 0; (i  <  (self-> len )); i++) {
            (new_data[i]  =  (self-> data )[i]);
        }
        if (((self-> cap )  >  0)) {
            free((( void* )(self-> data )));
        }
        ((self-> data )  =  new_data);
        ((self-> cap )  =  new_cap);
    }
    ((self-> data )[(self-> len )]  =  x);
    ((self-> len )  =  ((self-> len )  +  1));
}

Vector__string*   Vector_new__string (void) {
    Vector__string*   v = (( Vector__string* )malloc(sizeof( Vector__string )));
    ((v-> data )  =  NULL);
    ((v-> len )  =  0);
    ((v-> cap )  =  0);
    return v;
}

void   Vector_push__Param (Vector__Param*   self, Param   x) {
    if (((self-> len )  ==  (self-> cap ))) {
        int   new_cap = 4;
        if (((self-> cap )  >  0)) {
            (new_cap  =  ((self-> cap )  *  2));
        }
        Param*   new_data = (( Param* )malloc((new_cap  *  sizeof( Param ))));
        for (int   i = 0; (i  <  (self-> len )); i++) {
            (new_data[i]  =  (self-> data )[i]);
        }
        if (((self-> cap )  >  0)) {
            free((( void* )(self-> data )));
        }
        ((self-> data )  =  new_data);
        ((self-> cap )  =  new_cap);
    }
    ((self-> data )[(self-> len )]  =  x);
    ((self-> len )  =  ((self-> len )  +  1));
}

Vector__Param*   Vector_new__Param (void) {
    Vector__Param*   v = (( Vector__Param* )malloc(sizeof( Vector__Param )));
    ((v-> data )  =  NULL);
    ((v-> len )  =  0);
    ((v-> cap )  =  0);
    return v;
}

void   Vector_push__Stmt (Vector__Stmt*   self, Stmt   x) {
    if (((self-> len )  ==  (self-> cap ))) {
        int   new_cap = 4;
        if (((self-> cap )  >  0)) {
            (new_cap  =  ((self-> cap )  *  2));
        }
        Stmt*   new_data = (( Stmt* )malloc((new_cap  *  sizeof( Stmt ))));
        for (int   i = 0; (i  <  (self-> len )); i++) {
            (new_data[i]  =  (self-> data )[i]);
        }
        if (((self-> cap )  >  0)) {
            free((( void* )(self-> data )));
        }
        ((self-> data )  =  new_data);
        ((self-> cap )  =  new_cap);
    }
    ((self-> data )[(self-> len )]  =  x);
    ((self-> len )  =  ((self-> len )  +  1));
}

Vector__Stmt*   Vector_new__Stmt (void) {
    Vector__Stmt*   v = (( Vector__Stmt* )malloc(sizeof( Vector__Stmt )));
    ((v-> data )  =  NULL);
    ((v-> len )  =  0);
    ((v-> cap )  =  0);
    return v;
}

