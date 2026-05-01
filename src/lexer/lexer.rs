use crate::lexer::{Keyword, Operator, Token, TokenKind};

pub struct Lexer {
    pub input: Vec<char>,
    pub position: usize,
    pub read_position: usize,
    pub ch: char,
    pub line: usize,
    pub column: usize,
}

impl Lexer {
    pub fn new(input: &str) -> Self {
        let mut lexer = Self {
            input: input.chars().collect(),
            position: 0,
            read_position: 0,
            ch: '\0',
            line: 1,
            column: 0,
        };
        lexer.read_char();
        lexer
    }

    pub fn read_char(&mut self) {
        if self.ch == '\n' {
            self.line += 1;
            self.column = 0;
        }

        if self.read_position >= self.input.len() {
            self.ch = '\0';
        } else {
            self.ch = self.input[self.read_position];
        }

        self.position = self.read_position;
        self.read_position += 1;
        self.column += 1;
    }

    pub fn peek_char(&self) -> char {
        if self.read_position >= self.input.len() {
            return '\0';
        }
        self.input[self.read_position]
    }

    fn peek_at(&self, offset: usize) -> char {
        let idx = self.read_position + offset;
        if idx >= self.input.len() {
            return '\0';
        }
        self.input[idx]
    }

    pub fn skip_trivia(&mut self) {
        loop {
            match self.ch {
                ' ' | '\t' | '\r' | '\n' => self.read_char(),
                '/' if self.peek_char() == '/' => {
                    while self.ch != '\n' && self.ch != '\0' {
                        self.read_char();
                    }
                }
                '/' if self.peek_char() == '*' => {
                    self.read_char(); // '/'
                    self.read_char(); // '*'
                    while self.ch != '\0' && !(self.ch == '*' && self.peek_char() == '/') {
                        self.read_char();
                    }
                    if self.ch == '*' {
                        self.read_char(); // '*'
                        self.read_char(); // '/'
                    }
                }
                _ => break,
            }
        }
    }

