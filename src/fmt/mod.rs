use crate::ast::*;
use crate::types::Type;

pub fn format(program: &Program) -> String {
    let mut f = Formatter::new();
    f.emit_program(program);
    f.finish()
}

struct Formatter {
    out: String,
    indent: usize,
}

const INDENT_UNIT: &str = "    ";

const PREC_NONE: u8 = 0;
const PREC_ASSIGN: u8 = 10;
const PREC_OR: u8 = 20;
const PREC_AND: u8 = 30;
const PREC_BIT_OR: u8 = 40;
const PREC_BIT_XOR: u8 = 50;
const PREC_BIT_AND: u8 = 60;
const PREC_EQ: u8 = 70;
const PREC_CMP: u8 = 80;
const PREC_SHIFT: u8 = 90;
const PREC_ADD: u8 = 100;
const PREC_MUL: u8 = 110;
const PREC_CAST: u8 = 120;
const PREC_UNARY: u8 = 130;
const PREC_POSTFIX: u8 = 150;

impl Formatter {
    fn new() -> Self {
        Self { out: String::new(), indent: 0 }
    }

    fn finish(mut self) -> String {
        if !self.out.ends_with('\n') {
            self.out.push('\n');
        }
        self.out
    }

    fn write(&mut self, s: &str) {
        self.out.push_str(s);
    }

    fn write_indent(&mut self) {
        for _ in 0..self.indent {
            self.out.push_str(INDENT_UNIT);
        }
    }

    fn emit_program(&mut self, program: &Program) {
        for (i, s) in program.iter().enumerate() {
            if i > 0 {
                self.write("\n");
            }
            self.emit_top_stmt(s);
        }
    }

    fn emit_block(&mut self, stmts: &[Stmt]) {
        for (i, s) in stmts.iter().enumerate() {
            if i > 0 && has_blank_line_between(&stmts[i - 1], s) {
                self.write("\n");
            }
            self.emit_stmt(s);
        }
    }

