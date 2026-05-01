const PREC = {
  assign: 1,
  or: 2,
  and: 3,
  bit_or: 4,
  bit_xor: 5,
  bit_and: 6,
  equal: 7,
  compare: 8,
  shift: 9,
  add: 10,
  mul: 11,
  cast: 12,
  unary: 13,
  postfix: 14,
};

module.exports = grammar({
  name: 'glide',

  word: $ => $.identifier,

  extras: $ => [
    /\s+/,
    $.line_comment,
    $.block_comment,
  ],

  rules: {
    source_file: $ => repeat($._top_item),

    _top_item: $ => choice(
      $.import_stmt,
      $.fn_decl,
      $.struct_decl,
      $.interface_decl,
      $.impl_decl,
      $.let_stmt,
      $.const_stmt,
    ),

    visibility: _ => 'pub',

    import_stmt: $ => seq(
      'import',
      field('path', $.string_literal),
      ';',
    ),

    // ---- declarations ----

    fn_decl: $ => seq(
      optional($.visibility),
      'fn',
      field('name', $.identifier),
      field('params', $.param_list),
      optional(seq('->', field('return_type', $._type))),
      field('body', $.block),
    ),

    param_list: $ => seq(
      '(',
      optional(seq(
        $.param,
        repeat(seq(',', $.param)),
        optional(','),
      )),
      ')',
    ),

    param: $ => seq(
      field('name', $.identifier),
      ':',
      field('type', $._type),
    ),

    struct_decl: $ => seq(
      optional($.visibility),
      'struct',
      field('name', $.identifier),
      '{',
      optional(seq(
        $.struct_field,
        repeat(seq(',', $.struct_field)),
        optional(','),
      )),
      '}',
    ),

    struct_field: $ => seq(
      field('name', $.identifier),
      ':',
      field('type', $._type),
    ),

    interface_decl: $ => seq(
      'interface',
      field('name', $.identifier),
      '{',
      repeat($.interface_method_sig),
      '}',
    ),

    interface_method_sig: $ => seq(
      'fn',
      field('name', $.identifier),
      field('params', $.param_list),
      optional(seq('->', field('return_type', $._type))),
      ';',
    ),

    impl_decl: $ => seq(
      'impl',
      field('interface', $.identifier),
      optional(seq('for', field('target', $._type))),
      '{',
      repeat($.fn_decl),
      '}',
    ),

    // ---- statements ----

    _statement: $ => choice(
      $.let_stmt,
      $.const_stmt,
      $.fn_decl,
      $.struct_decl,
      $.if_stmt,
      $.while_stmt,
      $.for_stmt,
      $.return_stmt,
      $.spawn_stmt,
      $.break_stmt,
      $.continue_stmt,
      $.block,
      $.expression_statement,
    ),

    let_stmt: $ => seq(
      'let',
      field('name', $.identifier),
      optional(seq(':', field('type', $._type))),
      optional(seq('=', field('value', $._expression))),
      ';',
    ),

    const_stmt: $ => seq(
      optional($.visibility),
      'const',
      field('name', $.identifier),
      optional(seq(':', field('type', $._type))),
      '=',
      field('value', $._expression),
      ';',
    ),

    if_stmt: $ => seq(
      'if',
      field('condition', $._expression),
      field('consequence', $.block),
      optional(field('alternative', seq('else', choice($.if_stmt, $.block)))),
    ),

    while_stmt: $ => seq(
      'while',
      field('condition', $._expression),
      field('body', $.block),
    ),

    for_stmt: $ => seq(
      'for',
      optional($._for_init),
      ';',
      optional(field('condition', $._expression)),
      ';',
      optional(field('step', $._expression)),
      field('body', $.block),
    ),

    _for_init: $ => choice(
      seq(
        'let',
        field('name', $.identifier),
        optional(seq(':', field('type', $._type))),
        optional(seq('=', field('value', $._expression))),
      ),
      $._expression,
    ),

    return_stmt: $ => seq('return', optional($._expression), ';'),
    spawn_stmt:  $ => seq('spawn', $._expression, ';'),
    break_stmt:  $ => seq('break', ';'),
    continue_stmt: $ => seq('continue', ';'),

    block: $ => seq('{', repeat($._statement), '}'),

    expression_statement: $ => seq($._expression, ';'),

    // ---- types ----

    _type: $ => choice(
      $.named_type,
      $.pointer_type,
      $.chan_type,
    ),

    named_type:   $ => $.identifier,
    pointer_type: $ => seq('*', $._type),
    chan_type:    $ => seq('chan', '<', $._type, '>'),

    // ---- expressions ----

    _expression: $ => choice(
      $.assignment,
      $._binary_or_unary,
    ),

    assignment: $ => prec.right(PREC.assign, seq(
      field('lhs', $._binary_or_unary),
      field('op', choice('=', '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '<<=', '>>=')),
      field('rhs', $._expression),
    )),

    _binary_or_unary: $ => choice(
      $.binary_expr,
      $.unary_expr,
      $.cast_expr,
      $._postfix_chain,
    ),

    binary_expr: $ => choice(
      ...[
        ['||', PREC.or],
        ['&&', PREC.and],
        ['|',  PREC.bit_or],
        ['^',  PREC.bit_xor],
        ['&',  PREC.bit_and],
        ['==', PREC.equal],
        ['!=', PREC.equal],
        ['<',  PREC.compare],
        ['<=', PREC.compare],
        ['>',  PREC.compare],
        ['>=', PREC.compare],
        ['<<', PREC.shift],
        ['>>', PREC.shift],
        ['+',  PREC.add],
        ['-',  PREC.add],
        ['*',  PREC.mul],
        ['/',  PREC.mul],
        ['%',  PREC.mul],
      ].map(([op, p]) => prec.left(p, seq(
        field('left', $._binary_or_unary),
        field('op', op),
        field('right', $._binary_or_unary),
      ))),
    ),

    unary_expr: $ => prec(PREC.unary, seq(
      field('op', choice('-', '!', '~', '*', '&')),
      field('operand', $._binary_or_unary),
    )),

    cast_expr: $ => prec.left(PREC.cast, seq(
      $._binary_or_unary,
      'as',
      $._type,
    )),

    _postfix_chain: $ => choice(
      $._primary,
      $.call_expr,
      $.index_expr,
      $.member_expr,
      $.arrow_member_expr,
      $.postfix_expr,
    ),

    call_expr: $ => prec(PREC.postfix, seq(
      field('callee', $._postfix_chain),
      field('args', $.argument_list),
    )),

    argument_list: $ => seq(
      '(',
      optional(seq(
        $._expression,
        repeat(seq(',', $._expression)),
        optional(','),
      )),
      ')',
    ),

    index_expr: $ => prec(PREC.postfix, seq(
      field('object', $._postfix_chain),
      '[',
      field('index', $._expression),
      ']',
    )),

    member_expr: $ => prec(PREC.postfix, seq(
      field('object', $._postfix_chain),
      '.',
      field('field', $.identifier),
    )),

    arrow_member_expr: $ => prec(PREC.postfix, seq(
      field('object', $._postfix_chain),
      '->',
      field('field', $.identifier),
    )),

    postfix_expr: $ => prec(PREC.postfix, seq(
      $._postfix_chain,
      field('op', choice('++', '--')),
    )),

    _primary: $ => choice(
      $.parenthesized,
      $.array_literal,
      $.macro_call,
      $.path_expr,
      $.struct_literal,
      $.new_expr,
      $.sizeof_expr,
      $.identifier_expr,
      $.number_literal,
      $.float_literal,
      $.string_literal,
      $.char_literal,
      $.bool_literal,
      $.null_literal,
    ),

    path_expr: $ => prec(20, seq(
      field('type', $.identifier),
      '::',
      field('member', $.identifier),
    )),

    macro_call: $ => prec(20, seq(
      field('name', $.identifier),
      '!',
      '(',
      optional(seq(
        $._expression,
        repeat(seq(',', $._expression)),
        optional(','),
      )),
      ')',
    )),

    array_literal: $ => seq(
      '[',
      optional(seq(
        $._expression,
        repeat(seq(',', $._expression)),
        optional(','),
      )),
      ']',
    ),

    parenthesized: $ => seq('(', $._expression, ')'),

    struct_literal: $ => prec(2, seq(
      field('type', $.identifier),
      '{',
      optional(seq(
        $.struct_lit_field,
        repeat(seq(',', $.struct_lit_field)),
        optional(','),
      )),
      '}',
    )),

    struct_lit_field: $ => seq(
      field('name', $.identifier),
      ':',
      field('value', $._expression),
    ),

    new_expr: $ => seq(
      'new',
      field('type', $.identifier),
      '{',
      optional(seq(
        $.struct_lit_field,
        repeat(seq(',', $.struct_lit_field)),
        optional(','),
      )),
      '}',
    ),

    sizeof_expr: $ => seq('sizeof', '(', $._type, ')'),

    identifier_expr: $ => $.identifier,

    // ---- tokens ----

    identifier: _ => /[A-Za-z_][A-Za-z0-9_]*/,

    number_literal: _ => token(choice(
      /0[xX][0-9A-Fa-f_]+/,
      /0[bB][01_]+/,
      /0[oO][0-7_]+/,
      /[0-9][0-9_]*/,
    )),

    float_literal: _ => token(choice(
      /[0-9][0-9_]*\.[0-9][0-9_]*([eE][+-]?[0-9_]+)?/,
      /[0-9][0-9_]*[eE][+-]?[0-9_]+/,
    )),

    string_literal: _ => token(seq(
      '"',
      repeat(choice(/[^"\\\n]/, /\\./)),
      '"',
    )),

    char_literal: _ => token(seq(
      "'",
      choice(/[^'\\]/, /\\./),
      "'",
    )),

    bool_literal: _ => choice('true', 'false'),
    null_literal: _ => 'null',

    line_comment:  _ => token(seq('//', /[^\n]*/)),
    block_comment: _ => token(seq('/*', /[^*]*\*+([^/*][^*]*\*+)*/, '/')),
  },
});