    pub fn next_token(&mut self) -> Token {
        self.skip_trivia();

        let start_line = self.line;
        let start_column = self.column;

        if self.ch.is_digit(10) {
            let (kind, lexeme) = self.lex_number();
            return Token { token: kind, lexeme, line: start_line, column: start_column };
        }

        if is_letter(self.ch) {
            let literal = self.read_identifier();
            let kind = match Keyword::from_str(&literal) {
                Some(kw) => TokenKind::Keyword(kw),
                None => TokenKind::Identifier(literal.clone()),
            };
            return Token { token: kind, lexeme: literal, line: start_line, column: start_column };
        }

        if self.ch == '"' {
            let (kind, lexeme) = self.lex_string();
            return Token { token: kind, lexeme, line: start_line, column: start_column };
        }

        if self.ch == '\'' {
            let (kind, lexeme) = self.lex_char_literal();
            return Token { token: kind, lexeme, line: start_line, column: start_column };
        }

        let token_kind = match self.ch {
            '+' => {
                if self.peek_char() == '=' {
                    self.read_char();
                    TokenKind::Operator(Operator::PlusAssign)
                } else if self.peek_char() == '+' {
                    self.read_char();
                    TokenKind::Operator(Operator::Increment)
                } else {
                    TokenKind::Operator(Operator::Plus)
                }
            }
            '-' => {
                if self.peek_char() == '=' {
                    self.read_char();
                    TokenKind::Operator(Operator::MinusAssign)
                } else if self.peek_char() == '-' {
                    self.read_char();
                    TokenKind::Operator(Operator::Decrement)
                } else if self.peek_char() == '>' {
                    self.read_char();
                    TokenKind::Operator(Operator::Arrow)
                } else {
                    TokenKind::Operator(Operator::Minus)
                }
            }
            '*' => self.match_compound('=', Operator::StarAssign, Operator::Star),
            '/' => self.match_compound('=', Operator::SlashAssign, Operator::Slash),
            '%' => self.match_compound('=', Operator::PercentAssign, Operator::Percent),

            '=' => {
                if self.peek_char() == '=' {
                    self.read_char();
                    TokenKind::Operator(Operator::Equal)
                } else if self.peek_char() == '>' {
                    self.read_char();
                    TokenKind::Operator(Operator::ArrowFunction)
                } else {
                    TokenKind::Operator(Operator::Assignment)
                }
            }
            '!' => self.match_compound('=', Operator::NotEqual, Operator::Bang),

            '<' => {
                if self.peek_char() == '<' && self.peek_at(1) == '=' {
                    self.read_char();
                    self.read_char();
                    TokenKind::Operator(Operator::LeftShiftAssign)
                } else if self.peek_char() == '<' {
                    self.read_char();
                    TokenKind::Operator(Operator::LeftShift)
                } else if self.peek_char() == '=' {
                    self.read_char();
                    TokenKind::Operator(Operator::LessEqual)
                } else {
                    TokenKind::Operator(Operator::LessThan)
                }
            }
            '>' => {
                if self.peek_char() == '>' && self.peek_at(1) == '=' {
                    self.read_char();
                    self.read_char();
                    TokenKind::Operator(Operator::RightShiftAssign)
                } else if self.peek_char() == '>' {
                    self.read_char();
                    TokenKind::Operator(Operator::RightShift)
                } else if self.peek_char() == '=' {
                    self.read_char();
                    TokenKind::Operator(Operator::GreaterEqual)
                } else {
                    TokenKind::Operator(Operator::GreaterThan)
                }
            }

            '&' => {
                if self.peek_char() == '&' {
                    self.read_char();
                    TokenKind::Operator(Operator::And)
                } else if self.peek_char() == '=' {
                    self.read_char();
                    TokenKind::Operator(Operator::BitAndAssign)
                } else {
                    TokenKind::Operator(Operator::BitAnd)
                }
            }
            '|' => {
                if self.peek_char() == '|' {
                    self.read_char();
                    TokenKind::Operator(Operator::Or)
                } else if self.peek_char() == '=' {
                    self.read_char();
                    TokenKind::Operator(Operator::BitOrAssign)
                } else {
                    TokenKind::Operator(Operator::BitOr)
                }
            }
            '^' => self.match_compound('=', Operator::BitXorAssign, Operator::BitXor),
            '~' => TokenKind::Operator(Operator::BitNot),

            ';' => TokenKind::Operator(Operator::Semicolon),
            ',' => TokenKind::Operator(Operator::Comma),
            '.' => TokenKind::Operator(Operator::Dot),
            ':' => {
                if self.peek_char() == ':' {
                    self.read_char();
                    TokenKind::Operator(Operator::DoubleColon)
                } else {
                    TokenKind::Operator(Operator::Colon)
                }
            }
            '?' => TokenKind::Operator(Operator::QuestionMark),
            '(' => TokenKind::Operator(Operator::LParen),
            ')' => TokenKind::Operator(Operator::RParen),
            '{' => TokenKind::Operator(Operator::LBrace),
            '}' => TokenKind::Operator(Operator::RBrace),
            '[' => TokenKind::Operator(Operator::LBracket),
            ']' => TokenKind::Operator(Operator::RBracket),

            '\0' => TokenKind::EndOfFile,

            _ => TokenKind::Unknown(self.ch),
        };

        self.read_char();

        let lexeme = match &token_kind {
            TokenKind::Operator(op) => op.to_lexeme().to_string(),
            TokenKind::Unknown(c) => c.to_string(),
            TokenKind::EndOfFile => "\0".to_string(),
            _ => String::new(),
        };

        Token { token: token_kind, lexeme, line: start_line, column: start_column }
    }

    fn match_compound(&mut self, expected: char, with: Operator, without: Operator) -> TokenKind {
        if self.peek_char() == expected {
            self.read_char();
            TokenKind::Operator(with)
        } else {
            TokenKind::Operator(without)
        }
    }

    fn read_identifier(&mut self) -> String {
        let position = self.position;
        while is_letter(self.ch) || self.ch.is_digit(10) {
            self.read_char();
        }
        self.input[position..self.position].iter().collect()
    }

    fn lex_number(&mut self) -> (TokenKind, String) {
        let start = self.position;

        if self.ch == '0' {
            let next = self.peek_char();
            if next == 'x' || next == 'X' {
                return self.lex_radix_int(start, 16, |c| c.is_ascii_hexdigit());
            }
            if next == 'b' || next == 'B' {
                return self.lex_radix_int(start, 2, |c| c == '0' || c == '1');
            }
            if next == 'o' || next == 'O' {
                return self.lex_radix_int(start, 8, |c| ('0'..='7').contains(&c));
            }
        }

        while self.ch.is_digit(10) || self.ch == '_' {
            self.read_char();
        }

        let mut is_float = false;

        // `.` must be followed by a digit so `1.method()` stays as int . ident.
        if self.ch == '.' && self.peek_char().is_digit(10) {
            is_float = true;
            self.read_char();
            while self.ch.is_digit(10) || self.ch == '_' {
                self.read_char();
            }
        }

        if self.ch == 'e' || self.ch == 'E' {
            is_float = true;
            self.read_char();
            if self.ch == '+' || self.ch == '-' {
                self.read_char();
            }
            let exp_start = self.position;
            while self.ch.is_digit(10) || self.ch == '_' {
                self.read_char();
            }
            if self.position == exp_start {
                let lex: String = self.input[start..self.position].iter().collect();
                return (TokenKind::Error(format!("expected digits in exponent: '{}'", lex)), lex);
            }
        }

        let lex: String = self.input[start..self.position].iter().collect();
        let cleaned: String = lex.chars().filter(|c| *c != '_').collect();

        if is_float {
            match cleaned.parse::<f64>() {
                Ok(v) => (TokenKind::Float(v), lex),
                Err(_) => (TokenKind::Error(format!("invalid float literal: '{}'", lex)), lex),
            }
        } else {
            match cleaned.parse::<i64>() {
                Ok(v) => (TokenKind::Int(v), lex),
                Err(_) => (TokenKind::Error(format!("invalid integer literal: '{}'", lex)), lex),
            }
        }
    }