    fn emit_top_stmt(&mut self, s: &Stmt) {
        match &s.kind {
            StmtKind::Fn { name, params, ret_type, body } => {
                self.write_indent();
                if s.is_pub { self.write("pub "); }
                self.write("fn ");
                self.write(name);
                self.write("(");
                for (i, p) in params.iter().enumerate() {
                    if i > 0 {
                        self.write(", ");
                    }
                    self.write(&p.name);
                    self.write(": ");
                    self.write(&format_type(&p.ty));
                }
                self.write(")");
                if let Some(rt) = ret_type {
                    self.write(" -> ");
                    self.write(&format_type(rt));
                }
                self.write(" {\n");
                self.indent += 1;
                self.emit_block(body);
                self.indent -= 1;
                self.write_indent();
                self.write("}\n");
            }
            StmtKind::Struct { name, fields } => {
                self.write_indent();
                if s.is_pub { self.write("pub "); }
                self.write("struct ");
                self.write(name);
                if fields.is_empty() {
                    self.write(" {}\n");
                    return;
                }
                self.write(" {\n");
                self.indent += 1;
                for f in fields {
                    self.write_indent();
                    self.write(&f.name);
                    self.write(": ");
                    self.write(&format_type(&f.ty));
                    self.write(",\n");
                }
                self.indent -= 1;
                self.write_indent();
                self.write("}\n");
            }
            StmtKind::Let { .. } | StmtKind::Const { .. } => {
                self.write_indent();
                self.emit_simple_stmt(s);
            }
            StmtKind::Import(p) => {
                self.write("import ");
                if is_path_like(p) {
                    self.write(&p.replace('/', "::"));
                } else {
                    self.write("\"");
                    self.write(&format_string(p));
                    self.write("\"");
                }
                self.write(";\n");
            }
            StmtKind::Interface { name, methods } => {
                self.write_indent();
                self.write("interface ");
                self.write(name);
                if methods.is_empty() {
                    self.write(" {}\n");
                    return;
                }
                self.write(" {\n");
                self.indent += 1;
                for m in methods {
                    self.write_indent();
                    self.write("fn ");
                    self.write(&m.name);
                    self.write("(");
                    for (i, p) in m.params.iter().enumerate() {
                        if i > 0 { self.write(", "); }
                        self.write(&p.name);
                        self.write(": ");
                        self.write(&format_type(&p.ty));
                    }
                    self.write(")");
                    if let Some(rt) = &m.ret_type {
                        self.write(" -> ");
                        self.write(&format_type(rt));
                    }
                    self.write(";\n");
                }
                self.indent -= 1;
                self.write_indent();
                self.write("}\n");
            }
            StmtKind::Impl { interface, target, methods } => {
                self.write_indent();
                self.write("impl ");
                if let Some(iface) = interface {
                    self.write(iface);
                    self.write(" for ");
                }
                self.write(&format_type(target));
                if methods.is_empty() {
                    self.write(" {}\n");
                    return;
                }
                self.write(" {\n");
                self.indent += 1;
                for m in methods {
                    self.emit_top_stmt(m);
                }
                self.indent -= 1;
                self.write_indent();
                self.write("}\n");
            }
            StmtKind::Enum { name, variants } => {
                self.write_indent();
                if s.is_pub { self.write("pub "); }
                self.write("enum ");
                self.write(name);
                if variants.is_empty() {
                    self.write(" {}\n");
                    return;
                }
                self.write(" {\n");
                self.indent += 1;
                for v in variants {
                    self.write_indent();
                    self.write(&v.name);
                    if !v.fields.is_empty() {
                        self.write("(");
                        for (i, f) in v.fields.iter().enumerate() {
                            if i > 0 { self.write(", "); }
                            self.write(&format_type(f));
                        }
                        self.write(")");
                    }
                    self.write(",\n");
                }
                self.indent -= 1;
                self.write_indent();
                self.write("}\n");
            }
            _ => {
                self.write_indent();
                self.emit_simple_stmt(s);
            }
        }
    }

    fn emit_stmt(&mut self, s: &Stmt) {
        match &s.kind {
            StmtKind::Match { scrutinee, arms } => {
                self.write_indent();
                self.write("match ");
                self.emit_expr(scrutinee, PREC_NONE);
                self.write(" {\n");
                self.indent += 1;
                for arm in arms {
                    self.write_indent();
                    self.emit_pattern(&arm.pattern);
                    self.write(" => ");
                    if arm.body.len() == 1 {
                        if let StmtKind::Expr(e) = &arm.body[0].kind {
                            self.emit_expr(e, PREC_NONE);
                            self.write(",\n");
                            continue;
                        }
                    }
                    self.write("{\n");
                    self.indent += 1;
                    for s in &arm.body {
                        self.emit_stmt(s);
                    }
                    self.indent -= 1;
                    self.write_indent();
                    self.write("}\n");
                }
                self.indent -= 1;
                self.write_indent();
                self.write("}\n");
            }
            StmtKind::Block(stmts) => {
                self.write_indent();
                self.write("{\n");
                self.indent += 1;
                self.emit_block(stmts);
                self.indent -= 1;
                self.write_indent();
                self.write("}\n");
            }
            StmtKind::If { cond, then_block, else_block } => {
                self.write_indent();
                self.emit_if_chain(cond, then_block, else_block.as_deref());
                self.write("\n");
            }
            StmtKind::While { cond, body } => {
                self.write_indent();
                self.write("while ");
                self.emit_expr(cond, PREC_NONE);
                self.write(" {\n");
                self.indent += 1;
                self.emit_block(body);
                self.indent -= 1;
                self.write_indent();
                self.write("}\n");
            }
            StmtKind::For { init, cond, step, body } => {
                self.write_indent();
                self.write("for ");
                if let Some(init_stmt) = init {
                    self.emit_for_init(init_stmt);
                }
                self.write("; ");
                if let Some(c) = cond {
                    self.emit_expr(c, PREC_NONE);
                }
                self.write("; ");
                if let Some(st) = step {
                    self.emit_expr(st, PREC_NONE);
                }
                self.write(" {\n");
                self.indent += 1;
                self.emit_block(body);
                self.indent -= 1;
                self.write_indent();
                self.write("}\n");
            }
            StmtKind::Fn { .. } | StmtKind::Struct { .. } => {
                self.emit_top_stmt(s);
            }
            _ => {
                self.write_indent();
                self.emit_simple_stmt(s);
            }
        }
    }

