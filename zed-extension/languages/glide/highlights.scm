; Keywords
[
  "fn"
  "let"
  "mut"
  "const"
  "struct"
  "interface"
  "impl"
  "import"
  "pub"
  "enum"
  "match"
  "defer"
  "if"
  "else"
  "while"
  "for"
  "break"
  "continue"
  "return"
  "spawn"
  "chan"
  "as"
  "sizeof"
  "new"
  "move"
  "type"
  "extern"
  "c_include"
  "c_link"
  "macro"
] @keyword

; Built-in / known primitive types
((identifier) @type.builtin
 (#match? @type.builtin "^(int|uint|long|ulong|i8|i16|i32|i64|u8|u16|u32|u64|usize|isize|f32|f64|float|bool|char|string|void)$"))

; Constants
(bool_literal) @boolean
(null_literal) @constant.builtin

; Literals
(number_literal) @number
(float_literal)  @number.float
(string_literal) @string
(char_literal)   @string

; Comments
(line_comment)  @comment
(block_comment) @comment

; Types
(named_type (identifier) @type)
(chan_type)
"chan" @type.builtin

; Operators / punctuation (generic — overridden by more specific captures below)
[
  "+" "-" "/" "%" "*"
  "==" "!=" "<" "<=" ">" ">="
  "&&" "||"
  "&" "|" "^" "~" "<<" ">>"
  "=" "+=" "-=" "*=" "/=" "%=" "&=" "|=" "^=" "<<=" ">>="
  "++" "--"
  "->"
] @operator

[ "{" "}" "(" ")" "[" "]" ] @punctuation.bracket
[ ";" "," ":" "." "::" ] @punctuation.delimiter

; `!` as unary not (when it appears as a unary op)
(unary_expr "!" @operator)

; Function declaration & calls
(fn_decl   name: (identifier) @function)
(extern_fn name: (identifier) @function)
(extern_type name: (identifier) @type)
(call_expr callee: (identifier_expr (identifier) @function.call))

; Struct / interface / impl
(struct_decl name: (identifier) @type)
(interface_decl name: (identifier) @type)
(interface_method_sig name: (identifier) @function)
(impl_decl interface: (identifier) @type)
(struct_field name: (identifier) @property)
(struct_lit_field name: (identifier) @property)
(struct_literal type: (identifier) @type)
(new_expr type: (identifier) @type)

; Members
(member_expr field: (identifier) @property)

; Method calls (override the @property above for call_expr targets)
(call_expr
  callee: (member_expr field: (identifier) @function.method))

; Params and locals
(param name: (identifier) @variable.parameter)
(let_stmt name: (identifier) @variable)
(const_stmt name: (identifier) @constant)

(path_expr type: (identifier) @type)
(path_expr member: (identifier) @function)

; Macro calls (placed last so `!` and macro name override the generic
; operator/identifier captures above)
(macro_call name: (identifier) @function.macro
            "!" @function.macro)

(method_macro_call name: (identifier) @function.macro
                   "!" @function.macro)

(path_macro_call type: (identifier) @type
                 name: (identifier) @function.macro
                 "!" @function.macro)

; Macro definition: `macro name!(matchers) { body }`
(macro_def name: (identifier) @function.macro
           "!" @function.macro)

; Matcher binders: `$x:expr` and the `$($x:expr),*` form
(macro_matcher_var
  "$" @punctuation.special
  name: (identifier) @variable.parameter
  kind: (identifier) @type.builtin)

(macro_matcher_rep
  "$" @punctuation.special
  name: (identifier) @variable.parameter
  kind: (identifier) @type.builtin
  "*" @punctuation.special)

; Body placeholder `$x` and repetition `$( ... );*`
(macro_var_expr
  "$" @punctuation.special
  name: (identifier) @variable.parameter)

(macro_rep_stmt
  "$" @punctuation.special
  "*" @punctuation.special)
