use crate::ast::*;
use crate::lexer::{Keyword, Lexer, Operator, Token, TokenKind};
use crate::types::Type;

#[derive(Debug, Clone)]
pub struct ParseError {
    pub message: String,
    pub line: usize,
    pub column: usize,
}

impl std::fmt::Display for ParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "parse error at {}:{}: {}", self.line, self.column, self.message)
    }
}

pub struct Parser {
    lexer: Lexer,
    current: Token,
    peek: Token,
    no_struct_lit: bool,
}

impl Parser {
    pub fn new(input: &str) -> Self {
        let mut lexer = Lexer::new(input);
        let current = lexer.next_token();
        let peek = lexer.next_token();
        Self { lexer, current, peek, no_struct_lit: false }
    }

    fn advance(&mut self) -> Token {
        let next = self.lexer.next_token();
        let new_peek = std::mem::replace(&mut self.peek, next);
        std::mem::replace(&mut self.current, new_peek)
    }

    fn at_eof(&self) -> bool {
        matches!(self.current.token, TokenKind::EndOfFile)
    }

    fn at_op(&self, op: Operator) -> bool {
        matches!(&self.current.token, TokenKind::Operator(o) if *o == op)
    }

    fn at_keyword(&self, kw: Keyword) -> bool {
        matches!(&self.current.token, TokenKind::Keyword(k) if *k == kw)
    }

    fn eat_op(&mut self, op: Operator) -> bool {
        if self.at_op(op) {
            self.advance();
            true
        } else {
            false
        }
    }

    fn expect_op(&mut self, op: Operator) -> Result<(), ParseError> {
        if self.at_op(op) {
            self.advance();
            Ok(())
        } else {
            Err(self.err(format!(
                "expected '{}' but found '{}'",
                op.to_lexeme(),
                self.current.lexeme
            )))
        }
    }

    /// Consume a closing `>` for type-arg lists. Handles the `>>` token used
    /// to close nested generics (`Vec<Pair<int, U>>`) by splitting it into
    /// two `>` tokens.
    fn expect_close_gt(&mut self) -> Result<(), ParseError> {
        if self.at_op(Operator::GreaterThan) {
            self.advance();
            return Ok(());
        }
        if self.at_op(Operator::RightShift) {
            // Replace current `>>` with a `>` so the outer level can consume it.
            self.current.token = TokenKind::Operator(Operator::GreaterThan);
            self.current.lexeme = ">".to_string();
            self.current.column += 1;
            return Ok(());
        }
        Err(self.err(format!(
            "expected '>' but found '{}'",
            self.current.lexeme
        )))
    }

    fn expect_ident(&mut self) -> Result<String, ParseError> {
        if let TokenKind::Identifier(name) = &self.current.token {
            let name = name.clone();
            self.advance();
            Ok(name)
        } else {
            Err(self.err(format!(
                "expected identifier but found '{}'",
                self.current.lexeme
            )))
        }
    }

    fn expect_ident_with_pos(&mut self) -> Result<(String, Pos), ParseError> {
        let pos = self.current_pos();
        let name = self.expect_ident()?;
        Ok((name, pos))
    }

    /// Accepts an identifier or a keyword in member position (after `::` or `.`).
    /// Lets methods like `Arena::new`, `Foo::match` etc. work even when the
    /// member name collides with a Glide keyword.
    fn expect_ident_or_keyword(&mut self) -> Result<String, ParseError> {
        match &self.current.token {
            TokenKind::Identifier(name) => {
                let name = name.clone();
                self.advance();
                Ok(name)
            }
            TokenKind::Keyword(kw) => {
                let lex = kw.to_lexeme().to_string();
                self.advance();
                Ok(lex)
            }
            _ => Err(self.err(format!(
                "expected identifier but found '{}'",
                self.current.lexeme
            ))),
        }
    }

    fn err(&self, msg: impl Into<String>) -> ParseError {
        ParseError {
            message: msg.into(),
            line: self.current.line,
            column: self.current.column,
        }
    }

    pub fn parse_program(&mut self) -> Result<Program, ParseError> {
        let mut stmts = Vec::new();
        while !self.at_eof() {
            if let TokenKind::Error(msg) = &self.current.token {
                return Err(self.err(format!("lexer error: {}", msg)));
            }
            stmts.push(self.parse_stmt()?);
        }
        Ok(stmts)
    }

    fn current_pos(&self) -> Pos {
        Pos { line: self.current.line, column: self.current.column }
    }

    fn parse_stmt(&mut self) -> Result<Stmt, ParseError> {
        let pos = self.current_pos();
        let mut is_pub = false;
        if self.at_keyword(Keyword::Pub) {
            self.advance();
            is_pub = true;
        }
        let kind = self.parse_stmt_kind()?;
        Ok(Stmt { kind, pos, is_pub, source_file: None })
    }