    fn emit_if_chain(
        &mut self,
        cond: &Expr,
        then_block: &[Stmt],
        else_block: Option<&[Stmt]>,
    ) {
        self.write("if ");
        self.emit_expr(cond, PREC_NONE);
        self.write(" {\n");
        self.indent += 1;
        self.emit_block(then_block);
        self.indent -= 1;
        self.write_indent();
        self.write("}");

        let Some(else_b) = else_block else { return };

        if else_b.len() == 1 {
            if let StmtKind::If { cond, then_block, else_block } = &else_b[0].kind {
                self.write(" else ");
                self.emit_if_chain(cond, then_block, else_block.as_deref());
                return;
            }
        }
        self.write(" else {\n");
        self.indent += 1;
        self.emit_block(else_b);
        self.indent -= 1;
        self.write_indent();
        self.write("}");
    }

    fn emit_pattern(&mut self, pat: &Pattern) {
        match &pat.kind {
            PatternKind::Wildcard => self.write("_"),
            PatternKind::Bind(name) => self.write(name),
            PatternKind::Literal(e) => self.emit_expr(e, PREC_NONE),
            PatternKind::Variant { enum_name, variant, bindings } => {
                if let Some(en) = enum_name {
                    self.write(en);
                    self.write("::");
                }
                self.write(variant);
                if !bindings.is_empty() {
                    self.write("(");
                    for (i, b) in bindings.iter().enumerate() {
                        if i > 0 { self.write(", "); }
                        self.emit_pattern(b);
                    }
                    self.write(")");
                }
            }
        }
    }

    fn emit_simple_stmt(&mut self, s: &Stmt) {
        match &s.kind {
            StmtKind::Let { name, ty, value } => {
                self.write("let ");
                self.write(name);
                if let Some(t) = ty {
                    self.write(": ");
                    self.write(&format_type(t));
                }
                if let Some(v) = value {
                    self.write(" = ");
                    self.emit_expr(v, PREC_NONE);
                }
                self.write(";\n");
            }
            StmtKind::Const { name, ty, value } => {
                if s.is_pub { self.write("pub "); }
                self.write("const ");
                self.write(name);
                if let Some(t) = ty {
                    self.write(": ");
                    self.write(&format_type(t));
                }
                self.write(" = ");
                self.emit_expr(value, PREC_NONE);
                self.write(";\n");
            }
            StmtKind::Break => self.write("break;\n"),
            StmtKind::Continue => self.write("continue;\n"),
            StmtKind::Return(value) => {
                self.write("return");
                if let Some(v) = value {
                    self.write(" ");
                    self.emit_expr(v, PREC_NONE);
                }
                self.write(";\n");
            }
            StmtKind::Expr(e) => {
                self.emit_expr(e, PREC_NONE);
                self.write(";\n");
            }
            StmtKind::Spawn(e) => {
                self.write("spawn ");
                self.emit_expr(e, PREC_NONE);
                self.write(";\n");
            }
            _ => {}
        }
    }

    fn emit_for_init(&mut self, s: &Stmt) {
        match &s.kind {
            StmtKind::Let { name, ty, value } => {
                self.write("let ");
                self.write(name);
                if let Some(t) = ty {
                    self.write(": ");
                    self.write(&format_type(t));
                }
                if let Some(v) = value {
                    self.write(" = ");
                    self.emit_expr(v, PREC_NONE);
                }
            }
            StmtKind::Expr(e) => self.emit_expr(e, PREC_NONE),
            _ => {}
        }
    }

