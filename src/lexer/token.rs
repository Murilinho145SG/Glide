use crate::lexer::{Keyword, Operator};

#[derive(Debug, Clone, PartialEq)]
pub enum TokenKind {
    Identifier(String),
    Keyword(Keyword),
    Operator(Operator),

    Int(i64),
    Float(f64),
    String(String),
    Char(char),

    EndOfFile,

    Unknown(char),
    Error(String),
}

#[derive(Debug, Clone, PartialEq)]
pub struct Token {
    pub token: TokenKind,
    pub lexeme: String,
    pub line: usize,
    pub column: usize,
}