    fn parse_stmt_kind(&mut self) -> Result<StmtKind, ParseError> {
        if self.at_keyword(Keyword::Import) {
            self.parse_import()
        } else if self.at_keyword(Keyword::Let) {
            self.parse_let()
        } else if self.at_keyword(Keyword::Const) {
            self.parse_const()
        } else if self.at_keyword(Keyword::Fn) {
            self.parse_fn()
        } else if self.at_keyword(Keyword::Struct) {
            self.parse_struct_decl()
        } else if self.at_keyword(Keyword::Enum) {
            self.parse_enum()
        } else if self.at_keyword(Keyword::Match) {
            self.parse_match()
        } else if self.at_keyword(Keyword::Interface) {
            self.parse_interface()
        } else if self.at_keyword(Keyword::Impl) {
            self.parse_impl()
        } else if self.at_keyword(Keyword::If) {
            self.parse_if()
        } else if self.at_keyword(Keyword::While) {
            self.parse_while()
        } else if self.at_keyword(Keyword::For) {
            self.parse_for()
        } else if self.at_keyword(Keyword::Break) {
            self.advance();
            self.expect_op(Operator::Semicolon)?;
            Ok(StmtKind::Break)
        } else if self.at_keyword(Keyword::Continue) {
            self.advance();
            self.expect_op(Operator::Semicolon)?;
            Ok(StmtKind::Continue)
        } else if self.at_keyword(Keyword::Spawn) {
            self.parse_spawn()
        } else if self.at_keyword(Keyword::Defer) {
            self.parse_defer()
        } else if self.at_keyword(Keyword::Type) {
            self.parse_type_alias()
        } else if self.at_keyword(Keyword::Extern) {
            self.parse_extern()
        } else if self.at_keyword(Keyword::CInclude) {
            self.parse_c_include()
        } else if self.at_keyword(Keyword::CLink) {
            self.parse_c_link()
        } else if self.at_keyword(Keyword::Return) {
            self.parse_return()
        } else if self.at_op(Operator::LBrace) {
            let body = self.parse_block()?;
            Ok(StmtKind::Block(body))
        } else {
            self.parse_expr_stmt()
        }
    }

    fn parse_import(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'import'
        let path = match &self.current.token {
            TokenKind::String(s) => {
                let s = s.clone();
                self.advance();
                s
            }
            TokenKind::Identifier(_) => {
                let mut parts = Vec::new();
                parts.push(self.expect_ident()?);
                while self.at_op(Operator::DoubleColon) {
                    self.advance();
                    parts.push(self.expect_ident()?);
                }
                parts.join("/")
            }
            _ => return Err(self.err(format!(
                "expected string literal or path after 'import' but found '{}'",
                self.current.lexeme
            ))),
        };
        self.expect_op(Operator::Semicolon)?;
        Ok(StmtKind::Import(path))
    }

    fn parse_enum(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'enum'
        let name = self.expect_ident()?;
        self.expect_op(Operator::LBrace)?;
        let mut variants = Vec::new();
        while !self.at_op(Operator::RBrace) && !self.at_eof() {
            if let TokenKind::Error(msg) = &self.current.token {
                return Err(self.err(format!("lexer error: {}", msg)));
            }
            let pos = self.current_pos();
            let vname = self.expect_ident()?;
            let mut fields = Vec::new();
            if self.eat_op(Operator::LParen) {
                if !self.at_op(Operator::RParen) {
                    loop {
                        fields.push(self.parse_type()?);
                        if !self.eat_op(Operator::Comma) {
                            break;
                        }
                    }
                }
                self.expect_op(Operator::RParen)?;
            }
            variants.push(EnumVariant { name: vname, fields, pos });
            if !self.eat_op(Operator::Comma) {
                break;
            }
        }
        self.expect_op(Operator::RBrace)?;
        Ok(StmtKind::Enum { name, variants })
    }

    fn parse_match(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'match'
        let scrutinee = self.parse_cond_expr()?;
        self.expect_op(Operator::LBrace)?;
        let mut arms = Vec::new();
        while !self.at_op(Operator::RBrace) && !self.at_eof() {
            if let TokenKind::Error(msg) = &self.current.token {
                return Err(self.err(format!("lexer error: {}", msg)));
            }
            let pattern = self.parse_pattern()?;
            self.expect_op(Operator::ArrowFunction)?; // '=>'
            let body = if self.at_op(Operator::LBrace) {
                self.parse_block()?
            } else {
                let pos = self.current_pos();
                let lhs = self.parse_expr(0)?;
                let expr = self.parse_assign_chain(lhs)?;
                vec![Stmt {
                    kind: StmtKind::Expr(expr),
                    pos,
                    is_pub: false,
                    source_file: None,
                }]
            };
            arms.push(MatchArm { pattern, body });
            self.eat_op(Operator::Comma);
        }
        self.expect_op(Operator::RBrace)?;
        Ok(StmtKind::Match { scrutinee, arms })
    }

