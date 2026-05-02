use crate::types::Type;

#[derive(Debug, Clone, PartialEq)]
pub enum UnaryOp {
    Neg,
    Not,
    BitNot,
    Deref,
    AddrOf,
    AddrOfMut,
    PostInc,
    PostDec,
}

#[derive(Debug, Clone, PartialEq)]
pub enum BinaryOp {
    Add,
    Sub,
    Mul,
    Div,
    Mod,

    Eq,
    NotEq,
    Lt,
    LtEq,
    Gt,
    GtEq,

    And,
    Or,

    BitAnd,
    BitOr,
    BitXor,
    Shl,
    Shr,
}

#[derive(Debug, Clone, PartialEq)]
pub enum AssignOp {
    Assign,
    AddAssign,
    SubAssign,
    MulAssign,
    DivAssign,
    ModAssign,
    BitAndAssign,
    BitOrAssign,
    BitXorAssign,
    ShlAssign,
    ShrAssign,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Expr {
    pub kind: ExprKind,
    pub pos: Pos,
}

#[derive(Debug, Clone, PartialEq)]
pub enum ExprKind {
    Int(i64),
    Float(f64),
    String(String),
    Char(char),
    Bool(bool),
    Null,

    Ident(String),

    Unary(UnaryOp, Box<Expr>),
    Binary(BinaryOp, Box<Expr>, Box<Expr>),
    Assign(Box<Expr>, AssignOp, Box<Expr>),

    Call(Box<Expr>, Vec<Expr>),
    Index(Box<Expr>, Box<Expr>),
    Member(Box<Expr>, String),

    Cast(Box<Expr>, Type),
    Sizeof(Type),

    StructLit {
        type_name: String,
        fields: Vec<(String, Expr)>,
    },

    New {
        type_name: String,
        fields: Vec<(String, Expr)>,
    },

    ArrayLit {
        elements: Vec<Expr>,
        elem_type: Option<Type>,
        as_slice: bool,
    },

    AddrOfTemp {
        value: Box<Expr>,
        ty: Type,
    },

    MacroCall {
        name: String,
        args: Vec<Expr>,
    },

    Path {
        ty: String,
        member: String,
    },

    EnumCtor {
        enum_name: String,
        variant: String,
        args: Vec<Expr>,
    },

    FnExpr {
        params: Vec<Param>,
        ret_type: Option<Type>,
        body: Vec<Stmt>,
        is_move: bool,
    },
}

impl Expr {
    pub fn new(kind: ExprKind, pos: Pos) -> Self {
        Self { kind, pos }
    }
}

#[derive(Debug, Clone, PartialEq)]
pub struct Param {
    pub name: String,
    pub ty: Type,
    pub pos: Pos,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Field {
    pub name: String,
    pub ty: Type,
    pub pos: Pos,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash, Default)]
pub struct Pos {
    pub line: usize,
    pub column: usize,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Stmt {
    pub kind: StmtKind,
    pub pos: Pos,
    pub is_pub: bool,
    pub source_file: Option<std::path::PathBuf>,
}

#[derive(Debug, Clone, PartialEq)]
pub enum StmtKind {
    Let {
        name: String,
        ty: Option<Type>,
        value: Option<Expr>,
        is_mut: bool,
        is_owned: bool,
    },
    Const {
        name: String,
        ty: Option<Type>,
        value: Expr,
    },
    Fn {
        name: String,
        type_params: Vec<String>,
        params: Vec<Param>,
        ret_type: Option<Type>,
        body: Vec<Stmt>,
    },
    Struct {
        name: String,
        type_params: Vec<String>,
        fields: Vec<Field>,
    },
    Block(Vec<Stmt>),
    If {
        cond: Expr,
        then_block: Vec<Stmt>,
        else_block: Option<Vec<Stmt>>,
    },
    While {
        cond: Expr,
        body: Vec<Stmt>,
    },
    For {
        init: Option<Box<Stmt>>,
        cond: Option<Expr>,
        step: Option<Expr>,
        body: Vec<Stmt>,
    },
    Break,
    Continue,
    Return(Option<Expr>),
    Expr(Expr),
    Spawn(Expr),

    Interface {
        name: String,
        methods: Vec<MethodSig>,
    },
    Impl {
        interface: Option<String>,
        type_params: Vec<String>,
        target: Type,
        methods: Vec<Stmt>,
    },
    Import(String),
    Enum {
        name: String,
        variants: Vec<EnumVariant>,
    },
    Match {
        scrutinee: Expr,
        arms: Vec<MatchArm>,
    },
    Defer(Expr),
    TypeAlias {
        name: String,
        ty: Type,
    },
    ExternFn {
        name: String,
        params: Vec<Param>,
        ret_type: Option<Type>,
        variadic: bool,
    },
    ExternType {
        name: String,
        c_repr: Option<String>,
    },
    CInclude(String),
    CLink(String),
}

#[derive(Debug, Clone, PartialEq)]
pub struct EnumVariant {
    pub name: String,
    pub fields: Vec<Type>,
    pub pos: Pos,
}

#[derive(Debug, Clone, PartialEq)]
pub struct MatchArm {
    pub pattern: Pattern,
    pub body: Vec<Stmt>,
}

#[derive(Debug, Clone, PartialEq)]
pub struct Pattern {
    pub kind: PatternKind,
    pub pos: Pos,
}

#[derive(Debug, Clone, PartialEq)]
pub enum PatternKind {
    Wildcard,
    Bind(String),
    Literal(Box<Expr>),
    Variant {
        enum_name: Option<String>,
        variant: String,
        bindings: Vec<Pattern>,
    },
}

#[derive(Debug, Clone, PartialEq)]
pub struct MethodSig {
    pub name: String,
    pub params: Vec<Param>,
    pub ret_type: Option<Type>,
    pub pos: Pos,
}

pub type Program = Vec<Stmt>;