    fn lex_radix_int<F: Fn(char) -> bool>(
        &mut self,
        start: usize,
        radix: u32,
        is_digit: F,
    ) -> (TokenKind, String) {
        self.read_char(); // '0'
        self.read_char(); // 'x' / 'b' / 'o'
        let digits_start = self.position;
        while is_digit(self.ch) || self.ch == '_' {
            self.read_char();
        }
        let lex: String = self.input[start..self.position].iter().collect();
        if self.position == digits_start {
            return (TokenKind::Error(format!("expected digits after '{}'", &lex)), lex);
        }
        let cleaned: String = lex[2..].chars().filter(|c| *c != '_').collect();
        match i64::from_str_radix(&cleaned, radix) {
            Ok(v) => (TokenKind::Int(v), lex),
            Err(_) => (TokenKind::Error(format!("invalid integer literal: '{}'", lex)), lex),
        }
    }

    fn lex_string(&mut self) -> (TokenKind, String) {
        let start = self.position;
        self.read_char(); // opening '"'
        let mut value = String::new();
        loop {
            match self.ch {
                '"' => {
                    self.read_char();
                    let lex: String = self.input[start..self.position].iter().collect();
                    return (TokenKind::String(value), lex);
                }
                '\0' => {
                    let lex: String = self.input[start..self.position].iter().collect();
                    return (TokenKind::Error("unterminated string literal".to_string()), lex);
                }
                '\\' => {
                    self.read_char();
                    match self.read_escape() {
                        Ok(c) => value.push(c),
                        Err(e) => {
                            let lex: String = self.input[start..self.position].iter().collect();
                            return (TokenKind::Error(e), lex);
                        }
                    }
                }
                c => {
                    value.push(c);
                    self.read_char();
                }
            }
        }
    }

    fn lex_char_literal(&mut self) -> (TokenKind, String) {
        let start = self.position;
        self.read_char(); // opening '\''

        if self.ch == '\'' {
            self.read_char();
            let lex: String = self.input[start..self.position].iter().collect();
            return (TokenKind::Error("empty char literal".to_string()), lex);
        }
        if self.ch == '\0' {
            let lex: String = self.input[start..self.position].iter().collect();
            return (TokenKind::Error("unterminated char literal".to_string()), lex);
        }

        let c = if self.ch == '\\' {
            self.read_char();
            match self.read_escape() {
                Ok(c) => c,
                Err(e) => {
                    let lex: String = self.input[start..self.position].iter().collect();
                    return (TokenKind::Error(e), lex);
                }
            }
        } else {
            let c = self.ch;
            self.read_char();
            c
        };

        if self.ch != '\'' {
            let lex: String = self.input[start..self.position].iter().collect();
            return (
                TokenKind::Error("char literal must contain exactly one codepoint".to_string()),
                lex,
            );
        }
        self.read_char(); // closing '\''
        let lex: String = self.input[start..self.position].iter().collect();
        (TokenKind::Char(c), lex)
    }

    fn read_escape(&mut self) -> Result<char, String> {
        if self.ch == 'x' {
            self.read_char();
            let h1 = self.ch;
            if !h1.is_ascii_hexdigit() {
                return Err(format!("invalid hex escape: '\\x{}'", h1));
            }
            self.read_char();
            let h2 = self.ch;
            if !h2.is_ascii_hexdigit() {
                return Err(format!("invalid hex escape: '\\x{}{}'", h1, h2));
            }
            self.read_char();
            let hex: String = [h1, h2].iter().collect();
            return match u32::from_str_radix(&hex, 16) {
                Ok(v) => Ok(char::from_u32(v).unwrap_or('?')),
                Err(_) => Err(format!("invalid hex escape: '\\x{}'", hex)),
            };
        }
        let c = match self.ch {
            'n' => '\n',
            't' => '\t',
            'r' => '\r',
            '\\' => '\\',
            '"' => '"',
            '\'' => '\'',
            '0' => '\0',
            '\0' => return Err("unterminated escape sequence".to_string()),
            other => return Err(format!("invalid escape: '\\{}'", other)),
        };
        self.read_char();
        Ok(c)
    }
}

fn is_letter(ch: char) -> bool {
    ch.is_alphabetic() || ch == '_'
}
