(fn_decl
  "fn" @context
  name: (identifier) @name
  params: (param_list) @context) @item

(struct_decl
  "struct" @context
  name: (identifier) @name) @item

(const_stmt
  "const" @context
  name: (identifier) @name) @item
