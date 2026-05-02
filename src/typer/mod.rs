use crate::ast::*;
use crate::types::Type;
use std::collections::HashMap;

#[derive(Debug, Clone)]
pub struct TypeError {
    pub message: String,
    pub line: usize,
    pub column: usize,
}

impl std::fmt::Display for TypeError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "type error at {}:{}: {}",
            self.line, self.column, self.message
        )
    }
}

#[derive(Clone)]
struct FnSig {
    params: Vec<Param>,
    ret_type: Option<Type>,
    variadic: bool,
    decl_pos: Option<Pos>,
    home_file: Option<std::path::PathBuf>,
    is_pub: bool,
}

#[derive(Debug, Clone)]
struct StructInfo {
    fields: Vec<Field>,
    #[allow(dead_code)]
    decl_pos: Pos,
    home_file: Option<std::path::PathBuf>,
    is_pub: bool,
}

#[derive(Debug, Clone)]
struct EnumInfo {
    variants: Vec<EnumVariant>,
    home_file: Option<std::path::PathBuf>,
    is_pub: bool,
}

#[derive(Debug, Clone)]
struct LocalSlot {
    ty: Type,
    decl_pos: Pos,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeclKind {
    Fn,
    Struct,
    Const,
    Let,
    Param,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum RefKind {
    Variable,
    Function,
    Type,
    Field,
}

#[derive(Debug, Clone)]
pub struct DeclInfo {
    pub pos: Pos,
    pub name: String,
    pub kind: DeclKind,
    pub ty: Option<Type>,
    pub detail: String,
    pub module: Option<String>,
    pub file: Option<std::path::PathBuf>,
    pub is_method: bool,
}

#[derive(Debug, Clone)]
pub struct RefInfo {
    pub pos: Pos,
    pub name: String,
    pub ty: Type,
    pub decl_pos: Option<Pos>,
    pub kind: RefKind,
}

#[derive(Debug, Default, Clone)]
pub struct SymbolIndex {
    pub decls: Vec<DeclInfo>,
    pub refs: Vec<RefInfo>,
    pub structs: HashMap<String, Vec<Field>>,
}

impl SymbolIndex {
    pub fn ref_at(&self, line: usize, col: usize) -> Option<&RefInfo> {
        for r in &self.refs {
            if r.pos.line == line
                && col >= r.pos.column
                && col < r.pos.column + r.name.chars().count()
            {
                return Some(r);
            }
        }
        None
    }
}

pub struct Typer {
    structs: HashMap<String, StructInfo>,
    enums: HashMap<String, EnumInfo>,
    fns: HashMap<String, FnSig>,
    scopes: Vec<HashMap<String, LocalSlot>>,
    current_ret: Option<Type>,
    current_pos: Pos,
    errors: Vec<TypeError>,
    index: SymbolIndex,
    current_module: Option<String>,
    current_file: Option<std::path::PathBuf>,
}

impl Default for Typer {
    fn default() -> Self {
        Self::new()
    }
}

impl Typer {
    pub fn new() -> Self {
        let mut t = Self {
            structs: HashMap::new(),
            enums: HashMap::new(),
            fns: HashMap::new(),
            scopes: Vec::new(),
            current_ret: None,
            current_pos: Pos::default(),
            errors: Vec::new(),
            index: SymbolIndex::default(),
            current_module: None,
            current_file: None,
        };
        t.register_builtins();
        t.register_stdlib();
        t.register_extras();
        t
    }

    fn register_extras(&mut self) {
        self.fns.insert("__glide_assert".into(), FnSig {
            params: vec![synth_param("cond", bool_ty()), synth_param("msg", string_ty())],
            ret_type: None,
            variadic: false,
            decl_pos: None,
            home_file: None,
            is_pub: true,
        });
        self.fns.insert("__glide_format".into(), FnSig {
            params: vec![synth_param("fmt", string_ty())],
            ret_type: Some(string_ty()),
            variadic: true,
            decl_pos: None,
            home_file: None,
            is_pub: true,
        });

        let entries: Vec<(&str, Vec<Param>, Option<Type>, &str)> = vec![
            ("panic",       vec![synth_param("msg", string_ty())],            None,                "fn panic(msg: string)"),
            ("args_count",  vec![],                                           Some(int_ty()),      "fn args_count() -> int"),
            ("args_at",     vec![synth_param("i", int_ty())],                 Some(string_ty()),   "fn args_at(i: int) -> string"),
            ("env_get",     vec![synth_param("name", string_ty())],           Some(string_ty()),   "fn env_get(name: string) -> string"),
            ("read_file",   vec![synth_param("path", string_ty())],           Some(string_ty()),   "fn read_file(path: string) -> string"),
            ("write_file",  vec![synth_param("path", string_ty()), synth_param("content", string_ty())], Some(bool_ty()), "fn write_file(path: string, content: string) -> bool"),
            ("file_exists", vec![synth_param("path", string_ty())],           Some(bool_ty()),     "fn file_exists(path: string) -> bool"),
        ];
        for (name, params, ret, detail) in entries {
            self.fns.insert(name.into(), FnSig {
                params,
                ret_type: ret.clone(),
                variadic: false,
                decl_pos: None,
                home_file: None,
                is_pub: true,
            });
            self.index.decls.push(DeclInfo {
                pos: Pos::default(),
                name: name.into(),
                kind: DeclKind::Fn,
                ty: ret,
                detail: detail.into(),
                module: None,
                file: None,
                is_method: false,
            });
        }
    }

    pub fn pre_register_module(
        &mut self,
        program: &Program,
        module: Option<String>,
        file: Option<std::path::PathBuf>,
    ) {
        let saved_module = self.current_module.take();
        let saved_file = self.current_file.take();
        self.current_module = module;
        self.current_file = file;
        for stmt in program {
            if !stmt.is_pub && !is_always_visible(&stmt.kind) {
                continue;
            }
            self.pre_register_stmt(stmt);
        }
        self.current_module = saved_module;
        self.current_file = saved_file;
    }

    fn register_builtins(&mut self) {
        let int = int_ty();
        let void_ptr = Type::Pointer(Box::new(void_ty()));
        let entries: Vec<(&str, Option<Type>, &str)> = vec![
            ("printf", Some(int.clone()),       "fn printf(fmt: string, ...) -> int"),
            ("scanf",  Some(int.clone()),       "fn scanf(fmt: string, ...) -> int"),
            ("puts",   Some(int.clone()),       "fn puts(s: string) -> int"),
            ("malloc", Some(void_ptr.clone()),  "fn malloc(size: int) -> *void"),
            ("calloc", Some(void_ptr.clone()),  "fn calloc(n: int, size: int) -> *void"),
            ("free",   None,                    "fn free(p: *void)"),
            ("strlen", Some(int.clone()),       "fn strlen(s: string) -> int"),
            ("strcmp", Some(int.clone()),       "fn strcmp(a: string, b: string) -> int"),
        ];
        for (name, ret, detail) in entries {
            self.fns.insert(name.into(), FnSig {
                params: vec![],
                ret_type: ret.clone(),
                variadic: true,
                decl_pos: None,
                home_file: None,
                is_pub: true,
            });
            self.index.decls.push(DeclInfo {
                pos: Pos::default(),
                name: name.into(),
                kind: DeclKind::Fn,
                ty: ret,
                detail: detail.into(),
                module: None,
                file: None,
                is_method: false,
            });
        }
    }

    fn register_stdlib(&mut self) {
        for (name, params, ret) in stdlib_signatures() {
            let full = format!("__glide_{}", name);
            self.fns.insert(full.clone(), FnSig {
                params: params.clone(),
                ret_type: ret.clone(),
                variadic: false,
                decl_pos: None,
                home_file: None,
                is_pub: true,
            });
            self.index.decls.push(DeclInfo {
                pos: Pos::default(),
                name: full,
                kind: DeclKind::Fn,
                ty: ret.clone(),
                detail: format_fn_detail(name, &params, ret.as_ref()),
                module: None,
                file: None,
                is_method: true,
            });
        }
    }

    fn is_chan_builtin(name: &str) -> bool {
        matches!(name, "make_chan" | "send" | "recv" | "close")
    }

    pub fn pre_register(&mut self, program: &Program) {
        for stmt in program {
            self.pre_register_stmt(stmt);
        }
    }

    fn pre_register_stmt(&mut self, stmt: &Stmt) {
        let module = self.current_module.clone();
        let file = stmt.source_file.clone().or_else(|| self.current_file.clone());
        match &stmt.kind {
            StmtKind::Struct { name, fields } => {
                self.structs.insert(
                    name.clone(),
                    StructInfo {
                        fields: fields.clone(),
                        decl_pos: stmt.pos,
                        home_file: file.clone(),
                        is_pub: stmt.is_pub,
                    },
                );
                self.index.structs.insert(name.clone(), fields.clone());
                self.index.decls.push(DeclInfo {
                    pos: stmt.pos,
                    name: name.clone(),
                    kind: DeclKind::Struct,
                    ty: None,
                    detail: format_struct_detail(name, fields),
                    module: module.clone(),
                    file: file.clone(),
                    is_method: false,
                });
                for f in fields {
                    self.index.decls.push(DeclInfo {
                        pos: f.pos,
                        name: f.name.clone(),
                        kind: DeclKind::Struct,
                        ty: Some(f.ty.clone()),
                        detail: format!("{}.{}: {}", name, f.name, type_name(&f.ty)),
                        module: module.clone(),
                        file: file.clone(),
                        is_method: false,
                    });
                }
            }
            StmtKind::Fn { name, params, ret_type, .. } => {
                self.fns.insert(name.clone(), FnSig {
                    params: params.clone(),
                    ret_type: ret_type.clone(),
                    variadic: false,
                    decl_pos: Some(stmt.pos),
                    home_file: file.clone(),
                    is_pub: stmt.is_pub,
                });
                self.index.decls.push(DeclInfo {
                    pos: stmt.pos,
                    name: name.clone(),
                    kind: DeclKind::Fn,
                    ty: ret_type.clone(),
                    detail: format_fn_detail(name, params, ret_type.as_ref()),
                    module: module.clone(),
                    file: file.clone(),
                    is_method: false,
                });
            }
            StmtKind::Enum { name, variants } => {
                self.enums.insert(
                    name.clone(),
                    EnumInfo {
                        variants: variants.clone(),
                        home_file: file.clone(),
                        is_pub: stmt.is_pub,
                    },
                );
                self.index.decls.push(DeclInfo {
                    pos: stmt.pos,
                    name: name.clone(),
                    kind: DeclKind::Struct,
                    ty: None,
                    detail: format!("enum {}", name),
                    module: module.clone(),
                    file: file.clone(),
                    is_method: false,
                });
            }
            StmtKind::Impl { target, methods, .. } => {
                let prefix = target.mangle();
                for m in methods {
                    if let StmtKind::Fn { name, params, ret_type, .. } = &m.kind {
                        let mangled = format!("{}_{}", prefix, name);
                        self.fns.insert(mangled.clone(), FnSig {
                            params: params.clone(),
                            ret_type: ret_type.clone(),
                            variadic: false,
                            decl_pos: Some(m.pos),
                            home_file: file.clone(),
                            is_pub: true,
                        });
                        self.index.decls.push(DeclInfo {
                            pos: m.pos,
                            name: mangled.clone(),
                            kind: DeclKind::Fn,
                            ty: ret_type.clone(),
                            detail: format_fn_detail(name, params, ret_type.as_ref()),
                            module: module.clone(),
                            file: file.clone(),
                            is_method: true,
                        });
                    }
                }
            }
            _ => {}
        }
    }

    pub fn check(mut self, program: Program) -> (Result<Program, Vec<TypeError>>, SymbolIndex) {
        self.pre_register(&program);

        let mut new_program = Vec::with_capacity(program.len());
        for stmt in program {
            new_program.push(self.check_top(stmt));
        }

        let result = if self.errors.is_empty() {
            Ok(new_program)
        } else {
            Err(self.errors)
        };
        (result, self.index)
    }

    fn check_top(&mut self, stmt: Stmt) -> Stmt {
        let saved_pos = self.current_pos;
        let saved_file = self.current_file.take();
        self.current_pos = stmt.pos;
        self.current_file = stmt.source_file.clone();
        let kind = match stmt.kind {
            StmtKind::Fn { name, params, ret_type, body } => {
                self.check_fn(name, params, ret_type, body)
            }
            StmtKind::Struct { name, fields } => StmtKind::Struct { name, fields },
            StmtKind::Let { name, ty, value } => self.check_let(name, ty, value),
            StmtKind::Const { name, ty, value } => self.check_const(name, ty, value),
            StmtKind::Interface { name, methods } => StmtKind::Interface { name, methods },
            StmtKind::Impl { interface, target, methods } => {
                self.check_impl(interface, target, methods)
            }
            StmtKind::Import(p) => StmtKind::Import(p),
            StmtKind::Enum { name, variants } => StmtKind::Enum { name, variants },
            StmtKind::Match { scrutinee, arms } => self.check_match(scrutinee, arms),
            other => {
                self.error("unsupported top-level statement".into());
                other
            }
        };
        self.current_pos = saved_pos;
        self.current_file = saved_file;
        Stmt { kind, pos: stmt.pos, is_pub: stmt.is_pub, source_file: stmt.source_file }
    }

    fn check_impl(
        &mut self,
        interface: Option<String>,
        target: Type,
        methods: Vec<Stmt>,
    ) -> StmtKind {
        let prefix = target.mangle();
        let mut new_methods = Vec::with_capacity(methods.len());
        for m in methods {
            let pos = m.pos;
            let m_pub = m.is_pub;
            if let StmtKind::Fn { name, params, ret_type, body } = m.kind {
                let mangled = format!("{}_{}", prefix, name);
                let new_kind = self.check_fn(mangled, params, ret_type, body);
                new_methods.push(Stmt { kind: new_kind, pos, is_pub: m_pub, source_file: None });
            } else {
                self.error("only `fn` declarations are allowed inside `impl`".into());
            }
        }
        StmtKind::Impl { interface, target, methods: new_methods }
    }

    fn check_fn(
        &mut self,
        name: String,
        params: Vec<Param>,
        ret_type: Option<Type>,
        body: Vec<Stmt>,
    ) -> StmtKind {
        self.scopes.push(HashMap::new());
        for p in &params {
            self.scopes.last_mut().unwrap().insert(
                p.name.clone(),
                LocalSlot { ty: p.ty.clone(), decl_pos: p.pos },
            );
            self.index.decls.push(DeclInfo {
                pos: p.pos,
                name: p.name.clone(),
                kind: DeclKind::Param,
                ty: Some(p.ty.clone()),
                detail: format!("{}: {}", p.name, type_name(&p.ty)),
                module: None,
                file: None,
                is_method: false,
            });
        }
        self.current_ret = ret_type.clone();

        let new_body: Vec<_> = body.into_iter().map(|s| self.check_stmt(s)).collect();

        self.current_ret = None;
        self.scopes.pop();

        StmtKind::Fn { name, params, ret_type, body: new_body }
    }

    fn check_stmt(&mut self, stmt: Stmt) -> Stmt {
        let saved_pos = self.current_pos;
        self.current_pos = stmt.pos;
        let is_pub = stmt.is_pub;
        let kind = self.check_stmt_kind(stmt.kind);
        self.current_pos = saved_pos;
        Stmt { kind, pos: stmt.pos, is_pub, source_file: stmt.source_file }
    }

    fn check_stmt_kind(&mut self, kind: StmtKind) -> StmtKind {
        match kind {
            StmtKind::Let { name, ty, value } => self.check_let(name, ty, value),
            StmtKind::Const { name, ty, value } => self.check_const(name, ty, value),

            StmtKind::Block(stmts) => {
                self.scopes.push(HashMap::new());
                let new = stmts.into_iter().map(|s| self.check_stmt(s)).collect();
                self.scopes.pop();
                StmtKind::Block(new)
            }

            StmtKind::If { cond, then_block, else_block } => {
                let (cond_new, cond_ty) = self.check_expr(cond);
                self.expect_type(&bool_ty(), &cond_ty, "if condition");
                self.scopes.push(HashMap::new());
                let then_new: Vec<_> =
                    then_block.into_iter().map(|s| self.check_stmt(s)).collect();
                self.scopes.pop();
                let else_new = else_block.map(|stmts| {
                    self.scopes.push(HashMap::new());
                    let r: Vec<_> = stmts.into_iter().map(|s| self.check_stmt(s)).collect();
                    self.scopes.pop();
                    r
                });
                StmtKind::If { cond: cond_new, then_block: then_new, else_block: else_new }
            }

            StmtKind::While { cond, body } => {
                let (cond_new, cond_ty) = self.check_expr(cond);
                self.expect_type(&bool_ty(), &cond_ty, "while condition");
                self.scopes.push(HashMap::new());
                let body_new: Vec<_> = body.into_iter().map(|s| self.check_stmt(s)).collect();
                self.scopes.pop();
                StmtKind::While { cond: cond_new, body: body_new }
            }

            StmtKind::For { init, cond, step, body } => {
                self.scopes.push(HashMap::new());
                let init_new = init.map(|s| Box::new(self.check_stmt(*s)));
                let cond_new = cond.map(|e| {
                    let (e_new, e_ty) = self.check_expr(e);
                    self.expect_type(&bool_ty(), &e_ty, "for condition");
                    e_new
                });
                let step_new = step.map(|e| self.check_expr(e).0);
                let body_new: Vec<_> = body.into_iter().map(|s| self.check_stmt(s)).collect();
                self.scopes.pop();
                StmtKind::For {
                    init: init_new,
                    cond: cond_new,
                    step: step_new,
                    body: body_new,
                }
            }

            StmtKind::Return(value) => {
                let value_new = match value {
                    Some(e) => {
                        let (e_new, e_ty) = self.check_expr(e);
                        match self.current_ret.clone() {
                            Some(ret) => self.expect_type(&ret, &e_ty, "return value"),
                            None => self.error(
                                "return with value in fn declared without a return type".into(),
                            ),
                        }
                        Some(e_new)
                    }
                    None => {
                        if let Some(ret) = self.current_ret.clone() {
                            self.error(format!(
                                "return without value in fn declared `-> {}`",
                                type_name(&ret)
                            ));
                        }
                        None
                    }
                };
                StmtKind::Return(value_new)
            }

            StmtKind::Expr(e) => {
                let (e_new, _) = self.check_expr(e);
                StmtKind::Expr(e_new)
            }

            StmtKind::Spawn(e) => {
                let (e_new, _) = self.check_expr(e);
                StmtKind::Spawn(e_new)
            }

            StmtKind::Defer(e) => {
                let (e_new, _) = self.check_expr(e);
                StmtKind::Defer(e_new)
            }

            StmtKind::Break => StmtKind::Break,
            StmtKind::Continue => StmtKind::Continue,

            StmtKind::Fn { .. } => {
                self.error("nested `fn` not supported".into());
                StmtKind::Break
            }
            StmtKind::Struct { .. } => {
                self.error("nested `struct` not supported".into());
                StmtKind::Break
            }
            StmtKind::Interface { .. } | StmtKind::Impl { .. } => {
                self.error("`interface`/`impl` only allowed at top level".into());
                StmtKind::Break
            }
            StmtKind::Import(_) => {
                self.error("`import` only allowed at top level".into());
                StmtKind::Break
            }
            StmtKind::Enum { .. } => {
                self.error("`enum` only allowed at top level".into());
                StmtKind::Break
            }
            StmtKind::Match { scrutinee, arms } => self.check_match(scrutinee, arms),
        }
    }

    fn check_match(&mut self, scrutinee: Expr, arms: Vec<MatchArm>) -> StmtKind {
        let (sc_new, sc_ty) = self.check_expr(scrutinee);
        let enum_name = match &sc_ty {
            Type::Named(n) if self.enums.contains_key(n) => Some(n.clone()),
            _ => None,
        };
        let mut new_arms = Vec::with_capacity(arms.len());
        for arm in arms {
            self.scopes.push(HashMap::new());
            let pat = self.check_pattern(arm.pattern, &sc_ty, enum_name.as_deref());
            let body: Vec<Stmt> = arm.body.into_iter().map(|s| self.check_stmt(s)).collect();
            self.scopes.pop();
            new_arms.push(MatchArm { pattern: pat, body });
        }
        StmtKind::Match { scrutinee: sc_new, arms: new_arms }
    }

    fn check_pattern(&mut self, pat: Pattern, scrut_ty: &Type, enum_hint: Option<&str>) -> Pattern {
        let pos = pat.pos;
        let kind = match pat.kind {
            PatternKind::Wildcard => PatternKind::Wildcard,
            PatternKind::Bind(name) => {
                self.declare(name.clone(), scrut_ty.clone(), pos);
                PatternKind::Bind(name)
            }
            PatternKind::Literal(e) => {
                let (e_new, e_ty) = self.check_expr(*e);
                self.expect_type(scrut_ty, &e_ty, "match literal pattern");
                PatternKind::Literal(Box::new(e_new))
            }
            PatternKind::Variant { enum_name, variant, bindings } => {
                let resolved_enum = enum_name.clone()
                    .or_else(|| enum_hint.map(|s| s.to_string()));
                let info = match resolved_enum.as_ref().and_then(|n| self.enums.get(n).cloned()) {
                    Some(i) => i,
                    None => {
                        self.error(format!("unknown enum for variant `{}`", variant));
                        return Pattern { kind: PatternKind::Wildcard, pos };
                    }
                };
                let v = match info.variants.iter().find(|v| v.name == variant) {
                    Some(v) => v.clone(),
                    None => {
                        self.error(format!(
                            "enum `{}` has no variant `{}`",
                            resolved_enum.as_deref().unwrap_or("?"), variant
                        ));
                        return Pattern { kind: PatternKind::Wildcard, pos };
                    }
                };
                if v.fields.len() != bindings.len() {
                    self.error(format!(
                        "variant `{}::{}` expects {} field(s), got {}",
                        resolved_enum.as_deref().unwrap_or("?"), variant,
                        v.fields.len(), bindings.len(),
                    ));
                }
                let mut new_bindings = Vec::with_capacity(bindings.len());
                for (i, b) in bindings.into_iter().enumerate() {
                    let field_ty = v.fields.get(i).cloned().unwrap_or_else(error_ty);
                    new_bindings.push(self.check_pattern(b, &field_ty, None));
                }
                PatternKind::Variant {
                    enum_name: resolved_enum,
                    variant,
                    bindings: new_bindings,
                }
            }
        };
        Pattern { kind, pos }
    }

    fn check_let(
        &mut self,
        name: String,
        ty: Option<Type>,
        value: Option<Expr>,
    ) -> StmtKind {
        let decl_pos = self.current_pos;
        if let (Some(annot_ty), Some(v)) = (&ty, value.as_ref()) {
            if let Some(rewritten) = self.try_make_chan_with_hint(v, annot_ty) {
                self.declare(name.clone(), annot_ty.clone(), decl_pos);
                self.record_let_decl(&name, annot_ty, decl_pos);
                return StmtKind::Let {
                    name,
                    ty,
                    value: Some(rewritten),
                };
            }
        }

        let (value_new, value_ty) = match value {
            Some(v) => {
                let (v_new, v_ty) = self.check_expr(v);
                (Some(v_new), Some(v_ty))
            }
            None => (None, None),
        };

        let final_ty = match (ty, value_ty.as_ref()) {
            (Some(t), Some(vt)) => {
                self.expect_type(&t, vt, &format!("initializer for `{}`", name));
                Some(t)
            }
            (Some(t), None) => Some(t),
            (None, Some(vt)) => Some(vt.clone()),
            (None, None) => {
                self.error(format!(
                    "`let {}`: requires either a type annotation or an initializer",
                    name
                ));
                None
            }
        };

        if let Some(t) = &final_ty {
            self.declare(name.clone(), t.clone(), decl_pos);
            self.record_let_decl(&name, t, decl_pos);
        }

        StmtKind::Let { name, ty: final_ty, value: value_new }
    }

    fn check_const(&mut self, name: String, ty: Option<Type>, value: Expr) -> StmtKind {
        let decl_pos = self.current_pos;
        if let Some(annot_ty) = &ty {
            if let Some(rewritten) = self.try_make_chan_with_hint(&value, annot_ty) {
                self.declare(name.clone(), annot_ty.clone(), decl_pos);
                self.record_const_decl(&name, annot_ty, decl_pos);
                return StmtKind::Const {
                    name,
                    ty,
                    value: rewritten,
                };
            }
        }

        let (value_new, value_ty) = self.check_expr(value);
        let final_ty = match ty {
            Some(t) => {
                self.expect_type(&t, &value_ty, &format!("initializer for const `{}`", name));
                Some(t)
            }
            None => Some(value_ty),
        };
        if let Some(t) = &final_ty {
            self.declare(name.clone(), t.clone(), decl_pos);
            self.record_const_decl(&name, t, decl_pos);
        }
        StmtKind::Const { name, ty: final_ty, value: value_new }
    }

    fn record_let_decl(&mut self, name: &str, ty: &Type, pos: Pos) {
        self.index.decls.push(DeclInfo {
            pos,
            name: name.into(),
            kind: DeclKind::Let,
            ty: Some(ty.clone()),
            detail: format!("let {}: {}", name, type_name(ty)),
            module: None,
            file: None,
            is_method: false,
        });
    }

    fn record_const_decl(&mut self, name: &str, ty: &Type, pos: Pos) {
        self.index.decls.push(DeclInfo {
            pos,
            name: name.into(),
            kind: DeclKind::Const,
            ty: Some(ty.clone()),
            detail: format!("const {}: {}", name, type_name(ty)),
            module: None,
            file: None,
            is_method: false,
        });
    }

    fn try_make_chan_with_hint(&mut self, expr: &Expr, hint: &Type) -> Option<Expr> {
        let (callee, args) = match &expr.kind {
            ExprKind::Call(c, a) => (c.as_ref(), a),
            _ => return None,
        };
        let name = match &callee.kind {
            ExprKind::Ident(n) => n,
            _ => return None,
        };
        if name != "make_chan" {
            return None;
        }
        let inner = match hint {
            Type::Chan(inner) => inner,
            _ => return None,
        };

        if args.len() != 1 {
            self.error(format!(
                "`make_chan` expects 1 argument (capacity), got {}",
                args.len()
            ));
        }
        let arg = args
            .first()
            .cloned()
            .unwrap_or_else(|| Expr::new(ExprKind::Int(0), expr.pos));
        let (arg_new, arg_ty) = self.check_expr(arg);
        self.expect_type(&int_ty(), &arg_ty, "`make_chan` capacity");

        let mangled = inner.mangle();
        let pos = expr.pos;
        let new_callee = Expr::new(
            ExprKind::Ident(format!("__glide_make_chan_{}", mangled)),
            pos,
        );
        Some(Expr::new(
            ExprKind::Call(Box::new(new_callee), vec![arg_new]),
            pos,
        ))
    }

    fn declare(&mut self, name: String, ty: Type, pos: Pos) {
        if self.scopes.is_empty() {
            self.scopes.push(HashMap::new());
        }
        self.scopes.last_mut().unwrap().insert(name, LocalSlot { ty, decl_pos: pos });
    }

    fn lookup(&self, name: &str) -> Option<LocalSlot> {
        for scope in self.scopes.iter().rev() {
            if let Some(slot) = scope.get(name) {
                return Some(slot.clone());
            }
        }
        None
    }

    fn check_expr(&mut self, expr: Expr) -> (Expr, Type) {
        let pos = expr.pos;
        let saved_pos = self.current_pos;
        self.current_pos = pos;
        let (kind, ty) = self.check_expr_kind(expr.kind, pos);
        self.current_pos = saved_pos;
        (Expr::new(kind, pos), ty)
    }

    fn check_expr_kind(&mut self, kind: ExprKind, pos: Pos) -> (ExprKind, Type) {
        match kind {
            ExprKind::Int(n)    => (ExprKind::Int(n), int_ty()),
            ExprKind::Float(f)  => (ExprKind::Float(f), float_ty()),
            ExprKind::Bool(b)   => (ExprKind::Bool(b), bool_ty()),
            ExprKind::Char(c)   => (ExprKind::Char(c), char_ty()),
            ExprKind::String(s) => (ExprKind::String(s), string_ty()),
            ExprKind::Null      => (ExprKind::Null, nullptr_ty()),

            ExprKind::Ident(name) => {
                if let Some(slot) = self.lookup(&name) {
                    self.index.refs.push(RefInfo {
                        pos,
                        name: name.clone(),
                        ty: slot.ty.clone(),
                        decl_pos: Some(slot.decl_pos),
                        kind: RefKind::Variable,
                    });
                    (ExprKind::Ident(name), slot.ty)
                } else if let Some(sig) = self.fns.get(&name).cloned() {
                    self.index.refs.push(RefInfo {
                        pos,
                        name: name.clone(),
                        ty: sig.ret_type.clone().unwrap_or_else(void_ty),
                        decl_pos: sig.decl_pos,
                        kind: RefKind::Function,
                    });
                    (ExprKind::Ident(name), error_ty())
                } else {
                    self.error(format!("unknown name `{}`", name));
                    (ExprKind::Ident(name), error_ty())
                }
            }

            ExprKind::Unary(op, inner) => {
                let (inner_new, inner_ty) = self.check_expr(*inner);
                let result_ty = match op {
                    UnaryOp::Neg | UnaryOp::BitNot => inner_ty.clone(),
                    UnaryOp::Not => bool_ty(),
                    UnaryOp::Deref => match &inner_ty {
                        Type::Pointer(t) => (**t).clone(),
                        t if is_error(t) => error_ty(),
                        _ => {
                            self.error(format!(
                                "cannot dereference non-pointer of type `{}`",
                                type_name(&inner_ty)
                            ));
                            error_ty()
                        }
                    },
                    UnaryOp::AddrOf => {
                        if !is_lvalue(&inner_new) {
                            if is_error(&inner_ty) {
                                Type::Pointer(Box::new(inner_ty.clone()))
                            } else {
                                let ptr_ty = Type::Pointer(Box::new(inner_ty.clone()));
                                return (
                                    ExprKind::AddrOfTemp {
                                        value: Box::new(inner_new),
                                        ty: inner_ty,
                                    },
                                    ptr_ty,
                                );
                            }
                        } else {
                            Type::Pointer(Box::new(inner_ty.clone()))
                        }
                    }
                    UnaryOp::PostInc | UnaryOp::PostDec => inner_ty.clone(),
                };
                (ExprKind::Unary(op, Box::new(inner_new)), result_ty)
            }

            ExprKind::Binary(op, l, r) => {
                let (l_new, l_ty) = self.check_expr(*l);
                let (r_new, r_ty) = self.check_expr(*r);
                let result_ty = self.check_binop(&op, &l_ty, &r_ty);
                (ExprKind::Binary(op, Box::new(l_new), Box::new(r_new)), result_ty)
            }

            ExprKind::Assign(lhs, op, rhs) => {
                let (lhs_new, lhs_ty) = self.check_expr(*lhs);
                if !is_lvalue(&lhs_new) {
                    self.error("invalid assignment target (must be variable, deref, index, or member)".into());
                }
                let (rhs_new, rhs_ty) = self.check_expr(*rhs);
                self.expect_type(&lhs_ty, &rhs_ty, "assignment");
                (ExprKind::Assign(Box::new(lhs_new), op, Box::new(rhs_new)), lhs_ty)
            }

            ExprKind::Call(callee, args) => {
                let (call_expr, ty) = self.check_call(*callee, args, pos);
                (call_expr.kind, ty)
            }

            ExprKind::Member(obj, field) => {
                let (mem_expr, ty) = self.check_member(*obj, field, pos);
                (mem_expr.kind, ty)
            }

            ExprKind::Index(obj, idx) => {
                let (obj_new, obj_ty) = self.check_expr(*obj);
                let (idx_new, idx_ty) = self.check_expr(*idx);
                self.expect_type(&int_ty(), &idx_ty, "array index");
                let elem_ty = match &obj_ty {
                    Type::Pointer(t) => (**t).clone(),
                    t if is_error(t) => error_ty(),
                    _ => {
                        self.error(format!(
                            "cannot index non-pointer of type `{}`",
                            type_name(&obj_ty)
                        ));
                        error_ty()
                    }
                };
                (ExprKind::Index(Box::new(obj_new), Box::new(idx_new)), elem_ty)
            }

            ExprKind::Cast(inner, target) => {
                let (inner_new, _) = self.check_expr(*inner);
                (ExprKind::Cast(Box::new(inner_new), target.clone()), target)
            }

            ExprKind::Sizeof(target) => (ExprKind::Sizeof(target), int_ty()),

            ExprKind::StructLit { type_name: tn, fields } => {
                let (lit_expr, ty) = self.check_struct_lit(tn, fields, pos);
                (lit_expr.kind, ty)
            }

            ExprKind::New { type_name: tn, fields } => {
                let (lit_expr, lit_ty) = self.check_struct_lit(tn.clone(), fields, pos);
                let new_fields = match lit_expr.kind {
                    ExprKind::StructLit { fields, .. } => fields,
                    _ => Vec::new(),
                };
                let ptr_ty = if is_error(&lit_ty) {
                    error_ty()
                } else {
                    Type::Pointer(Box::new(lit_ty))
                };
                (ExprKind::New { type_name: tn, fields: new_fields }, ptr_ty)
            }

            ExprKind::AddrOfTemp { value, ty } => {
                let ptr_ty = Type::Pointer(Box::new(ty.clone()));
                (ExprKind::AddrOfTemp { value, ty }, ptr_ty)
            }

            ExprKind::MacroCall { name, args } => {
                self.expand_macro(name, args, pos)
            }

            ExprKind::Path { ty, member } => {
                if let Some(info) = self.enums.get(&ty).cloned() {
                    if let Some(v) = info.variants.iter().find(|v| v.name == member) {
                        if !v.fields.is_empty() {
                            self.error(format!(
                                "variant `{}::{}` requires {} arg(s)",
                                ty, member, v.fields.len()
                            ));
                        }
                        return (
                            ExprKind::EnumCtor { enum_name: ty.clone(), variant: member, args: Vec::new() },
                            Type::Named(ty),
                        );
                    }
                    self.error(format!("enum `{}` has no variant `{}`", ty, member));
                    return (ExprKind::Ident(format!("{}_{}", ty, member)), error_ty());
                }
                let mangled = format!("{}_{}", ty, member);
                self.check_expr_kind(ExprKind::Ident(mangled), pos)
            }

            ExprKind::EnumCtor { enum_name, variant, args } => {
                let info = match self.enums.get(&enum_name).cloned() {
                    Some(i) => i,
                    None => {
                        self.error(format!("unknown enum `{}`", enum_name));
                        return (ExprKind::EnumCtor { enum_name, variant, args }, error_ty());
                    }
                };
                let v = match info.variants.iter().find(|v| v.name == variant) {
                    Some(v) => v.clone(),
                    None => {
                        self.error(format!("enum `{}` has no variant `{}`", enum_name, variant));
                        return (ExprKind::EnumCtor { enum_name, variant, args }, error_ty());
                    }
                };
                if v.fields.len() != args.len() {
                    self.error(format!(
                        "variant `{}::{}` expects {} arg(s), got {}",
                        enum_name, variant, v.fields.len(), args.len()
                    ));
                }
                let mut new_args = Vec::with_capacity(args.len());
                for (i, a) in args.into_iter().enumerate() {
                    let (a_new, a_ty) = self.check_expr(a);
                    if let Some(expected) = v.fields.get(i) {
                        self.expect_type(expected, &a_ty, &format!("variant `{}::{}` field {}", enum_name, variant, i));
                    }
                    new_args.push(a_new);
                }
                let result_ty = Type::Named(enum_name.clone());
                (
                    ExprKind::EnumCtor { enum_name, variant, args: new_args },
                    result_ty,
                )
            }

            ExprKind::ArrayLit { elements, .. } => {
                if elements.is_empty() {
                    self.error("empty array literal `[]` requires a type annotation".into());
                    return (
                        ExprKind::ArrayLit { elements: Vec::new(), elem_type: None },
                        error_ty(),
                    );
                }
                let mut new_elems = Vec::with_capacity(elements.len());
                let mut elem_ty: Option<Type> = None;
                for e in elements {
                    let (e_new, e_ty) = self.check_expr(e);
                    if let Some(expected) = &elem_ty {
                        self.expect_type(expected, &e_ty, "array element");
                    } else if !is_error(&e_ty) {
                        elem_ty = Some(e_ty);
                    }
                    new_elems.push(e_new);
                }
                let inner = elem_ty.clone().unwrap_or_else(error_ty);
                let result_ty = if is_error(&inner) {
                    error_ty()
                } else {
                    Type::Pointer(Box::new(inner))
                };
                (
                    ExprKind::ArrayLit { elements: new_elems, elem_type: elem_ty },
                    result_ty,
                )
            }
        }
    }

    fn check_call(&mut self, callee: Expr, args: Vec<Expr>, pos: Pos) -> (Expr, Type) {
        if let ExprKind::Member(_, _) = &callee.kind {
            return self.check_method_call(callee, args, pos);
        }

        if let ExprKind::Path { ty, member } = &callee.kind {
            if self.enums.contains_key(ty) {
                let (kind, ret_ty) = self.check_expr_kind(
                    ExprKind::EnumCtor {
                        enum_name: ty.clone(),
                        variant: member.clone(),
                        args,
                    },
                    pos,
                );
                return (Expr::new(kind, pos), ret_ty);
            }
        }

        let fn_name = match &callee.kind {
            ExprKind::Ident(n) => Some(n.clone()),
            ExprKind::Path { ty, member } => Some(format!("{}_{}", ty, member)),
            _ => None,
        };

        let callee_new = callee;

        let mut args_new = Vec::with_capacity(args.len());
        let mut arg_tys = Vec::with_capacity(args.len());
        for a in args {
            let (a_new, a_ty) = self.check_expr(a);
            args_new.push(a_new);
            arg_tys.push(a_ty);
        }

        if let Some(name) = &fn_name {
            if Self::is_chan_builtin(name) {
                return self.check_chan_builtin(name, args_new, &arg_tys, pos);
            }
        }

        let result_ty = if let Some(name) = &fn_name {
            if let Some(sig) = self.fns.get(name).cloned() {
                if !sig.is_pub && sig.home_file.is_some() && sig.home_file != self.current_file {
                    self.error(format!(
                        "`{}` is private to its module",
                        name
                    ));
                }
                if !sig.variadic {
                    if sig.params.len() != args_new.len() {
                        self.error(format!(
                            "`{}`: expected {} argument(s), got {}",
                            name,
                            sig.params.len(),
                            args_new.len()
                        ));
                    } else {
                        for (i, p) in sig.params.iter().enumerate() {
                            let saved = self.current_pos;
                            self.current_pos = args_new[i].pos;
                            self.expect_type(
                                &p.ty,
                                &arg_tys[i],
                                &format!("argument `{}` of `{}`", p.name, name),
                            );
                            self.current_pos = saved;
                        }
                    }
                }
                sig.ret_type.unwrap_or_else(void_ty)
            } else {
                self.error(format!("unknown function `{}`", name));
                error_ty()
            }
        } else {
            self.error("indirect calls (function pointers) are not supported yet".into());
            error_ty()
        };

        (
            Expr::new(ExprKind::Call(Box::new(callee_new), args_new), pos),
            result_ty,
        )
    }

    fn expand_macro(&mut self, name: String, args: Vec<Expr>, pos: Pos) -> (ExprKind, Type) {
        match name.as_str() {
            "println" => self.expand_print_macro(args, pos, true),
            "print" => self.expand_print_macro(args, pos, false),
            "format" => self.expand_format_macro(args, pos),
            "assert" => self.expand_assert_macro(args, pos),
            _ => {
                self.error(format!("unknown macro `{}!`", name));
                (ExprKind::MacroCall { name, args }, error_ty())
            }
        }
    }

    fn expand_print_macro(&mut self, args: Vec<Expr>, pos: Pos, with_newline: bool) -> (ExprKind, Type) {
        let (fmt, printf_args) = self.build_printf_format(args, with_newline);
        let mut call_args = Vec::with_capacity(printf_args.len() + 1);
        call_args.push(Expr::new(ExprKind::String(fmt), pos));
        call_args.extend(printf_args);
        let callee = Expr::new(ExprKind::Ident("printf".into()), pos);
        (ExprKind::Call(Box::new(callee), call_args), void_ty())
    }

    fn expand_format_macro(&mut self, args: Vec<Expr>, pos: Pos) -> (ExprKind, Type) {
        let (fmt, printf_args) = self.build_printf_format(args, false);
        let mut call_args = Vec::with_capacity(printf_args.len() + 1);
        call_args.push(Expr::new(ExprKind::String(fmt), pos));
        call_args.extend(printf_args);
        let callee = Expr::new(ExprKind::Ident("__glide_format".into()), pos);
        (ExprKind::Call(Box::new(callee), call_args), string_ty())
    }

    fn expand_assert_macro(&mut self, args: Vec<Expr>, pos: Pos) -> (ExprKind, Type) {
        if args.is_empty() {
            self.error("`assert!` requires at least 1 argument".into());
            return (ExprKind::MacroCall { name: "assert".into(), args }, error_ty());
        }
        let cond = args.into_iter().next().unwrap();
        let (cond_new, cond_ty) = self.check_expr(cond);
        self.expect_type(&bool_ty(), &cond_ty, "`assert!` condition");
        let msg = format!("assertion failed at {}:{}", pos.line, pos.column);
        let call = ExprKind::Call(
            Box::new(Expr::new(ExprKind::Ident("__glide_assert".into()), pos)),
            vec![cond_new, Expr::new(ExprKind::String(msg), pos)],
        );
        (call, void_ty())
    }

    fn build_printf_format(&mut self, args: Vec<Expr>, with_newline: bool) -> (String, Vec<Expr>) {
        if let Some(first) = args.first() {
            if let ExprKind::String(s) = &first.kind {
                if s.contains("{}") {
                    return self.build_template_format(s.clone(), args, with_newline);
                }
            }
        }
        let mut fmt = String::new();
        let mut printf_args: Vec<Expr> = Vec::new();
        for (i, arg) in args.into_iter().enumerate() {
            if i > 0 {
                fmt.push(' ');
            }
            let arg_pos = arg.pos;
            let (a_new, a_ty) = self.check_expr(arg);
            let (spec, transformed) = format_spec_for(&a_ty, a_new, arg_pos);
            fmt.push_str(spec);
            printf_args.push(transformed);
        }
        if with_newline {
            fmt.push('\n');
        }
        (fmt, printf_args)
    }

    fn build_template_format(
        &mut self,
        template: String,
        mut args: Vec<Expr>,
        with_newline: bool,
    ) -> (String, Vec<Expr>) {
        args.remove(0);
        let mut iter = args.into_iter();
        let mut fmt = String::new();
        let mut printf_args: Vec<Expr> = Vec::new();
        let chars: Vec<char> = template.chars().collect();
        let mut i = 0;
        while i < chars.len() {
            if i + 1 < chars.len() && chars[i] == '{' && chars[i + 1] == '}' {
                if let Some(arg) = iter.next() {
                    let arg_pos = arg.pos;
                    let (a_new, a_ty) = self.check_expr(arg);
                    let (spec, transformed) = format_spec_for(&a_ty, a_new, arg_pos);
                    fmt.push_str(spec);
                    printf_args.push(transformed);
                } else {
                    fmt.push_str("{}");
                }
                i += 2;
            } else if chars[i] == '%' {
                fmt.push_str("%%");
                i += 1;
            } else {
                fmt.push(chars[i]);
                i += 1;
            }
        }
        if with_newline {
            fmt.push('\n');
        }
        (fmt, printf_args)
    }

    fn check_method_call(&mut self, callee: Expr, args: Vec<Expr>, pos: Pos) -> (Expr, Type) {
        let ExprKind::Member(obj_box, method) = callee.kind else { unreachable!() };
        let obj = *obj_box;

        let (obj_new, obj_ty) = self.check_expr(obj);

        let mut new_args = Vec::with_capacity(args.len() + 1);
        let mut arg_tys = Vec::with_capacity(args.len() + 1);
        new_args.push(obj_new);
        arg_tys.push(obj_ty.clone());
        for a in args {
            let (a_new, a_ty) = self.check_expr(a);
            new_args.push(a_new);
            arg_tys.push(a_ty);
        }

        if matches!(&obj_ty, Type::Chan(_))
            && matches!(method.as_str(), "send" | "recv" | "close")
        {
            return self.check_chan_builtin(&method, new_args, &arg_tys, pos);
        }

        let full_prefix = obj_ty.mangle();
        let stripped_prefix = method_type_prefix(&obj_ty);

        let candidates: Vec<String> = {
            let mut v = Vec::new();
            v.push(format!("__glide_{}_{}", full_prefix, method));
            v.push(format!("{}_{}", full_prefix, method));
            if let Some(p) = &stripped_prefix {
                if p != &full_prefix {
                    v.push(format!("__glide_{}_{}", p, method));
                    v.push(format!("{}_{}", p, method));
                }
            }
            v
        };

        let resolved = candidates.iter().find_map(|n| {
            self.fns.get(n).cloned().map(|sig| (n.clone(), sig))
        }).or_else(|| {
            self.fns.get(&method).cloned().and_then(|sig| {
                if sig.variadic {
                    return None;
                }
                let first_ok = sig.params.first()
                    .map(|p| types_compat(&p.ty, &obj_ty))
                    .unwrap_or(false);
                if first_ok {
                    Some((method.clone(), sig))
                } else {
                    None
                }
            })
        });

        let (resolved_name, sig) = match resolved {
            Some(x) => x,
            None => {
                if !is_error(&obj_ty) {
                    self.error(format!(
                        "no method `{}` found for type `{}`",
                        method, type_name(&obj_ty)
                    ));
                }
                let dummy = Expr::new(ExprKind::Ident(method), pos);
                return (
                    Expr::new(ExprKind::Call(Box::new(dummy), new_args), pos),
                    error_ty(),
                );
            }
        };

        if let Some(first_param) = sig.params.first() {
            if let Type::Pointer(inner) = &first_param.ty {
                if **inner == arg_tys[0] && !matches!(arg_tys[0], Type::Pointer(_)) {
                    let obj = new_args.remove(0);
                    let obj_pos_inner = obj.pos;
                    let inner_ty = arg_tys[0].clone();
                    let wrapped = if is_lvalue(&obj) {
                        Expr::new(
                            ExprKind::Unary(UnaryOp::AddrOf, Box::new(obj)),
                            obj_pos_inner,
                        )
                    } else {
                        Expr::new(
                            ExprKind::AddrOfTemp { value: Box::new(obj), ty: inner_ty.clone() },
                            obj_pos_inner,
                        )
                    };
                    new_args.insert(0, wrapped);
                    arg_tys[0] = Type::Pointer(Box::new(inner_ty));
                }
            }
        }

        if sig.params.len() != new_args.len() {
            self.error(format!(
                "method `{}`: expected {} argument(s), got {}",
                method,
                sig.params.len().saturating_sub(1),
                new_args.len().saturating_sub(1),
            ));
        } else {
            for (i, p) in sig.params.iter().enumerate() {
                let saved = self.current_pos;
                self.current_pos = new_args[i].pos;
                self.expect_type(
                    &p.ty,
                    &arg_tys[i],
                    &format!("argument `{}` of `{}`", p.name, resolved_name),
                );
                self.current_pos = saved;
            }
        }

        let new_callee = Expr::new(ExprKind::Ident(resolved_name), pos);
        let result_ty = sig.ret_type.unwrap_or_else(void_ty);

        (
            Expr::new(ExprKind::Call(Box::new(new_callee), new_args), pos),
            result_ty,
        )
    }

    fn check_chan_builtin(
        &mut self,
        name: &str,
        args_new: Vec<Expr>,
        arg_tys: &[Type],
        pos: Pos,
    ) -> (Expr, Type) {
        let mk_call = |new_name: String, args: Vec<Expr>| {
            Expr::new(
                ExprKind::Call(
                    Box::new(Expr::new(ExprKind::Ident(new_name), pos)),
                    args,
                ),
                pos,
            )
        };

        match name {
            "make_chan" => {
                self.error(
                    "`make_chan` requires a `let` or `const` with type annotation, \
                     e.g. `let c: chan<int> = make_chan(N);`"
                        .into(),
                );
                (mk_call(name.into(), args_new), error_ty())
            }
            "send" => {
                if arg_tys.len() != 2 {
                    self.error(format!(
                        "`send` expects 2 arguments (channel, value), got {}",
                        arg_tys.len()
                    ));
                    return (mk_call(name.into(), args_new), void_ty());
                }
                let inner = match &arg_tys[0] {
                    Type::Chan(inner) => (**inner).clone(),
                    t if is_error(t) => return (mk_call(name.into(), args_new), void_ty()),
                    other => {
                        self.error(format!(
                            "first argument to `send` must be a channel, got `{}`",
                            type_name(other)
                        ));
                        return (mk_call(name.into(), args_new), void_ty());
                    }
                };
                self.expect_type(&inner, &arg_tys[1], "value sent on channel");
                let mangled = inner.mangle();
                (mk_call(format!("__glide_send_{}", mangled), args_new), void_ty())
            }
            "recv" => {
                if arg_tys.len() != 1 {
                    self.error(format!(
                        "`recv` expects 1 argument (channel), got {}",
                        arg_tys.len()
                    ));
                    return (mk_call(name.into(), args_new), error_ty());
                }
                let inner = match &arg_tys[0] {
                    Type::Chan(inner) => (**inner).clone(),
                    t if is_error(t) => return (mk_call(name.into(), args_new), error_ty()),
                    other => {
                        self.error(format!(
                            "argument to `recv` must be a channel, got `{}`",
                            type_name(other)
                        ));
                        return (mk_call(name.into(), args_new), error_ty());
                    }
                };
                let mangled = inner.mangle();
                (mk_call(format!("__glide_recv_{}", mangled), args_new), inner)
            }
            "close" => {
                if arg_tys.len() != 1 {
                    self.error(format!(
                        "`close` expects 1 argument (channel), got {}",
                        arg_tys.len()
                    ));
                    return (mk_call(name.into(), args_new), void_ty());
                }
                let inner = match &arg_tys[0] {
                    Type::Chan(inner) => (**inner).clone(),
                    t if is_error(t) => return (mk_call(name.into(), args_new), void_ty()),
                    other => {
                        self.error(format!(
                            "argument to `close` must be a channel, got `{}`",
                            type_name(other)
                        ));
                        return (mk_call(name.into(), args_new), void_ty());
                    }
                };
                let mangled = inner.mangle();
                (mk_call(format!("__glide_close_{}", mangled), args_new), void_ty())
            }
            _ => unreachable!(),
        }
    }

    fn check_member(&mut self, obj: Expr, field: String, pos: Pos) -> (Expr, Type) {
        let (obj_new, obj_ty) = self.check_expr(obj);
        let obj_pos = obj_new.pos;

        let (final_obj, struct_name) = match &obj_ty {
            Type::Named(n) => (obj_new, n.clone()),
            Type::Pointer(inner) => match inner.as_ref() {
                Type::Named(n) => {
                    let derefed = Expr::new(
                        ExprKind::Unary(UnaryOp::Deref, Box::new(obj_new)),
                        obj_pos,
                    );
                    (derefed, n.clone())
                }
                _ => {
                    self.error(format!(
                        "cannot access field `{}` on `{}`",
                        field,
                        type_name(&obj_ty)
                    ));
                    return (
                        Expr::new(ExprKind::Member(Box::new(obj_new), field), pos),
                        error_ty(),
                    );
                }
            },
            t if is_error(t) => {
                return (
                    Expr::new(ExprKind::Member(Box::new(obj_new), field), pos),
                    error_ty(),
                );
            }
            _ => {
                self.error(format!(
                    "cannot access field `{}` on `{}`",
                    field,
                    type_name(&obj_ty)
                ));
                return (
                    Expr::new(ExprKind::Member(Box::new(obj_new), field), pos),
                    error_ty(),
                );
            }
        };

        let info = match self.structs.get(&struct_name).cloned() {
            Some(info) => info,
            None => {
                self.error(format!(
                    "type `{}` has no fields",
                    struct_name
                ));
                return (
                    Expr::new(ExprKind::Member(Box::new(final_obj), field), pos),
                    error_ty(),
                );
            }
        };
        if !info.is_pub
            && info.home_file.is_some()
            && info.home_file != self.current_file
        {
            self.error(format!("struct `{}` is private to its module", struct_name));
        }

        let field_ty = match info.fields.iter().find(|f| f.name == field) {
            Some(f) => f.ty.clone(),
            None => {
                self.error(format!(
                    "struct `{}` has no field `{}`",
                    struct_name, field
                ));
                error_ty()
            }
        };

        (
            Expr::new(ExprKind::Member(Box::new(final_obj), field), pos),
            field_ty,
        )
    }

    fn check_struct_lit(
        &mut self,
        type_name: String,
        fields: Vec<(String, Expr)>,
        pos: Pos,
    ) -> (Expr, Type) {
        let struct_info = match self.structs.get(&type_name).cloned() {
            Some(info) => info,
            None => {
                self.error(format!("unknown struct `{}`", type_name));
                return (
                    Expr::new(ExprKind::StructLit { type_name, fields }, pos),
                    error_ty(),
                );
            }
        };
        if !struct_info.is_pub
            && struct_info.home_file.is_some()
            && struct_info.home_file != self.current_file
        {
            self.error(format!("struct `{}` is private to its module", type_name));
        }
        let struct_fields = struct_info.fields;

        let mut new_fields = Vec::with_capacity(fields.len());
        for (fname, fexpr) in fields {
            let (fexpr_new, fexpr_ty) = self.check_expr(fexpr);
            match struct_fields.iter().find(|f| f.name == fname) {
                Some(f) => {
                    self.expect_type(
                        &f.ty,
                        &fexpr_ty,
                        &format!("field `{}.{}`", type_name, fname),
                    );
                }
                None => self.error(format!(
                    "struct `{}` has no field `{}`",
                    type_name, fname
                )),
            }
            new_fields.push((fname, fexpr_new));
        }

        let ty = Type::Named(type_name.clone());
        (
            Expr::new(
                ExprKind::StructLit { type_name, fields: new_fields },
                pos,
            ),
            ty,
        )
    }

    fn check_binop(&mut self, op: &BinaryOp, l: &Type, r: &Type) -> Type {
        match op {
            BinaryOp::Add | BinaryOp::Sub | BinaryOp::Mul | BinaryOp::Div | BinaryOp::Mod
            | BinaryOp::BitAnd | BinaryOp::BitOr | BinaryOp::BitXor
            | BinaryOp::Shl | BinaryOp::Shr => {
                if !is_error(l) && !is_error(r) && l != r {
                    self.error(format!(
                        "operator `{}` requires matching operand types, got `{}` and `{}`",
                        binop_str(op),
                        type_name(l),
                        type_name(r)
                    ));
                }
                if is_error(l) { r.clone() } else { l.clone() }
            }
            BinaryOp::Eq | BinaryOp::NotEq | BinaryOp::Lt | BinaryOp::LtEq
            | BinaryOp::Gt | BinaryOp::GtEq => bool_ty(),
            BinaryOp::And | BinaryOp::Or => {
                self.expect_type(&bool_ty(), l, "logical operand");
                self.expect_type(&bool_ty(), r, "logical operand");
                bool_ty()
            }
        }
    }

    fn expect_type(&mut self, expected: &Type, actual: &Type, ctx: &str) {
        if !types_compat(expected, actual) {
            self.error(format!(
                "{}: expected `{}`, got `{}`",
                ctx,
                type_name(expected),
                type_name(actual)
            ));
        }
    }

    fn error(&mut self, msg: String) {
        self.errors.push(TypeError {
            message: msg,
            line: self.current_pos.line,
            column: self.current_pos.column,
        });
    }
}

fn int_ty() -> Type { Type::Named("int".into()) }
fn float_ty() -> Type { Type::Named("float".into()) }
fn bool_ty() -> Type { Type::Named("bool".into()) }
fn char_ty() -> Type { Type::Named("char".into()) }
fn string_ty() -> Type { Type::Named("string".into()) }
fn void_ty() -> Type { Type::Named("void".into()) }
fn error_ty() -> Type { Type::Named("__error__".into()) }
fn nullptr_ty() -> Type {
    Type::Pointer(Box::new(Type::Named("__null__".into())))
}

fn is_error(t: &Type) -> bool {
    matches!(t, Type::Named(n) if n == "__error__")
}

fn is_null_ptr(t: &Type) -> bool {
    matches!(
        t,
        Type::Pointer(inner) if matches!(inner.as_ref(), Type::Named(n) if n == "__null__")
    )
}

fn is_void_ptr(t: &Type) -> bool {
    matches!(
        t,
        Type::Pointer(inner) if matches!(inner.as_ref(), Type::Named(n) if n == "void")
    )
}

fn types_compat(expected: &Type, actual: &Type) -> bool {
    if is_error(expected) || is_error(actual) {
        return true;
    }
    if expected == actual {
        return true;
    }
    if is_null_ptr(actual) && matches!(expected, Type::Pointer(_)) {
        return true;
    }
    if is_null_ptr(expected) && matches!(actual, Type::Pointer(_)) {
        return true;
    }
    if is_void_ptr(actual) && matches!(expected, Type::Pointer(_)) {
        return true;
    }
    if is_void_ptr(expected) && matches!(actual, Type::Pointer(_)) {
        return true;
    }
    false
}

pub fn type_name(t: &Type) -> String {
    match t {
        Type::Named(n) => n.clone(),
        Type::Pointer(inner) => format!("*{}", type_name(inner)),
        Type::Chan(inner) => format!("chan<{}>", type_name(inner)),
    }
}

fn format_fn_detail(name: &str, params: &[Param], ret_type: Option<&Type>) -> String {
    let mut out = format!("fn {}(", name);
    for (i, p) in params.iter().enumerate() {
        if i > 0 { out.push_str(", "); }
        out.push_str(&p.name);
        out.push_str(": ");
        out.push_str(&type_name(&p.ty));
    }
    out.push(')');
    if let Some(t) = ret_type {
        out.push_str(" -> ");
        out.push_str(&type_name(t));
    }
    out
}

fn format_struct_detail(name: &str, fields: &[Field]) -> String {
    let mut out = format!("struct {} {{", name);
    if !fields.is_empty() {
        out.push(' ');
        for (i, f) in fields.iter().enumerate() {
            if i > 0 { out.push_str(", "); }
            out.push_str(&f.name);
            out.push_str(": ");
            out.push_str(&type_name(&f.ty));
        }
        out.push(' ');
    }
    out.push('}');
    out
}

fn is_lvalue(e: &Expr) -> bool {
    matches!(
        &e.kind,
        ExprKind::Ident(_)
            | ExprKind::Unary(UnaryOp::Deref, _)
            | ExprKind::Index(_, _)
            | ExprKind::Member(_, _)
    )
}

fn binop_str(op: &BinaryOp) -> &'static str {
    match op {
        BinaryOp::Add => "+", BinaryOp::Sub => "-", BinaryOp::Mul => "*",
        BinaryOp::Div => "/", BinaryOp::Mod => "%",
        BinaryOp::Eq => "==", BinaryOp::NotEq => "!=",
        BinaryOp::Lt => "<", BinaryOp::LtEq => "<=",
        BinaryOp::Gt => ">", BinaryOp::GtEq => ">=",
        BinaryOp::And => "&&", BinaryOp::Or => "||",
        BinaryOp::BitAnd => "&", BinaryOp::BitOr => "|", BinaryOp::BitXor => "^",
        BinaryOp::Shl => "<<", BinaryOp::Shr => ">>",
    }
}

fn synth_param(name: &str, ty: Type) -> Param {
    Param { name: name.into(), ty, pos: Pos::default() }
}

fn is_always_visible(kind: &StmtKind) -> bool {
    matches!(kind, StmtKind::Impl { .. })
}

fn format_spec_for(ty: &Type, arg: Expr, pos: Pos) -> (&'static str, Expr) {
    match ty {
        Type::Named(n) => match n.as_str() {
            "int"    => ("%d", arg),
            "float"  => ("%g", arg),
            "char"   => ("%c", arg),
            "string" => ("%s", arg),
            "bool"   => {
                let callee = Expr::new(
                    ExprKind::Ident("__glide_bool_to_string".into()),
                    pos,
                );
                ("%s", Expr::new(ExprKind::Call(Box::new(callee), vec![arg]), pos))
            }
            _ => ("%p", arg),
        },
        Type::Pointer(_) | Type::Chan(_) => ("%p", arg),
    }
}

fn method_type_prefix(ty: &Type) -> Option<String> {
    match ty {
        Type::Named(n) if !is_error(ty) => Some(n.clone()),
        Type::Pointer(inner) => method_type_prefix(inner),
        Type::Chan(_) => Some("chan".into()),
        _ => None,
    }
}

pub fn stdlib_signatures() -> Vec<(&'static str, Vec<Param>, Option<Type>)> {
    vec![
        ("int_to_string",   vec![synth_param("n", int_ty())],   Some(string_ty())),
        ("int_to_float",    vec![synth_param("n", int_ty())],   Some(float_ty())),
        ("int_abs",         vec![synth_param("n", int_ty())],   Some(int_ty())),

        ("float_to_string", vec![synth_param("f", float_ty())], Some(string_ty())),
        ("float_to_int",    vec![synth_param("f", float_ty())], Some(int_ty())),
        ("float_floor",     vec![synth_param("f", float_ty())], Some(float_ty())),
        ("float_ceil",      vec![synth_param("f", float_ty())], Some(float_ty())),

        ("string_len",      vec![synth_param("s", string_ty())], Some(int_ty())),
        ("string_eq",       vec![synth_param("a", string_ty()), synth_param("b", string_ty())], Some(bool_ty())),
        ("string_at",       vec![synth_param("s", string_ty()), synth_param("i", int_ty())],     Some(char_ty())),
        ("string_concat",   vec![synth_param("a", string_ty()), synth_param("b", string_ty())], Some(string_ty())),

        ("bool_to_string",  vec![synth_param("b", bool_ty())],  Some(string_ty())),

        ("char_to_int",     vec![synth_param("c", char_ty())], Some(int_ty())),
        ("char_is_digit",   vec![synth_param("c", char_ty())], Some(bool_ty())),
        ("char_is_alpha",   vec![synth_param("c", char_ty())], Some(bool_ty())),
    ]
}

pub fn is_stdlib_fn(name: &str) -> bool {
    stdlib_signatures().iter().any(|(n, _, _)| *n == name)
}