    fn emit_expr(&mut self, e: &Expr, parent_prec: u8) {
        match &e.kind {
            ExprKind::Int(n) => self.write(&n.to_string()),
            ExprKind::Float(f) => {
                let s = format!("{}", f);
                self.write(&s);
                if !s.contains('.') && !s.contains('e') && !s.contains('E') {
                    self.write(".0");
                }
            }
            ExprKind::Bool(b) => self.write(if *b { "true" } else { "false" }),
            ExprKind::Char(c) => {
                self.write("'");
                self.write(&format_char(*c));
                self.write("'");
            }
            ExprKind::String(s) => {
                self.write("\"");
                self.write(&format_string(s));
                self.write("\"");
            }
            ExprKind::Null => self.write("null"),
            ExprKind::Ident(n) => self.write(n),

            ExprKind::Unary(op, inner) => {
                let needs = parent_prec > PREC_UNARY && !matches!(op, UnaryOp::PostInc | UnaryOp::PostDec);
                if needs { self.write("("); }
                match op {
                    UnaryOp::Neg => { self.write("-"); self.emit_expr(inner, PREC_UNARY); }
                    UnaryOp::Not => { self.write("!"); self.emit_expr(inner, PREC_UNARY); }
                    UnaryOp::BitNot => { self.write("~"); self.emit_expr(inner, PREC_UNARY); }
                    UnaryOp::Deref => { self.write("*"); self.emit_expr(inner, PREC_UNARY); }
                    UnaryOp::AddrOf => { self.write("&"); self.emit_expr(inner, PREC_UNARY); }
                    UnaryOp::PostInc => { self.emit_expr(inner, PREC_POSTFIX); self.write("++"); }
                    UnaryOp::PostDec => { self.emit_expr(inner, PREC_POSTFIX); self.write("--"); }
                }
                if needs { self.write(")"); }
            }

            ExprKind::Binary(op, l, r) => {
                let prec = binop_prec(op);
                let needs = parent_prec > prec;
                if needs { self.write("("); }
                self.emit_expr(l, prec);
                self.write(" ");
                self.write(binop_str(op));
                self.write(" ");
                self.emit_expr(r, prec + 1);
                if needs { self.write(")"); }
            }

            ExprKind::Assign(lhs, op, rhs) => {
                let needs = parent_prec > PREC_ASSIGN;
                if needs { self.write("("); }
                self.emit_expr(lhs, PREC_ASSIGN + 1);
                self.write(" ");
                self.write(assignop_str(op));
                self.write(" ");
                self.emit_expr(rhs, PREC_ASSIGN);
                if needs { self.write(")"); }
            }

            ExprKind::Call(callee, args) => {
                self.emit_expr(callee, PREC_POSTFIX);
                self.write("(");
                for (i, a) in args.iter().enumerate() {
                    if i > 0 { self.write(", "); }
                    self.emit_expr(a, PREC_NONE);
                }
                self.write(")");
            }

            ExprKind::Index(obj, idx) => {
                self.emit_expr(obj, PREC_POSTFIX);
                self.write("[");
                self.emit_expr(idx, PREC_NONE);
                self.write("]");
            }

            ExprKind::Member(obj, name) => {
                if let ExprKind::Unary(UnaryOp::Deref, inner) = &obj.kind {
                    self.emit_expr(inner, PREC_POSTFIX);
                } else {
                    self.emit_expr(obj, PREC_POSTFIX);
                }
                self.write(".");
                self.write(name);
            }

            ExprKind::Cast(inner, ty) => {
                let needs = parent_prec > PREC_CAST;
                if needs { self.write("("); }
                self.emit_expr(inner, PREC_CAST);
                self.write(" as ");
                self.write(&format_type(ty));
                if needs { self.write(")"); }
            }

            ExprKind::Sizeof(ty) => {
                self.write("sizeof(");
                self.write(&format_type(ty));
                self.write(")");
            }

            ExprKind::StructLit { type_name, fields } => {
                self.write(type_name);
                self.emit_struct_lit_fields(fields);
            }

            ExprKind::New { type_name, fields } => {
                self.write("new ");
                self.write(type_name);
                self.emit_struct_lit_fields(fields);
            }

            ExprKind::ArrayLit { elements, .. } => {
                self.write("[");
                for (i, el) in elements.iter().enumerate() {
                    if i > 0 { self.write(", "); }
                    self.emit_expr(el, PREC_NONE);
                }
                self.write("]");
            }

            ExprKind::AddrOfTemp { value, .. } => {
                self.write("&");
                self.emit_expr(value, PREC_UNARY);
            }

            ExprKind::MacroCall { name, args } => {
                self.write(name);
                self.write("!(");
                for (i, a) in args.iter().enumerate() {
                    if i > 0 { self.write(", "); }
                    self.emit_expr(a, PREC_NONE);
                }
                self.write(")");
            }

            ExprKind::Path { ty, member } => {
                self.write(ty);
                self.write("::");
                self.write(member);
            }

            ExprKind::EnumCtor { enum_name, variant, args } => {
                self.write(enum_name);
                self.write("::");
                self.write(variant);
                if !args.is_empty() {
                    self.write("(");
                    for (i, a) in args.iter().enumerate() {
                        if i > 0 { self.write(", "); }
                        self.emit_expr(a, PREC_NONE);
                    }
                    self.write(")");
                }
            }
        }
    }