    fn parse_pattern(&mut self) -> Result<Pattern, ParseError> {
        let pos = self.current_pos();
        if let TokenKind::Operator(Operator::BitAnd) = self.current.token {
            // Reserved for &-patterns later; not supported now.
        }
        match &self.current.token {
            TokenKind::Identifier(name) if name == "_" => {
                self.advance();
                Ok(Pattern { kind: PatternKind::Wildcard, pos })
            }
            TokenKind::Identifier(_) => {
                let first = self.expect_ident()?;
                if self.at_op(Operator::DoubleColon) {
                    self.advance();
                    let variant = self.expect_ident()?;
                    let bindings = self.parse_pattern_args()?;
                    Ok(Pattern {
                        kind: PatternKind::Variant {
                            enum_name: Some(first),
                            variant,
                            bindings,
                        },
                        pos,
                    })
                } else if self.at_op(Operator::LParen) {
                    // bare variant constructor with args: Some(x)
                    let bindings = self.parse_pattern_args()?;
                    Ok(Pattern {
                        kind: PatternKind::Variant {
                            enum_name: None,
                            variant: first,
                            bindings,
                        },
                        pos,
                    })
                } else {
                    // Bare ident: could be a variant (no args) OR a binding.
                    // Resolution: if name starts with uppercase letter, treat as variant.
                    // Otherwise, treat as binding.
                    let is_variant = first.chars().next()
                        .map(|c| c.is_ascii_uppercase())
                        .unwrap_or(false);
                    if is_variant {
                        Ok(Pattern {
                            kind: PatternKind::Variant {
                                enum_name: None,
                                variant: first,
                                bindings: Vec::new(),
                            },
                            pos,
                        })
                    } else {
                        Ok(Pattern { kind: PatternKind::Bind(first), pos })
                    }
                }
            }
            TokenKind::Int(_) | TokenKind::Float(_) | TokenKind::String(_)
            | TokenKind::Char(_)
            | TokenKind::Keyword(Keyword::True) | TokenKind::Keyword(Keyword::False)
            | TokenKind::Keyword(Keyword::Null) => {
                let lit = self.parse_prefix()?;
                Ok(Pattern {
                    kind: PatternKind::Literal(Box::new(lit)),
                    pos,
                })
            }
            _ => Err(self.err(format!(
                "expected a pattern but found '{}'",
                self.current.lexeme
            ))),
        }
    }

    fn parse_pattern_args(&mut self) -> Result<Vec<Pattern>, ParseError> {
        if !self.eat_op(Operator::LParen) {
            return Ok(Vec::new());
        }
        let mut out = Vec::new();
        if !self.at_op(Operator::RParen) {
            loop {
                out.push(self.parse_pattern()?);
                if !self.eat_op(Operator::Comma) {
                    break;
                }
            }
        }
        self.expect_op(Operator::RParen)?;
        Ok(out)
    }

    fn parse_interface(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'interface'
        let name = self.expect_ident()?;
        self.expect_op(Operator::LBrace)?;
        let mut methods = Vec::new();
        while !self.at_op(Operator::RBrace) && !self.at_eof() {
            if let TokenKind::Error(msg) = &self.current.token {
                return Err(self.err(format!("lexer error: {}", msg)));
            }
            let pos = self.current_pos();
            if !self.at_keyword(Keyword::Fn) {
                return Err(self.err(format!(
                    "expected 'fn' inside interface body but found '{}'",
                    self.current.lexeme
                )));
            }
            self.advance(); // 'fn'
            let mname = self.expect_ident()?;
            self.expect_op(Operator::LParen)?;
            let params = self.parse_params()?;
            let ret_type = if self.eat_op(Operator::Arrow) {
                Some(self.parse_type()?)
            } else {
                None
            };
            self.expect_op(Operator::Semicolon)?;
            methods.push(MethodSig { name: mname, params, ret_type, pos });
        }
        self.expect_op(Operator::RBrace)?;
        Ok(StmtKind::Interface { name, methods })
    }

    fn parse_impl(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'impl'

        // Optional type params: `impl<T> Vector<T> { ... }`.
        let type_params = self.parse_type_params()?;

        let first_name = self.expect_ident()?;
        let (interface, target) = if self.at_keyword(Keyword::For) {
            self.advance();
            let target = self.parse_type()?;
            (Some(first_name), target)
        } else if self.at_op(Operator::LessThan) {
            // Generic target: `impl<T> Vector<T>`.
            self.advance(); // '<'
            let mut args = Vec::new();
            loop {
                args.push(self.parse_type()?);
                if !self.eat_op(Operator::Comma) { break; }
            }
            self.expect_close_gt()?;
            (None, Type::Generic { name: first_name, args })
        } else {
            (None, Type::Named(first_name))
        };

        self.expect_op(Operator::LBrace)?;
        let mut methods = Vec::new();
        while !self.at_op(Operator::RBrace) && !self.at_eof() {
            if let TokenKind::Error(msg) = &self.current.token {
                return Err(self.err(format!("lexer error: {}", msg)));
            }
            let pos = self.current_pos();
            let mut is_pub = true;
            if self.at_keyword(Keyword::Pub) {
                self.advance();
                is_pub = true;
            }
            if !self.at_keyword(Keyword::Fn) {
                return Err(self.err(format!(
                    "expected 'fn' inside impl body but found '{}'",
                    self.current.lexeme
                )));
            }
            let kind = self.parse_fn()?;
            methods.push(Stmt { kind, pos, is_pub, source_file: None });
        }
        self.expect_op(Operator::RBrace)?;
        Ok(StmtKind::Impl { interface, type_params, target, methods })
    }

