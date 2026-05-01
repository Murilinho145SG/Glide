#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Operator {
    // arithmetic
    Plus,
    Minus,
    Star,
    Slash,
    Percent,

    // compound assignment
    PlusAssign,
    MinusAssign,
    StarAssign,
    SlashAssign,
    PercentAssign,

    // comparison
    Equal,
    NotEqual,
    GreaterThan,
    GreaterEqual,
    LessThan,
    LessEqual,

    // logical
    And,
    Or,
    Bang,

    // bitwise
    BitAnd,
    BitOr,
    BitXor,
    BitNot,
    LeftShift,
    RightShift,

    // compound bitwise
    BitAndAssign,
    BitOrAssign,
    BitXorAssign,
    LeftShiftAssign,
    RightShiftAssign,

    // increment / decrement
    Increment,
    Decrement,

    // assignment
    Assignment,

    // delimiters
    LBrace,
    RBrace,
    LParen,
    RParen,
    LBracket,
    RBracket,

    // punctuation
    Comma,
    Dot,
    Semicolon,
    Colon,
    DoubleColon,
    QuestionMark,

    // arrows
    Arrow,         // ->  (return type)
    ArrowFunction, // =>  (lambda / match arm)
}

impl Operator {
    pub fn to_lexeme(&self) -> &'static str {
        match self {
            Operator::Plus          => "+",
            Operator::Minus         => "-",
            Operator::Star          => "*",
            Operator::Slash         => "/",
            Operator::Percent       => "%",

            Operator::PlusAssign    => "+=",
            Operator::MinusAssign   => "-=",
            Operator::StarAssign    => "*=",
            Operator::SlashAssign   => "/=",
            Operator::PercentAssign => "%=",

            Operator::Equal         => "==",
            Operator::NotEqual      => "!=",
            Operator::GreaterThan   => ">",
            Operator::GreaterEqual  => ">=",
            Operator::LessThan      => "<",
            Operator::LessEqual     => "<=",

            Operator::And           => "&&",
            Operator::Or            => "||",
            Operator::Bang          => "!",

            Operator::BitAnd        => "&",
            Operator::BitOr         => "|",
            Operator::BitXor        => "^",
            Operator::BitNot        => "~",
            Operator::LeftShift     => "<<",
            Operator::RightShift    => ">>",

            Operator::BitAndAssign     => "&=",
            Operator::BitOrAssign      => "|=",
            Operator::BitXorAssign     => "^=",
            Operator::LeftShiftAssign  => "<<=",
            Operator::RightShiftAssign => ">>=",

            Operator::Increment     => "++",
            Operator::Decrement     => "--",

            Operator::Assignment    => "=",

            Operator::LBrace        => "{",
            Operator::RBrace        => "}",
            Operator::LParen        => "(",
            Operator::RParen        => ")",
            Operator::LBracket      => "[",
            Operator::RBracket      => "]",

            Operator::Comma         => ",",
            Operator::Dot           => ".",
            Operator::Semicolon     => ";",
            Operator::Colon         => ":",
            Operator::DoubleColon   => "::",
            Operator::QuestionMark  => "?",

            Operator::Arrow         => "->",
            Operator::ArrowFunction => "=>",
        }
    }

    pub fn from_str(value: &str) -> Option<Operator> {
        match value {
            "+"  => Some(Operator::Plus),
            "-"  => Some(Operator::Minus),
            "*"  => Some(Operator::Star),
            "/"  => Some(Operator::Slash),
            "%"  => Some(Operator::Percent),

            "+=" => Some(Operator::PlusAssign),
            "-=" => Some(Operator::MinusAssign),
            "*=" => Some(Operator::StarAssign),
            "/=" => Some(Operator::SlashAssign),
            "%=" => Some(Operator::PercentAssign),

            "==" => Some(Operator::Equal),
            "!=" => Some(Operator::NotEqual),
            ">"  => Some(Operator::GreaterThan),
            ">=" => Some(Operator::GreaterEqual),
            "<"  => Some(Operator::LessThan),
            "<=" => Some(Operator::LessEqual),

            "&&" => Some(Operator::And),
            "||" => Some(Operator::Or),
            "!"  => Some(Operator::Bang),

            "&"  => Some(Operator::BitAnd),
            "|"  => Some(Operator::BitOr),
            "^"  => Some(Operator::BitXor),
            "~"  => Some(Operator::BitNot),
            "<<" => Some(Operator::LeftShift),
            ">>" => Some(Operator::RightShift),

            "&="  => Some(Operator::BitAndAssign),
            "|="  => Some(Operator::BitOrAssign),
            "^="  => Some(Operator::BitXorAssign),
            "<<=" => Some(Operator::LeftShiftAssign),
            ">>=" => Some(Operator::RightShiftAssign),

            "++" => Some(Operator::Increment),
            "--" => Some(Operator::Decrement),

            "="  => Some(Operator::Assignment),

            "{"  => Some(Operator::LBrace),
            "}"  => Some(Operator::RBrace),
            "("  => Some(Operator::LParen),
            ")"  => Some(Operator::RParen),
            "["  => Some(Operator::LBracket),
            "]"  => Some(Operator::RBracket),

            ","  => Some(Operator::Comma),
            "."  => Some(Operator::Dot),
            ";"  => Some(Operator::Semicolon),
            ":"  => Some(Operator::Colon),
            "::" => Some(Operator::DoubleColon),
            "?"  => Some(Operator::QuestionMark),

            "->" => Some(Operator::Arrow),
            "=>" => Some(Operator::ArrowFunction),

            _ => None,
        }
    }
}
