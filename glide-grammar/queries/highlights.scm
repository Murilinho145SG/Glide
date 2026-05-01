; Keywords
[
  "fn"
  "let"
  "const"
  "struct"
  "interface"
  "impl"
  "import"
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
] @keyword

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
"*" @operator

; Function declaration & calls
(fn_decl   name: (identifier) @function)
(call_expr callee: (identifier_expr (identifier) @function.call))
(macro_call name: (identifier) @function.macro)

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
(arrow_member_expr field: (identifier) @property)

; Method calls (override the @property above for call_expr targets)
(call_expr
  callee: (member_expr field: (identifier) @function.method))
(call_expr
  callee: (arrow_member_expr field: (identifier) @function.method))

; Params and locals
(param name: (identifier) @variable.parameter)
(let_stmt name: (identifier) @variable)
(const_stmt name: (identifier) @constant)

; Operators / punctuation
[
  "+" "-" "/" "%"
  "==" "!=" "<" "<=" ">" ">="
  "&&" "||" "!"
  "&" "|" "^" "~" "<<" ">>"
  "=" "+=" "-=" "*=" "/=" "%=" "&=" "|=" "^=" "<<=" ">>="
  "++" "--"
  "->"
] @operator

[ "{" "}" "(" ")" "[" "]" ] @punctuation.bracket
[ ";" "," ":" "." ] @punctuation.delimiter