    fn parse_struct_decl(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'struct'
        let name = self.expect_ident()?;
        let type_params = self.parse_type_params()?;
        self.expect_op(Operator::LBrace)?;
        let mut fields = Vec::new();
        while !self.at_op(Operator::RBrace) && !self.at_eof() {
            if let TokenKind::Error(msg) = &self.current.token {
                return Err(self.err(format!("lexer error: {}", msg)));
            }
            let (fname, fpos) = self.expect_ident_with_pos()?;
            self.expect_op(Operator::Colon)?;
            let fty = self.parse_type()?;
            fields.push(Field { name: fname, ty: fty, pos: fpos });
            if !self.eat_op(Operator::Comma) {
                break;
            }
        }
        self.expect_op(Operator::RBrace)?;
        Ok(StmtKind::Struct { name, type_params, fields })
    }

    fn parse_type_params(&mut self) -> Result<Vec<String>, ParseError> {
        if !self.eat_op(Operator::LessThan) {
            return Ok(Vec::new());
        }
        let mut tps = Vec::new();
        loop {
            tps.push(self.expect_ident()?);
            if !self.eat_op(Operator::Comma) {
                break;
            }
        }
        self.expect_close_gt()?;
        Ok(tps)
    }

    fn parse_if(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'if'
        let cond = self.parse_cond_expr()?;
        let then_block = self.parse_block()?;

        let else_block = if self.at_keyword(Keyword::Else) {
            self.advance();
            if self.at_keyword(Keyword::If) {
                let pos = self.current_pos();
                let kind = self.parse_if()?;
                Some(vec![Stmt { kind, pos, is_pub: false, source_file: None }])
            } else {
                Some(self.parse_block()?)
            }
        } else {
            None
        };

        Ok(StmtKind::If { cond, then_block, else_block })
    }

    fn parse_while(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'while'
        let cond = self.parse_cond_expr()?;
        let body = self.parse_block()?;
        Ok(StmtKind::While { cond, body })
    }

    fn parse_for(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'for'

        let saved = self.no_struct_lit;
        self.no_struct_lit = true;

        let init = self.parse_for_init()?;
        self.expect_op(Operator::Semicolon)?;

        let cond = if self.at_op(Operator::Semicolon) {
            None
        } else {
            Some(self.parse_expr(0)?)
        };
        self.expect_op(Operator::Semicolon)?;

        let step = if self.at_op(Operator::LBrace) {
            None
        } else {
            let lhs = self.parse_expr(0)?;
            Some(self.parse_assign_chain(lhs)?)
        };

        self.no_struct_lit = saved;

        let body = self.parse_block()?;
        Ok(StmtKind::For {
            init: init.map(Box::new),
            cond,
            step,
            body,
        })
    }

    fn parse_cond_expr(&mut self) -> Result<Expr, ParseError> {
        let saved = self.no_struct_lit;
        self.no_struct_lit = true;
        let e = self.parse_expr(0);
        self.no_struct_lit = saved;
        e
    }

    fn parse_for_init(&mut self) -> Result<Option<Stmt>, ParseError> {
        if self.at_op(Operator::Semicolon) {
            return Ok(None);
        }
        let pos = self.current_pos();
        let kind = if self.at_keyword(Keyword::Let) {
            self.advance();
            let is_mut = if self.at_keyword(Keyword::Mut) {
                self.advance();
                true
            } else {
                false
            };
            let name = self.expect_ident()?;
            let ty = if self.eat_op(Operator::Colon) {
                Some(self.parse_type()?)
            } else {
                None
            };
            let value = if self.eat_op(Operator::Assignment) {
                Some(self.parse_expr(0)?)
            } else {
                None
            };
            StmtKind::Let { name, ty, value, is_mut, is_owned: false }
        } else {
            let lhs = self.parse_expr(0)?;
            let expr = self.parse_assign_chain(lhs)?;
            StmtKind::Expr(expr)
        };
        Ok(Some(Stmt { kind, pos, is_pub: false, source_file: None }))
    }

    fn parse_block(&mut self) -> Result<Vec<Stmt>, ParseError> {
        self.expect_op(Operator::LBrace)?;
        let mut stmts = Vec::new();
        while !self.at_op(Operator::RBrace) && !self.at_eof() {
            if let TokenKind::Error(msg) = &self.current.token {
                return Err(self.err(format!("lexer error: {}", msg)));
            }
            stmts.push(self.parse_stmt()?);
        }
        self.expect_op(Operator::RBrace)?;
        Ok(stmts)
    }

    fn parse_fn(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'fn'
        // Allow keywords as fn names (e.g. `fn new`, `fn match`) since the
        // context after `fn` is unambiguous.
        let name = self.expect_ident_or_keyword()?;
        let type_params = self.parse_type_params()?;

        self.expect_op(Operator::LParen)?;
        let params = self.parse_params()?;

        let ret_type = if self.eat_op(Operator::Arrow) {
            Some(self.parse_type()?)
        } else {
            None
        };

        let body = self.parse_block()?;
        Ok(StmtKind::Fn { name, type_params, params, ret_type, body })
    }

    fn parse_params(&mut self) -> Result<Vec<Param>, ParseError> {
        let mut params = Vec::new();
        if self.at_op(Operator::RParen) {
            self.advance();
            return Ok(params);
        }
        loop {
            let (name, pos) = self.expect_ident_with_pos()?;
            self.expect_op(Operator::Colon)?;
            let ty = self.parse_type()?;
            params.push(Param { name, ty, pos });

            if self.eat_op(Operator::Comma) {
                if self.at_op(Operator::RParen) {
                    self.advance();
                    return Ok(params);
                }
                continue;
            }
            self.expect_op(Operator::RParen)?;
            return Ok(params);
        }
    }