    fn emit_struct_lit_fields(&mut self, fields: &[(String, Expr)]) {
        if fields.is_empty() {
            self.write(" {}");
            return;
        }
        let multi = fields.iter().any(|(_, v)| has_nested_struct(v));
        if !multi {
            self.write(" { ");
            for (i, (n, v)) in fields.iter().enumerate() {
                if i > 0 { self.write(", "); }
                self.write(n);
                self.write(": ");
                self.emit_expr(v, PREC_NONE);
            }
            self.write(" }");
        } else {
            self.write(" {\n");
            self.indent += 1;
            for (n, v) in fields {
                self.write_indent();
                self.write(n);
                self.write(": ");
                self.emit_expr(v, PREC_NONE);
                self.write(",\n");
            }
            self.indent -= 1;
            self.write_indent();
            self.write("}");
        }
    }
}

fn has_nested_struct(e: &Expr) -> bool {
    matches!(&e.kind, ExprKind::StructLit { .. } | ExprKind::New { .. })
}

fn is_path_like(s: &str) -> bool {
    if s.is_empty() {
        return false;
    }
    s.split('/').all(|seg| {
        !seg.is_empty()
            && seg.chars().enumerate().all(|(i, c)| {
                if i == 0 {
                    c.is_ascii_alphabetic() || c == '_'
                } else {
                    c.is_ascii_alphanumeric() || c == '_'
                }
            })
    })
}

fn has_blank_line_between(prev: &Stmt, next: &Stmt) -> bool {
    next.pos.line > stmt_last_line(prev) + 1
}

fn stmt_last_line(s: &Stmt) -> usize {
    let mut max = s.pos.line;
    let mut has_body = false;
    match &s.kind {
        StmtKind::Fn { body, .. }
        | StmtKind::While { body, .. }
        | StmtKind::For { body, .. } => {
            has_body = true;
            for c in body {
                let m = stmt_last_line(c);
                if m > max { max = m; }
            }
        }
        StmtKind::If { then_block, else_block, .. } => {
            has_body = true;
            for c in then_block {
                let m = stmt_last_line(c);
                if m > max { max = m; }
            }
            if let Some(b) = else_block {
                for c in b {
                    let m = stmt_last_line(c);
                    if m > max { max = m; }
                }
            }
        }
        StmtKind::Block(stmts) => {
            has_body = true;
            for c in stmts {
                let m = stmt_last_line(c);
                if m > max { max = m; }
            }
        }
        StmtKind::Struct { .. } => {
            has_body = true;
        }
        _ => {}
    }
    if has_body { max + 1 } else { max }
}