    fn parse_let(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'let'
        let is_mut = if self.at_keyword(Keyword::Mut) {
            self.advance();
            true
        } else {
            false
        };
        let name = self.expect_ident()?;

        let ty = if self.eat_op(Operator::Colon) {
            Some(self.parse_type()?)
        } else {
            None
        };

        let value = if self.eat_op(Operator::Assignment) {
            Some(self.parse_expr(0)?)
        } else {
            None
        };

        self.expect_op(Operator::Semicolon)?;
        Ok(StmtKind::Let { name, ty, value, is_mut, is_owned: false })
    }

    fn parse_fn_expr(&mut self, is_move: bool, pos: Pos) -> Result<Expr, ParseError> {
        self.advance(); // 'fn'
        self.expect_op(Operator::LParen)?;
        let params = self.parse_params()?;
        let ret_type = if self.eat_op(Operator::Arrow) {
            Some(self.parse_type()?)
        } else {
            None
        };
        let body = self.parse_block()?;
        Ok(Expr::new(
            ExprKind::FnExpr { params, ret_type, body, is_move },
            pos,
        ))
    }

    fn parse_type_alias(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'type'
        let name = self.expect_ident()?;
        let ty = self.parse_type()?;
        self.expect_op(Operator::Semicolon)?;
        Ok(StmtKind::TypeAlias { name, ty })
    }

    fn parse_extern(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'extern'
        // optional ABI string: extern "C" fn ...;
        if let TokenKind::String(_) = &self.current.token {
            self.advance();
        }
        if self.at_keyword(Keyword::Type) {
            return self.parse_extern_type();
        }
        if !self.at_keyword(Keyword::Fn) {
            return Err(self.err(format!(
                "expected `fn` or `type` after `extern` but found '{}'",
                self.current.lexeme
            )));
        }
        self.advance(); // 'fn'
        let name = self.expect_ident()?;
        self.expect_op(Operator::LParen)?;
        let (params, variadic) = self.parse_extern_params()?;
        let ret_type = if self.eat_op(Operator::Arrow) {
            Some(self.parse_type()?)
        } else {
            None
        };
        self.expect_op(Operator::Semicolon)?;
        Ok(StmtKind::ExternFn { name, params, ret_type, variadic })
    }

    fn parse_extern_type(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'type'
        let name = self.expect_ident()?;
        let c_repr = if self.eat_op(Operator::Assignment) {
            match &self.current.token {
                TokenKind::String(s) => {
                    let s = s.clone();
                    self.advance();
                    Some(s)
                }
                _ => return Err(self.err(format!(
                    "expected string literal after `=` in extern type but found '{}'",
                    self.current.lexeme
                ))),
            }
        } else {
            None
        };
        self.expect_op(Operator::Semicolon)?;
        Ok(StmtKind::ExternType { name, c_repr })
    }

    fn parse_c_include(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'c_include'
        let path = match &self.current.token {
            TokenKind::String(s) => s.clone(),
            _ => return Err(self.err(format!(
                "expected string literal after `c_include` but found '{}'",
                self.current.lexeme
            ))),
        };
        self.advance();
        self.expect_op(Operator::Semicolon)?;
        Ok(StmtKind::CInclude(path))
    }

    fn parse_c_link(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'c_link'
        let lib = match &self.current.token {
            TokenKind::String(s) => s.clone(),
            _ => return Err(self.err(format!(
                "expected string literal after `c_link` but found '{}'",
                self.current.lexeme
            ))),
        };
        self.advance();
        self.expect_op(Operator::Semicolon)?;
        Ok(StmtKind::CLink(lib))
    }

    fn parse_extern_params(&mut self) -> Result<(Vec<Param>, bool), ParseError> {
        let mut params = Vec::new();
        if self.at_op(Operator::RParen) {
            self.advance();
            return Ok((params, false));
        }
        loop {
            if self.at_op(Operator::Ellipsis) {
                self.advance();
                self.expect_op(Operator::RParen)?;
                return Ok((params, true));
            }
            let (name, pos) = self.expect_ident_with_pos()?;
            self.expect_op(Operator::Colon)?;
            let ty = self.parse_type()?;
            params.push(Param { name, ty, pos });

            if self.eat_op(Operator::Comma) {
                if self.at_op(Operator::RParen) {
                    self.advance();
                    return Ok((params, false));
                }
                continue;
            }
            self.expect_op(Operator::RParen)?;
            return Ok((params, false));
        }
    }

    fn parse_const(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'const'
        let name = self.expect_ident()?;

        let ty = if self.eat_op(Operator::Colon) {
            Some(self.parse_type()?)
        } else {
            None
        };

        self.expect_op(Operator::Assignment)?;
        let value = self.parse_expr(0)?;
        self.expect_op(Operator::Semicolon)?;
        Ok(StmtKind::Const { name, ty, value })
    }

    fn parse_defer(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'defer'
        let expr = self.parse_expr(0)?;
        self.expect_op(Operator::Semicolon)?;
        Ok(StmtKind::Defer(expr))
    }

    fn parse_spawn(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'spawn'
        let expr = self.parse_expr(0)?;
        if !matches!(&expr.kind, ExprKind::Call(..)) {
            return Err(self.err(
                "`spawn` requires a function call, e.g. `spawn worker(args);`",
            ));
        }
        self.expect_op(Operator::Semicolon)?;
        Ok(StmtKind::Spawn(expr))
    }

    fn parse_return(&mut self) -> Result<StmtKind, ParseError> {
        self.advance(); // 'return'
        let value = if self.at_op(Operator::Semicolon) {
            None
        } else {
            Some(self.parse_expr(0)?)
        };
        self.expect_op(Operator::Semicolon)?;
        Ok(StmtKind::Return(value))
    }

    fn parse_expr_stmt(&mut self) -> Result<StmtKind, ParseError> {
        let lhs = self.parse_expr(0)?;
        let expr = self.parse_assign_chain(lhs)?;
        self.expect_op(Operator::Semicolon)?;
        Ok(StmtKind::Expr(expr))
    }

    fn parse_assign_chain(&mut self, lhs: Expr) -> Result<Expr, ParseError> {
        if let Some(op) = self.match_assign_op() {
            let rhs = self.parse_expr(0)?;
            let rhs = self.parse_assign_chain(rhs)?;
            let pos = lhs.pos;
            Ok(Expr::new(ExprKind::Assign(Box::new(lhs), op, Box::new(rhs)), pos))
        } else {
            Ok(lhs)
        }
    }

    fn match_assign_op(&mut self) -> Option<AssignOp> {
        let op = match &self.current.token {
            TokenKind::Operator(Operator::Assignment) => AssignOp::Assign,
            TokenKind::Operator(Operator::PlusAssign) => AssignOp::AddAssign,
            TokenKind::Operator(Operator::MinusAssign) => AssignOp::SubAssign,
            TokenKind::Operator(Operator::StarAssign) => AssignOp::MulAssign,
            TokenKind::Operator(Operator::SlashAssign) => AssignOp::DivAssign,
            TokenKind::Operator(Operator::PercentAssign) => AssignOp::ModAssign,
            TokenKind::Operator(Operator::BitAndAssign) => AssignOp::BitAndAssign,
            TokenKind::Operator(Operator::BitOrAssign) => AssignOp::BitOrAssign,
            TokenKind::Operator(Operator::BitXorAssign) => AssignOp::BitXorAssign,
            TokenKind::Operator(Operator::LeftShiftAssign) => AssignOp::ShlAssign,
            TokenKind::Operator(Operator::RightShiftAssign) => AssignOp::ShrAssign,
            _ => return None,
        };
        self.advance();
        Some(op)
    }

    fn parse_type(&mut self) -> Result<Type, ParseError> {
        if self.eat_op(Operator::Star) {
            let inner = self.parse_type()?;
            return Ok(Type::Pointer(Box::new(inner)));
        }
        if self.at_op(Operator::LBracket) && matches!(&self.peek.token, TokenKind::Operator(Operator::RBracket)) {
            self.advance(); // '['
            self.advance(); // ']'
            let inner = self.parse_type()?;
            return Ok(Type::Slice(Box::new(inner)));
        }
        if self.eat_op(Operator::BitAnd) {
            if self.at_keyword(Keyword::Mut) {
                self.advance();
                let inner = self.parse_type()?;
                return Ok(Type::BorrowMut(Box::new(inner)));
            }
            let inner = self.parse_type()?;
            return Ok(Type::Borrow(Box::new(inner)));
        }
        if self.at_keyword(Keyword::Chan) {
            self.advance();
            self.expect_op(Operator::LessThan)?;
            let inner = self.parse_type()?;
            self.expect_close_gt()?;
            return Ok(Type::Chan(Box::new(inner)));
        }
        if self.at_keyword(Keyword::Fn) {
            self.advance();
            self.expect_op(Operator::LParen)?;
            let mut params = Vec::new();
            if !self.at_op(Operator::RParen) {
                loop {
                    // accept either `name: type` or just `type`
                    if let TokenKind::Identifier(_) = &self.current.token {
                        if matches!(&self.peek.token, TokenKind::Operator(Operator::Colon)) {
                            self.advance(); // consume name
                            self.advance(); // consume ':'
                            params.push(self.parse_type()?);
                        } else {
                            params.push(self.parse_type()?);
                        }
                    } else {
                        params.push(self.parse_type()?);
                    }
                    if !self.eat_op(Operator::Comma) {
                        break;
                    }
                }
            }
            self.expect_op(Operator::RParen)?;
            let ret = if self.eat_op(Operator::Arrow) {
                Some(Box::new(self.parse_type()?))
            } else {
                None
            };
            return Ok(Type::FnPtr { params, ret });
        }
        let name = self.expect_ident()?;
        if self.at_op(Operator::LessThan) {
            self.advance(); // '<'
            let mut args = Vec::new();
            loop {
                args.push(self.parse_type()?);
                if !self.eat_op(Operator::Comma) { break; }
            }
            self.expect_close_gt()?;
            return Ok(Type::Generic { name, args });
        }
        Ok(Type::Named(name))
    }

    // ---- Pratt expression parser ----