fn format_type(ty: &Type) -> String {
    match ty {
        Type::Named(n) => n.clone(),
        Type::Pointer(inner) => format!("*{}", format_type(inner)),
        Type::Chan(inner) => format!("chan<{}>", format_type(inner)),
    }
}

fn format_string(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    for c in s.chars() {
        match c {
            '\n' => out.push_str("\\n"),
            '\t' => out.push_str("\\t"),
            '\r' => out.push_str("\\r"),
            '\\' => out.push_str("\\\\"),
            '"' => out.push_str("\\\""),
            '\0' => out.push_str("\\0"),
            c if (c as u32) < 0x20 || c as u32 == 0x7f => {
                out.push_str(&format!("\\x{:02x}", c as u32));
            }
            c => out.push(c),
        }
    }
    out
}

fn format_char(c: char) -> String {
    match c {
        '\n' => "\\n".into(),
        '\t' => "\\t".into(),
        '\r' => "\\r".into(),
        '\\' => "\\\\".into(),
        '\'' => "\\'".into(),
        '\0' => "\\0".into(),
        c if (c as u32) < 0x20 || c as u32 == 0x7f => {
            format!("\\x{:02x}", c as u32)
        }
        c => c.to_string(),
    }
}

fn binop_prec(op: &BinaryOp) -> u8 {
    match op {
        BinaryOp::Or => PREC_OR,
        BinaryOp::And => PREC_AND,
        BinaryOp::BitOr => PREC_BIT_OR,
        BinaryOp::BitXor => PREC_BIT_XOR,
        BinaryOp::BitAnd => PREC_BIT_AND,
        BinaryOp::Eq | BinaryOp::NotEq => PREC_EQ,
        BinaryOp::Lt | BinaryOp::LtEq | BinaryOp::Gt | BinaryOp::GtEq => PREC_CMP,
        BinaryOp::Shl | BinaryOp::Shr => PREC_SHIFT,
        BinaryOp::Add | BinaryOp::Sub => PREC_ADD,
        BinaryOp::Mul | BinaryOp::Div | BinaryOp::Mod => PREC_MUL,
    }
}

fn binop_str(op: &BinaryOp) -> &'static str {
    match op {
        BinaryOp::Add => "+", BinaryOp::Sub => "-",
        BinaryOp::Mul => "*", BinaryOp::Div => "/", BinaryOp::Mod => "%",
        BinaryOp::Eq => "==", BinaryOp::NotEq => "!=",
        BinaryOp::Lt => "<", BinaryOp::LtEq => "<=",
        BinaryOp::Gt => ">", BinaryOp::GtEq => ">=",
        BinaryOp::And => "&&", BinaryOp::Or => "||",
        BinaryOp::BitAnd => "&", BinaryOp::BitOr => "|", BinaryOp::BitXor => "^",
        BinaryOp::Shl => "<<", BinaryOp::Shr => ">>",
    }
}

fn assignop_str(op: &AssignOp) -> &'static str {
    match op {
        AssignOp::Assign => "=", AssignOp::AddAssign => "+=",
        AssignOp::SubAssign => "-=", AssignOp::MulAssign => "*=",
        AssignOp::DivAssign => "/=", AssignOp::ModAssign => "%=",
        AssignOp::BitAndAssign => "&=", AssignOp::BitOrAssign => "|=",
        AssignOp::BitXorAssign => "^=",
        AssignOp::ShlAssign => "<<=", AssignOp::ShrAssign => ">>=",
    }
}