    fn parse_expr(&mut self, min_bp: u8) -> Result<Expr, ParseError> {
        let mut left = self.parse_prefix()?;
        const POSTFIX_BP: u8 = 150;

        loop {
            let pos = left.pos;
            if self.at_op(Operator::LParen) {
                if POSTFIX_BP < min_bp { break; }
                self.advance();
                let args = self.parse_call_args()?;
                left = Expr::new(ExprKind::Call(Box::new(left), args), pos);
                continue;
            }
            if self.at_op(Operator::LBracket) {
                if POSTFIX_BP < min_bp { break; }
                self.advance();
                let saved = self.no_struct_lit;
                self.no_struct_lit = false;
                let idx = self.parse_expr(0)?;
                self.no_struct_lit = saved;
                self.expect_op(Operator::RBracket)?;
                left = Expr::new(ExprKind::Index(Box::new(left), Box::new(idx)), pos);
                continue;
            }
            if self.at_op(Operator::Dot) {
                if POSTFIX_BP < min_bp { break; }
                self.advance();
                let name = self.expect_ident()?;
                left = Expr::new(ExprKind::Member(Box::new(left), name), pos);
                continue;
            }
            if self.at_op(Operator::Increment) {
                if POSTFIX_BP < min_bp { break; }
                self.advance();
                left = Expr::new(ExprKind::Unary(UnaryOp::PostInc, Box::new(left)), pos);
                continue;
            }
            if self.at_op(Operator::Decrement) {
                if POSTFIX_BP < min_bp { break; }
                self.advance();
                left = Expr::new(ExprKind::Unary(UnaryOp::PostDec, Box::new(left)), pos);
                continue;
            }
            if self.at_keyword(Keyword::As) {
                const AS_BP: u8 = 120;
                if AS_BP < min_bp { break; }
                self.advance();
                let target = self.parse_type()?;
                left = Expr::new(ExprKind::Cast(Box::new(left), target), pos);
                continue;
            }

            let (lbp, rbp, op) = match self.peek_infix() {
                Some(x) => x,
                None => break,
            };
            if lbp < min_bp { break; }
            self.advance();
            let right = self.parse_expr(rbp)?;
            left = Expr::new(
                ExprKind::Binary(op, Box::new(left), Box::new(right)),
                pos,
            );
        }

        Ok(left)
    }

    fn parse_prefix(&mut self) -> Result<Expr, ParseError> {
        const PREFIX_BP: u8 = 130;
        let pos = self.current_pos();
        let tok = self.current.clone();
        let kind = match tok.token {
            TokenKind::Int(n)    => { self.advance(); ExprKind::Int(n) }
            TokenKind::Float(f)  => { self.advance(); ExprKind::Float(f) }
            TokenKind::String(s) => { self.advance(); ExprKind::String(s) }
            TokenKind::Char(c)   => { self.advance(); ExprKind::Char(c) }
            TokenKind::Keyword(Keyword::True)  => { self.advance(); ExprKind::Bool(true) }
            TokenKind::Keyword(Keyword::False) => { self.advance(); ExprKind::Bool(false) }
            TokenKind::Keyword(Keyword::Null)  => { self.advance(); ExprKind::Null }
            TokenKind::Keyword(Keyword::Sizeof) => {
                self.advance();
                self.expect_op(Operator::LParen)?;
                let target = self.parse_type()?;
                self.expect_op(Operator::RParen)?;
                ExprKind::Sizeof(target)
            }
            TokenKind::Keyword(Keyword::New) => {
                self.advance();
                let type_name = self.expect_ident()?;
                self.expect_op(Operator::LBrace)?;
                let fields = self.parse_struct_lit_fields()?;
                ExprKind::New { type_name, fields }
            }
            TokenKind::Keyword(Keyword::Fn) => {
                return self.parse_fn_expr(false, pos);
            }
            TokenKind::Keyword(Keyword::Move) => {
                self.advance();
                if !self.at_keyword(Keyword::Fn) {
                    return Err(self.err(format!(
                        "expected `fn` after `move` but found '{}'",
                        self.current.lexeme
                    )));
                }
                return self.parse_fn_expr(true, pos);
            }
            TokenKind::Identifier(name) => {
                self.advance();
                if self.at_op(Operator::DoubleColon) {
                    self.advance();
                    let method = self.expect_ident_or_keyword()?;
                    ExprKind::Path { ty: name, member: method }
                } else if matches!(self.current.token, TokenKind::Operator(Operator::Bang))
                    && matches!(self.peek.token, TokenKind::Operator(Operator::LParen))
                {
                    self.advance(); // '!'
                    self.advance(); // '('
                    let args = self.parse_call_args()?;
                    ExprKind::MacroCall { name, args }
                } else if !self.no_struct_lit && self.at_op(Operator::LBrace) {
                    self.advance(); // '{'
                    let fields = self.parse_struct_lit_fields()?;
                    ExprKind::StructLit { type_name: name, fields }
                } else {
                    ExprKind::Ident(name)
                }
            }
            TokenKind::Operator(Operator::LParen) => {
                self.advance();
                let saved = self.no_struct_lit;
                self.no_struct_lit = false;
                let inner = self.parse_expr(0)?;
                self.no_struct_lit = saved;
                self.expect_op(Operator::RParen)?;
                return Ok(Expr::new(inner.kind, pos));
            }
            TokenKind::Operator(Operator::LBracket) => {
                self.advance();
                let saved = self.no_struct_lit;
                self.no_struct_lit = false;
                let mut elems = Vec::new();
                if !self.at_op(Operator::RBracket) {
                    loop {
                        elems.push(self.parse_expr(0)?);
                        if self.eat_op(Operator::Comma) {
                            if self.at_op(Operator::RBracket) {
                                break;
                            }
                            continue;
                        }
                        break;
                    }
                }
                self.no_struct_lit = saved;
                self.expect_op(Operator::RBracket)?;
                ExprKind::ArrayLit { elements: elems, elem_type: None, as_slice: false }
            }
            TokenKind::Operator(op) => {
                let unary = match op {
                    Operator::Minus  => UnaryOp::Neg,
                    Operator::Bang   => UnaryOp::Not,
                    Operator::BitNot => UnaryOp::BitNot,
                    Operator::Star   => UnaryOp::Deref,
                    Operator::BitAnd => {
                        self.advance();
                        if self.at_keyword(Keyword::Mut) {
                            self.advance();
                            let rhs = self.parse_expr(PREFIX_BP)?;
                            return Ok(Expr::new(
                                ExprKind::Unary(UnaryOp::AddrOfMut, Box::new(rhs)),
                                pos,
                            ));
                        }
                        let rhs = self.parse_expr(PREFIX_BP)?;
                        return Ok(Expr::new(
                            ExprKind::Unary(UnaryOp::AddrOf, Box::new(rhs)),
                            pos,
                        ));
                    }
                    _ => return Err(self.err(format!(
                        "unexpected operator '{}' at start of expression",
                        tok.lexeme
                    ))),
                };
                self.advance();
                let rhs = self.parse_expr(PREFIX_BP)?;
                ExprKind::Unary(unary, Box::new(rhs))
            }
            TokenKind::Error(msg) => return Err(self.err(format!("lexer error: {}", msg))),
            _ => return Err(self.err(format!(
                "unexpected token '{}' at start of expression",
                tok.lexeme
            ))),
        };
        Ok(Expr::new(kind, pos))
    }

    fn parse_call_args(&mut self) -> Result<Vec<Expr>, ParseError> {
        let mut args = Vec::new();
        if self.at_op(Operator::RParen) {
            self.advance();
            return Ok(args);
        }
        let saved = self.no_struct_lit;
        self.no_struct_lit = false;
        let result = (|| -> Result<Vec<Expr>, ParseError> {
            loop {
                args.push(self.parse_expr(0)?);
                if self.eat_op(Operator::Comma) {
                    if self.at_op(Operator::RParen) {
                        self.advance();
                        return Ok(std::mem::take(&mut args));
                    }
                    continue;
                }
                self.expect_op(Operator::RParen)?;
                return Ok(std::mem::take(&mut args));
            }
        })();
        self.no_struct_lit = saved;
        result
    }

    fn parse_struct_lit_fields(&mut self) -> Result<Vec<(String, Expr)>, ParseError> {
        let mut fields = Vec::new();
        if self.at_op(Operator::RBrace) {
            self.advance();
            return Ok(fields);
        }
        let saved = self.no_struct_lit;
        self.no_struct_lit = false;
        loop {
            let name = self.expect_ident()?;
            self.expect_op(Operator::Colon)?;
            let value = self.parse_expr(0)?;
            fields.push((name, value));
            if self.eat_op(Operator::Comma) {
                if self.at_op(Operator::RBrace) {
                    self.advance();
                    self.no_struct_lit = saved;
                    return Ok(fields);
                }
                continue;
            }
            self.expect_op(Operator::RBrace)?;
            self.no_struct_lit = saved;
            return Ok(fields);
        }
    }

    fn peek_infix(&self) -> Option<(u8, u8, BinaryOp)> {
        let op = match &self.current.token {
            TokenKind::Operator(o) => *o,
            _ => return None,
        };
        let info = match op {
            Operator::Or           => (20,  21,  BinaryOp::Or),
            Operator::And          => (30,  31,  BinaryOp::And),
            Operator::BitOr        => (40,  41,  BinaryOp::BitOr),
            Operator::BitXor       => (50,  51,  BinaryOp::BitXor),
            Operator::BitAnd       => (60,  61,  BinaryOp::BitAnd),
            Operator::Equal        => (70,  71,  BinaryOp::Eq),
            Operator::NotEqual     => (70,  71,  BinaryOp::NotEq),
            Operator::LessThan     => (80,  81,  BinaryOp::Lt),
            Operator::LessEqual    => (80,  81,  BinaryOp::LtEq),
            Operator::GreaterThan  => (80,  81,  BinaryOp::Gt),
            Operator::GreaterEqual => (80,  81,  BinaryOp::GtEq),
            Operator::LeftShift    => (90,  91,  BinaryOp::Shl),
            Operator::RightShift   => (90,  91,  BinaryOp::Shr),
            Operator::Plus         => (100, 101, BinaryOp::Add),
            Operator::Minus        => (100, 101, BinaryOp::Sub),
            Operator::Star         => (110, 111, BinaryOp::Mul),
            Operator::Slash        => (110, 111, BinaryOp::Div),
            Operator::Percent      => (110, 111, BinaryOp::Mod),
            _ => return None,
        };
        Some(info)
    }
}
