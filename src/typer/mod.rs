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
    is_owned: bool,
    moved_at: Option<Pos>,
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

#[derive(Clone)]
struct GenericFnInfo {
    type_params: Vec<String>,
    params: Vec<Param>,
    ret_type: Option<Type>,
    body: Vec<Stmt>,
    decl_pos: Pos,
}

#[derive(Clone)]
struct GenericStructInfo {
    type_params: Vec<String>,
    fields: Vec<Field>,
    decl_pos: Pos,
}

pub struct Typer {
    structs: HashMap<String, StructInfo>,
    enums: HashMap<String, EnumInfo>,
    fns: HashMap<String, FnSig>,
    generic_fns: HashMap<String, GenericFnInfo>,
    generic_structs: HashMap<String, GenericStructInfo>,
    mono_queue: Vec<(String, Vec<Type>)>,
    struct_mono_queue: Vec<(String, Vec<Type>)>,
    mono_done: std::collections::HashSet<String>,
    type_aliases: HashMap<String, Type>,
    scopes: Vec<HashMap<String, LocalSlot>>,
    current_ret: Option<Type>,
    current_pos: Pos,
    errors: Vec<TypeError>,
    index: SymbolIndex,
    current_module: Option<String>,
    current_file: Option<std::path::PathBuf>,
    synth_stmts: Vec<Stmt>,
    anon_counter: usize,
    closure_struct_names: std::collections::HashSet<String>,
    type_param_scope: std::collections::HashSet<String>,
    expected_ret_hint: Option<Type>,
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
            generic_fns: HashMap::new(),
            generic_structs: HashMap::new(),
            mono_queue: Vec::new(),
            struct_mono_queue: Vec::new(),
            mono_done: std::collections::HashSet::new(),
            type_aliases: HashMap::new(),
            scopes: Vec::new(),
            current_ret: None,
            current_pos: Pos::default(),
            errors: Vec::new(),
            index: SymbolIndex::default(),
            current_module: None,
            current_file: None,
            synth_stmts: Vec::new(),
            anon_counter: 0,
            closure_struct_names: std::collections::HashSet::new(),
            type_param_scope: std::collections::HashSet::new(),
            expected_ret_hint: None,
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
            ("Arena_new",   vec![synth_param("cap", int_ty())],                                                Some(arena_ptr()), "fn Arena::new(cap: int) -> *Arena"),
            ("Arena_alloc", vec![synth_param("self", arena_ptr()), synth_param("size", int_ty())],             Some(Type::Pointer(Box::new(void_ty()))), "fn Arena.alloc(size: int) -> *void"),
            ("Arena_free",  vec![synth_param("self", arena_ptr())],                                            None,              "fn Arena.free()"),
            ("Arena_used",  vec![synth_param("self", arena_ptr())],                                            Some(int_ty()),    "fn Arena.used() -> int"),
            ("Arena_reset", vec![synth_param("self", arena_ptr())],                                            None,              "fn Arena.reset()"),
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
            StmtKind::Struct { name, type_params, fields } => {
                if !type_params.is_empty() {
                    self.generic_structs.insert(name.clone(), GenericStructInfo {
                        type_params: type_params.clone(),
                        fields: fields.clone(),
                        decl_pos: stmt.pos,
                    });
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
                    return;
                }
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
            StmtKind::Fn { name, type_params, params, ret_type, body } => {
                if !type_params.is_empty() {
                    self.generic_fns.insert(name.clone(), GenericFnInfo {
                        type_params: type_params.clone(),
                        params: params.clone(),
                        ret_type: ret_type.clone(),
                        body: body.clone(),
                        decl_pos: stmt.pos,
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
                    return;
                }
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
            StmtKind::TypeAlias { name, ty } => {
                self.type_aliases.insert(name.clone(), ty.clone());
                self.index.decls.push(DeclInfo {
                    pos: stmt.pos,
                    name: name.clone(),
                    kind: DeclKind::Struct,
                    ty: Some(ty.clone()),
                    detail: format!("type {} {}", name, type_name(ty)),
                    module: module.clone(),
                    file: file.clone(),
                    is_method: false,
                });
            }
            StmtKind::ExternFn { name, params, ret_type, variadic } => {
                self.fns.insert(name.clone(), FnSig {
                    params: params.clone(),
                    ret_type: ret_type.clone(),
                    variadic: *variadic,
                    decl_pos: Some(stmt.pos),
                    home_file: file.clone(),
                    is_pub: true,
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
            StmtKind::ExternType { name, .. } => {
                self.structs.insert(
                    name.clone(),
                    StructInfo {
                        fields: Vec::new(),
                        decl_pos: stmt.pos,
                        home_file: file.clone(),
                        is_pub: true,
                    },
                );
                self.index.structs.insert(name.clone(), Vec::new());
                self.index.decls.push(DeclInfo {
                    pos: stmt.pos,
                    name: name.clone(),
                    kind: DeclKind::Struct,
                    ty: None,
                    detail: format!("extern type {}", name),
                    module: module.clone(),
                    file: file.clone(),
                    is_method: false,
                });
            }
            _ => {}
        }
    }

    /// Walks a type tree, replacing any Type::Generic whose name is a known
    /// generic struct with Type::Named(mangled). Side-effect: queues additional
    /// monos onto self.struct_mono_queue.
    fn finalize_generic(&mut self, t: &Type) -> Type {
        match t {
            Type::Generic { name, args } => {
                let new_args: Vec<Type> = args.iter().map(|a| self.finalize_generic(a)).collect();
                if self.generic_structs.contains_key(name) {
                    let mangled = mono_mangle(name, &new_args);
                    if !self.mono_done.contains(&mangled) {
                        self.mono_done.insert(mangled.clone());
                        self.struct_mono_queue.push((name.clone(), new_args));
                    }
                    return Type::Named(mangled);
                }
                Type::Generic { name: name.clone(), args: new_args }
            }
            Type::Pointer(inner) => Type::Pointer(Box::new(self.finalize_generic(inner))),
            Type::Borrow(inner) => Type::Borrow(Box::new(self.finalize_generic(inner))),
            Type::BorrowMut(inner) => Type::BorrowMut(Box::new(self.finalize_generic(inner))),
            Type::Chan(inner) => Type::Chan(Box::new(self.finalize_generic(inner))),
            Type::Slice(inner) => Type::Slice(Box::new(self.finalize_generic(inner))),
            Type::FnPtr { params, ret } => Type::FnPtr {
                params: params.iter().map(|p| self.finalize_generic(p)).collect(),
                ret: ret.as_ref().map(|r| Box::new(self.finalize_generic(r))),
            },
            Type::Named(_) => t.clone(),
        }
    }

    fn collect_generic_uses_in_type(&mut self, t: &Type) {
        match t {
            Type::Generic { name, args } => {
                for a in args {
                    self.collect_generic_uses_in_type(a);
                }
                if self.generic_structs.contains_key(name) {
                    let mangled = mono_mangle(name, args);
                    if !self.mono_done.contains(&mangled) {
                        self.mono_done.insert(mangled);
                        self.struct_mono_queue.push((name.clone(), args.clone()));
                    }
                }
            }
            Type::Pointer(inner)
            | Type::Borrow(inner)
            | Type::BorrowMut(inner)
            | Type::Chan(inner)
            | Type::Slice(inner) => self.collect_generic_uses_in_type(inner),
            Type::FnPtr { params, ret } => {
                for p in params { self.collect_generic_uses_in_type(p); }
                if let Some(r) = ret { self.collect_generic_uses_in_type(r); }
            }
            Type::Named(_) => {}
        }
    }

    fn collect_generic_uses_in_expr(&mut self, e: &Expr) {
        match &e.kind {
            ExprKind::Cast(inner, t) => {
                self.collect_generic_uses_in_expr(inner);
                self.collect_generic_uses_in_type(t);
            }
            ExprKind::Sizeof(t) => self.collect_generic_uses_in_type(t),
            ExprKind::Unary(_, inner) => self.collect_generic_uses_in_expr(inner),
            ExprKind::Binary(_, l, r) => {
                self.collect_generic_uses_in_expr(l);
                self.collect_generic_uses_in_expr(r);
            }
            ExprKind::Assign(l, _, r) => {
                self.collect_generic_uses_in_expr(l);
                self.collect_generic_uses_in_expr(r);
            }
            ExprKind::Call(c, args) => {
                self.collect_generic_uses_in_expr(c);
                for a in args { self.collect_generic_uses_in_expr(a); }
            }
            ExprKind::Index(a, b) => {
                self.collect_generic_uses_in_expr(a);
                self.collect_generic_uses_in_expr(b);
            }
            ExprKind::Member(a, _) => self.collect_generic_uses_in_expr(a),
            ExprKind::StructLit { fields, .. }
            | ExprKind::New { fields, .. } => {
                for (_, v) in fields { self.collect_generic_uses_in_expr(v); }
            }
            ExprKind::ArrayLit { elements, elem_type, .. } => {
                for e in elements { self.collect_generic_uses_in_expr(e); }
                if let Some(t) = elem_type { self.collect_generic_uses_in_type(t); }
            }
            ExprKind::AddrOfTemp { value, ty } => {
                self.collect_generic_uses_in_expr(value);
                self.collect_generic_uses_in_type(ty);
            }
            ExprKind::MacroCall { args, .. } => {
                for a in args { self.collect_generic_uses_in_expr(a); }
            }
            ExprKind::EnumCtor { args, .. } => {
                for a in args { self.collect_generic_uses_in_expr(a); }
            }
            ExprKind::FnExpr { params, ret_type, body, .. } => {
                for p in params { self.collect_generic_uses_in_type(&p.ty); }
                if let Some(t) = ret_type { self.collect_generic_uses_in_type(t); }
                for s in body { self.collect_generic_uses_in_stmt(s); }
            }
            _ => {}
        }
    }

    fn collect_generic_uses_in_stmt(&mut self, stmt: &Stmt) {
        match &stmt.kind {
            StmtKind::Let { ty, value, .. } => {
                if let Some(t) = ty { self.collect_generic_uses_in_type(t); }
                if let Some(v) = value { self.collect_generic_uses_in_expr(v); }
            }
            StmtKind::Const { ty, value, .. } => {
                if let Some(t) = ty { self.collect_generic_uses_in_type(t); }
                self.collect_generic_uses_in_expr(value);
            }
            StmtKind::Fn { type_params, params, ret_type, body, .. } => {
                if !type_params.is_empty() { return; }
                for p in params { self.collect_generic_uses_in_type(&p.ty); }
                if let Some(t) = ret_type { self.collect_generic_uses_in_type(t); }
                for s in body { self.collect_generic_uses_in_stmt(s); }
            }
            StmtKind::Struct { type_params, fields, .. } => {
                if !type_params.is_empty() { return; }
                for f in fields { self.collect_generic_uses_in_type(&f.ty); }
            }
            StmtKind::Block(stmts) => for s in stmts { self.collect_generic_uses_in_stmt(s); },
            StmtKind::If { cond, then_block, else_block } => {
                self.collect_generic_uses_in_expr(cond);
                for s in then_block { self.collect_generic_uses_in_stmt(s); }
                if let Some(b) = else_block { for s in b { self.collect_generic_uses_in_stmt(s); } }
            }
            StmtKind::While { cond, body } => {
                self.collect_generic_uses_in_expr(cond);
                for s in body { self.collect_generic_uses_in_stmt(s); }
            }
            StmtKind::For { init, cond, step, body } => {
                if let Some(s) = init { self.collect_generic_uses_in_stmt(s); }
                if let Some(c) = cond { self.collect_generic_uses_in_expr(c); }
                if let Some(s) = step { self.collect_generic_uses_in_expr(s); }
                for s in body { self.collect_generic_uses_in_stmt(s); }
            }
            StmtKind::Return(v) => if let Some(e) = v { self.collect_generic_uses_in_expr(e); },
            StmtKind::Expr(e) | StmtKind::Spawn(e) | StmtKind::Defer(e) => {
                self.collect_generic_uses_in_expr(e);
            }
            StmtKind::Match { scrutinee, arms } => {
                self.collect_generic_uses_in_expr(scrutinee);
                for a in arms {
                    for s in &a.body { self.collect_generic_uses_in_stmt(s); }
                }
            }
            StmtKind::Impl { methods, .. } => for m in methods { self.collect_generic_uses_in_stmt(m); },
            StmtKind::ExternFn { params, ret_type, .. } => {
                for p in params { self.collect_generic_uses_in_type(&p.ty); }
                if let Some(t) = ret_type { self.collect_generic_uses_in_type(t); }
            }
            _ => {}
        }
    }

    fn resolve_alias(&self, t: &Type) -> Type {
        match t {
            Type::Named(n) => {
                if let Some(target) = self.type_aliases.get(n) {
                    return self.resolve_alias(target);
                }
                t.clone()
            }
            Type::Generic { .. } => Type::Named(t.mangle()),
            Type::Pointer(inner) => Type::Pointer(Box::new(self.resolve_alias(inner))),
            Type::Borrow(inner) => Type::Borrow(Box::new(self.resolve_alias(inner))),
            Type::BorrowMut(inner) => Type::BorrowMut(Box::new(self.resolve_alias(inner))),
            Type::Chan(inner) => Type::Chan(Box::new(self.resolve_alias(inner))),
            Type::Slice(inner) => Type::Slice(Box::new(self.resolve_alias(inner))),
            Type::FnPtr { params, ret } => Type::FnPtr {
                params: params.iter().map(|p| self.resolve_alias(p)).collect(),
                ret: ret.as_ref().map(|r| Box::new(self.resolve_alias(r))),
            },
        }
    }

    pub fn check(mut self, program: Program) -> (Result<Program, Vec<TypeError>>, SymbolIndex) {
        self.pre_register(&program);

        // Collect all generic struct uses across the program, queue monomorphizations.
        for stmt in &program {
            self.collect_generic_uses_in_stmt(stmt);
        }

        // Drain struct monomorphization queue eagerly so subsequent type checks
        // see the resulting concrete struct in self.structs. Substituted field
        // types may themselves be generic; finalize_type converts those to
        // Named(mangled) and queues additional monos as a side effect.
        let mut struct_monos: Vec<Stmt> = Vec::new();
        while let Some((orig_name, type_args)) = self.struct_mono_queue.pop() {
            if let Some(g) = self.generic_structs.get(&orig_name).cloned() {
                let mangled = mono_mangle(&orig_name, &type_args);
                let subst: HashMap<String, Type> = g.type_params.iter()
                    .cloned()
                    .zip(type_args.iter().cloned())
                    .collect();
                let new_fields: Vec<Field> = g.fields.iter().map(|f| Field {
                    name: f.name.clone(),
                    ty: self.finalize_generic(&subst_type(&f.ty, &subst)),
                    pos: f.pos,
                }).collect();
                self.structs.insert(mangled.clone(), StructInfo {
                    fields: new_fields.clone(),
                    decl_pos: g.decl_pos,
                    home_file: None,
                    is_pub: true,
                });
                self.index.structs.insert(mangled.clone(), new_fields.clone());
                struct_monos.push(Stmt {
                    kind: StmtKind::Struct {
                        name: mangled,
                        type_params: Vec::new(),
                        fields: new_fields,
                    },
                    pos: g.decl_pos,
                    is_pub: false,
                    source_file: None,
                });
            }
        }

        let mut new_program = Vec::with_capacity(program.len() + struct_monos.len());
        // Insert struct monos first so they appear before user code in C output.
        // Reverse so that dependencies (mono'd first via finalize_generic) come
        // before their users (who triggered the recursive mono).
        struct_monos.reverse();
        for s in struct_monos { new_program.push(s); }
        for stmt in program {
            new_program.push(self.check_top(stmt));
        }

        // Re-check any closures lifted to top-level during the main pass.
        let mut synth = std::mem::take(&mut self.synth_stmts);
        while !synth.is_empty() {
            let mut next_round = Vec::new();
            for stmt in synth {
                let checked = self.check_top(stmt);
                next_round.push(checked);
            }
            // Place synth stmts BEFORE the user code, so they're declared in C.
            for s in next_round {
                new_program.insert(0, s);
            }
            synth = std::mem::take(&mut self.synth_stmts);
        }

        // Drain monomorphization queue. Each instantiation may itself trigger
        // new generic calls, which append to the queue — keep going until empty.
        while let Some((orig_name, type_args)) = self.mono_queue.pop() {
            if let Some(g) = self.generic_fns.get(&orig_name).cloned() {
                let mangled = mono_mangle(&orig_name, &type_args);
                let subst: HashMap<String, Type> = g.type_params.iter()
                    .cloned()
                    .zip(type_args.iter().cloned())
                    .collect();
                let new_params: Vec<Param> = g.params.iter().map(|p| Param {
                    name: p.name.clone(),
                    ty: subst_type(&p.ty, &subst),
                    pos: p.pos,
                }).collect();
                let new_ret = g.ret_type.as_ref().map(|t| subst_type(t, &subst));
                let new_body: Vec<Stmt> = g.body.iter()
                    .cloned()
                    .map(|s| subst_stmt(s, &subst))
                    .collect();
                self.fns.insert(mangled.clone(), FnSig {
                    params: new_params.clone(),
                    ret_type: new_ret.clone(),
                    variadic: false,
                    decl_pos: Some(g.decl_pos),
                    home_file: None,
                    is_pub: true,
                });
                let stmt = Stmt {
                    kind: StmtKind::Fn {
                        name: mangled,
                        type_params: Vec::new(),
                        params: new_params,
                        ret_type: new_ret,
                        body: new_body,
                    },
                    pos: g.decl_pos,
                    is_pub: false,
                    source_file: None,
                };
                let checked = self.check_top(stmt);
                new_program.insert(0, checked);
            }
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
            StmtKind::Fn { name, type_params, params, ret_type, body } => {
                self.check_fn(name, type_params, params, ret_type, body)
            }
            StmtKind::Struct { name, type_params, fields } => {
                for f in &fields {
                    if matches!(self.resolve_alias(&f.ty), Type::Borrow(_) | Type::BorrowMut(_)) {
                        let saved = self.current_pos;
                        self.current_pos = f.pos;
                        self.error(format!(
                            "field `{}.{}`: borrow types (`&T`, `&mut T`) cannot be stored in struct fields (use `*T` for raw pointer)",
                            name, f.name
                        ));
                        self.current_pos = saved;
                    }
                }
                StmtKind::Struct { name, type_params, fields }
            }
            StmtKind::Let { name, ty, value, is_mut, .. } => self.check_let(name, ty, value, is_mut),
            StmtKind::Const { name, ty, value } => self.check_const(name, ty, value),
            StmtKind::Interface { name, methods } => StmtKind::Interface { name, methods },
            StmtKind::Impl { interface, target, methods } => {
                self.check_impl(interface, target, methods)
            }
            StmtKind::Import(p) => StmtKind::Import(p),
            StmtKind::Enum { name, variants } => StmtKind::Enum { name, variants },
            StmtKind::Match { scrutinee, arms } => self.check_match(scrutinee, arms),
            StmtKind::TypeAlias { name, ty } => StmtKind::TypeAlias { name, ty },
            StmtKind::ExternFn { name, params, ret_type, variadic } => {
                StmtKind::ExternFn { name, params, ret_type, variadic }
            }
            StmtKind::ExternType { name, c_repr } => {
                StmtKind::ExternType { name, c_repr }
            }
            StmtKind::CInclude(p) => StmtKind::CInclude(p),
            StmtKind::CLink(p) => StmtKind::CLink(p),
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
            if let StmtKind::Fn { name, type_params, params, ret_type, body } = m.kind {
                let mangled = format!("{}_{}", prefix, name);
                let new_kind = self.check_fn(mangled, type_params, params, ret_type, body);
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
        type_params: Vec<String>,
        params: Vec<Param>,
        ret_type: Option<Type>,
        body: Vec<Stmt>,
    ) -> StmtKind {
        // Generic fns: defer body checking. Each monomorphization will be
        // checked separately after substitution. Pass through unchanged.
        if !type_params.is_empty() {
            return StmtKind::Fn { name, type_params, params, ret_type, body };
        }

        self.scopes.push(HashMap::new());
        for p in &params {
            self.scopes.last_mut().unwrap().insert(
                p.name.clone(),
                LocalSlot { ty: p.ty.clone(), decl_pos: p.pos, is_owned: false, moved_at: None },
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

        StmtKind::Fn { name, type_params, params, ret_type, body: new_body }
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
            StmtKind::Let { name, ty, value, is_mut, .. } => self.check_let(name, ty, value, is_mut),
            StmtKind::Const { name, ty, value } => self.check_const(name, ty, value),
            StmtKind::TypeAlias { name, ty } => StmtKind::TypeAlias { name, ty },

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
                        // Reject returning an owned local: would conflict with auto-drop.
                        if let ExprKind::Ident(n) = &e.kind {
                            if let Some(slot) = self.lookup(n) {
                                if slot.is_owned {
                                    self.error(format!(
                                        "cannot return owned value `{}` (auto-drop would free it)",
                                        n
                                    ));
                                }
                            }
                        }
                        // Reject return of borrow expressions where the source is a local
                        // (escape rule, except pass-through of borrow params).
                        if let ExprKind::Unary(op, inner) = &e.kind {
                            if matches!(op, UnaryOp::AddrOf | UnaryOp::AddrOfMut) {
                                if let ExprKind::Ident(n) = &inner.kind {
                                    if let Some(slot) = self.lookup(n) {
                                        let is_param_borrow = matches!(
                                            self.resolve_alias(&slot.ty),
                                            Type::Borrow(_) | Type::BorrowMut(_)
                                        );
                                        if !is_param_borrow {
                                            self.error(format!(
                                                "cannot return borrow of local `{}` (would dangle after function returns)",
                                                n
                                            ));
                                        }
                                    }
                                }
                            }
                        }
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
            StmtKind::ExternFn { .. } => {
                self.error("`extern fn` only allowed at top level".into());
                StmtKind::Break
            }
            StmtKind::ExternType { .. } => {
                self.error("`extern type` only allowed at top level".into());
                StmtKind::Break
            }
            StmtKind::CInclude(_) | StmtKind::CLink(_) => {
                self.error("`c_include` / `c_link` only allowed at top level".into());
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
        is_mut: bool,
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
                    is_mut,
                    is_owned: false,
                };
            }
        }

        // Slice literal: `let s: []T = [a, b, c];` flips ArrayLit's as_slice flag.
        let mut value = value;
        if let (Some(annot_ty), Some(v)) = (&ty, value.as_mut()) {
            if let Type::Slice(_) = self.resolve_alias(annot_ty) {
                if let ExprKind::ArrayLit { as_slice, .. } = &mut v.kind {
                    *as_slice = true;
                }
            }
        }

        // Generic struct literal: `let v: Vec<int> = Vec { ... };` rewrites the
        // StructLit's `type_name` to the mangled mono name.
        if let (Some(annot_ty), Some(v)) = (&ty, value.as_mut()) {
            if let Type::Generic { name: gname, args } = annot_ty {
                let mangled = mono_mangle(gname, args);
                match &mut v.kind {
                    ExprKind::StructLit { type_name, .. } if type_name == gname => {
                        *type_name = mangled.clone();
                    }
                    ExprKind::New { type_name, .. } if type_name == gname => {
                        *type_name = mangled.clone();
                    }
                    _ => {}
                }
            }
        }

        // Auto-allocation pattern: `let p: *T = T { ... };` rewrites to New (heap alloc)
        // and marks the slot as owned (auto-drop at scope end).
        let mut auto_owned_value: Option<Expr> = None;
        if let (Some(annot_ty), Some(v)) = (&ty, value.as_ref()) {
            if let (Type::Pointer(inner), ExprKind::StructLit { type_name, fields }) =
                (self.resolve_alias(annot_ty), &v.kind)
            {
                if let Type::Named(want_name) = inner.as_ref() {
                    if want_name == type_name {
                        let new_expr = Expr::new(
                            ExprKind::New {
                                type_name: type_name.clone(),
                                fields: fields.clone(),
                            },
                            v.pos,
                        );
                        auto_owned_value = Some(new_expr);
                    }
                }
            }
        }

        let was_auto_owned = auto_owned_value.is_some();

        let (value_new, value_ty) = if let Some(rewritten) = auto_owned_value {
            let (v_new, v_ty) = self.check_expr(rewritten);
            (Some(v_new), Some(v_ty))
        } else {
            match value {
                Some(v) => {
                    // Reject moving an owned value (would double-free).
                    // Raw *T (from arena, fn calls, casts) can be freely copied.
                    if let ExprKind::Ident(src_name) = &v.kind {
                        if let Some(slot) = self.lookup(src_name) {
                            if slot.is_owned {
                                self.error(format!(
                                    "cannot move owned value `{}` into another binding (auto-drop conflict)",
                                    src_name
                                ));
                            }
                        }
                    }
                    let saved_hint = self.expected_ret_hint.take();
                    self.expected_ret_hint = ty.clone();
                    let (v_new, v_ty) = self.check_expr(v);
                    self.expected_ret_hint = saved_hint;
                    (Some(v_new), Some(v_ty))
                }
                None => (None, None),
            }
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
            if was_auto_owned {
                self.declare_owned(name.clone(), t.clone(), decl_pos);
            } else {
                self.declare(name.clone(), t.clone(), decl_pos);
            }
            self.record_let_decl(&name, t, decl_pos);
        }

        StmtKind::Let {
            name,
            ty: final_ty,
            value: value_new,
            is_mut,
            is_owned: was_auto_owned,
        }
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
        self.scopes.last_mut().unwrap().insert(
            name,
            LocalSlot { ty, decl_pos: pos, is_owned: false, moved_at: None },
        );
    }

    fn declare_owned(&mut self, name: String, ty: Type, pos: Pos) {
        if self.scopes.is_empty() {
            self.scopes.push(HashMap::new());
        }
        self.scopes.last_mut().unwrap().insert(
            name,
            LocalSlot { ty, decl_pos: pos, is_owned: true, moved_at: None },
        );
    }

    fn mark_moved(&mut self, name: &str, pos: Pos) {
        for scope in self.scopes.iter_mut().rev() {
            if let Some(slot) = scope.get_mut(name) {
                slot.moved_at = Some(pos);
                return;
            }
        }
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
                    if let Some(moved_pos) = slot.moved_at {
                        self.error(format!(
                            "use of moved value `{}` (moved at line {})",
                            name, moved_pos.line
                        ));
                    }
                    self.index.refs.push(RefInfo {
                        pos,
                        name: name.clone(),
                        ty: slot.ty.clone(),
                        decl_pos: Some(slot.decl_pos),
                        kind: RefKind::Variable,
                    });
                    (ExprKind::Ident(name), slot.ty)
                } else if let Some(sig) = self.fns.get(&name).cloned() {
                    let fn_ptr_ty = Type::FnPtr {
                        params: sig.params.iter().map(|p| p.ty.clone()).collect(),
                        ret: sig.ret_type.clone().map(Box::new),
                    };
                    self.index.refs.push(RefInfo {
                        pos,
                        name: name.clone(),
                        ty: fn_ptr_ty.clone(),
                        decl_pos: sig.decl_pos,
                        kind: RefKind::Function,
                    });
                    (ExprKind::Ident(name), fn_ptr_ty)
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
                    UnaryOp::Deref => match self.resolve_alias(&inner_ty) {
                        Type::Pointer(t) | Type::Borrow(t) | Type::BorrowMut(t) => (*t).clone(),
                        t if is_error(&t) => error_ty(),
                        _ => {
                            self.error(format!(
                                "cannot dereference non-pointer of type `{}`",
                                type_name(&inner_ty)
                            ));
                            error_ty()
                        }
                    },
                    UnaryOp::AddrOf | UnaryOp::AddrOfMut => {
                        // If inner is already a pointer-like, `&x` and `&mut x`
                        // collapse: the value already represents the address.
                        let inner_resolved = self.resolve_alias(&inner_ty);
                        if let Type::Pointer(t) | Type::Borrow(t) | Type::BorrowMut(t) =
                            &inner_resolved
                        {
                            let inner_t = (**t).clone();
                            let result_ty = if matches!(op, UnaryOp::AddrOfMut) {
                                Type::BorrowMut(Box::new(inner_t))
                            } else {
                                Type::Borrow(Box::new(inner_t))
                            };
                            return (inner_new.kind, result_ty);
                        }
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
                            if matches!(op, UnaryOp::AddrOfMut) {
                                Type::BorrowMut(Box::new(inner_ty.clone()))
                            } else {
                                Type::Borrow(Box::new(inner_ty.clone()))
                            }
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
                let resolved = self.resolve_alias(&obj_ty);
                let elem_ty = match &resolved {
                    Type::Pointer(t) => (**t).clone(),
                    Type::Slice(t) => (**t).clone(),
                    t if is_error(t) => error_ty(),
                    _ => {
                        self.error(format!(
                            "cannot index non-pointer of type `{}`",
                            type_name(&obj_ty)
                        ));
                        error_ty()
                    }
                };
                // For slices, rewrite `s[i]` to `s.data[i]` so codegen indexes the
                // backing pointer rather than the slice struct.
                let obj_for_index = if matches!(resolved, Type::Slice(_)) {
                    Expr::new(
                        ExprKind::Member(Box::new(obj_new.clone()), "data".into()),
                        obj_new.pos,
                    )
                } else {
                    obj_new
                };
                (
                    ExprKind::Index(Box::new(obj_for_index), Box::new(idx_new)),
                    elem_ty,
                )
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

            ExprKind::ArrayLit { elements, as_slice, .. } => {
                if elements.is_empty() {
                    self.error("empty array literal `[]` requires a type annotation".into());
                    return (
                        ExprKind::ArrayLit { elements: Vec::new(), elem_type: None, as_slice },
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
                } else if as_slice {
                    Type::Slice(Box::new(inner))
                } else {
                    Type::Pointer(Box::new(inner))
                };
                (
                    ExprKind::ArrayLit { elements: new_elems, elem_type: elem_ty, as_slice },
                    result_ty,
                )
            }

            ExprKind::FnExpr { params, ret_type, body, is_move } => {
                self.lower_fn_expr(params, ret_type, body, is_move, pos)
            }
        }
    }

    fn lower_fn_expr(
        &mut self,
        params: Vec<Param>,
        ret_type: Option<Type>,
        body: Vec<Stmt>,
        is_move: bool,
        pos: Pos,
    ) -> (ExprKind, Type) {
        let id = self.anon_counter;
        self.anon_counter += 1;

        // Detect free variables in the body (captured from enclosing scope).
        let mut bound: std::collections::HashSet<String> = params.iter().map(|p| p.name.clone()).collect();
        let mut captures: Vec<String> = Vec::new();
        let mut seen: std::collections::HashSet<String> = std::collections::HashSet::new();
        for s in &body {
            collect_free_idents(s, &mut bound, &mut captures, &mut seen);
        }

        // Resolve each capture's type from the enclosing scope.
        let mut capture_fields: Vec<(String, Type)> = Vec::new();
        for name in &captures {
            if let Some(slot) = self.lookup(name) {
                if slot.is_owned {
                    self.error(format!(
                        "cannot capture owned value `{}` (auto-drop conflict)",
                        name
                    ));
                }
                capture_fields.push((name.clone(), slot.ty.clone()));
            }
        }

        if !capture_fields.is_empty() && !is_move {
            self.error(
                "closures that capture variables must be declared with `move` (e.g. `move fn() { ... }`)".into(),
            );
        }

        let synth_fn_name = format!("__glide_anon_fn_{}", id);

        if capture_fields.is_empty() {
            // No captures: lift body to a top-level fn, the expression
            // becomes an Ident referring to the lifted fn (decays to fn pointer).
            let lifted_fn = Stmt {
                kind: StmtKind::Fn {
                    name: synth_fn_name.clone(),
                    type_params: Vec::new(),
                    params: params.clone(),
                    ret_type: ret_type.clone(),
                    body,
                },
                pos,
                is_pub: false,
                source_file: None,
            };
            self.fns.insert(synth_fn_name.clone(), FnSig {
                params: params.clone(),
                ret_type: ret_type.clone(),
                variadic: false,
                decl_pos: Some(pos),
                home_file: None,
                is_pub: true,
            });
            self.synth_stmts.push(lifted_fn);

            let fn_ptr_ty = Type::FnPtr {
                params: params.iter().map(|p| p.ty.clone()).collect(),
                ret: ret_type.map(Box::new),
            };
            return (ExprKind::Ident(synth_fn_name), fn_ptr_ty);
        }

        // Captures present: build a closure struct + a method-like fn that takes
        // the closure struct as `self`.
        let closure_struct_name = format!("__glide_closure_{}", id);

        let struct_fields: Vec<Field> = capture_fields.iter().map(|(n, t)| Field {
            name: n.clone(),
            ty: t.clone(),
            pos,
        }).collect();

        let struct_decl = Stmt {
            kind: StmtKind::Struct {
                name: closure_struct_name.clone(),
                type_params: Vec::new(),
                fields: struct_fields.clone(),
            },
            pos,
            is_pub: false,
            source_file: None,
        };

        // Register the struct in the typer.
        self.structs.insert(closure_struct_name.clone(), StructInfo {
            fields: struct_fields.clone(),
            decl_pos: pos,
            home_file: None,
            is_pub: true,
        });
        self.index.structs.insert(closure_struct_name.clone(), struct_fields.clone());
        self.closure_struct_names.insert(closure_struct_name.clone());

        // Rewrite the body so references to captured names become `self.name`.
        let captured_set: std::collections::HashSet<String> =
            capture_fields.iter().map(|(n, _)| n.clone()).collect();
        let new_body: Vec<Stmt> = body
            .into_iter()
            .map(|s| rewrite_captures_stmt(s, &captured_set))
            .collect();

        // Build the fn that takes self + params.
        let mut fn_params = Vec::with_capacity(params.len() + 1);
        fn_params.push(Param {
            name: "self".to_string(),
            ty: Type::Pointer(Box::new(Type::Named(closure_struct_name.clone()))),
            pos,
        });
        for p in &params {
            fn_params.push(p.clone());
        }

        let lifted_fn_name = format!("{}_call", closure_struct_name);
        let lifted_fn = Stmt {
            kind: StmtKind::Fn {
                name: lifted_fn_name.clone(),
                type_params: Vec::new(),
                params: fn_params.clone(),
                ret_type: ret_type.clone(),
                body: new_body,
            },
            pos,
            is_pub: false,
            source_file: None,
        };

        self.fns.insert(lifted_fn_name.clone(), FnSig {
            params: fn_params,
            ret_type: ret_type.clone(),
            variadic: false,
            decl_pos: Some(pos),
            home_file: None,
            is_pub: true,
        });

        self.synth_stmts.push(struct_decl);
        self.synth_stmts.push(lifted_fn);

        // The expression becomes a struct lit with the captured values.
        // For move semantics, mark sources as moved.
        let init_fields: Vec<(String, Expr)> = capture_fields
            .iter()
            .map(|(n, _)| (n.clone(), Expr::new(ExprKind::Ident(n.clone()), pos)))
            .collect();

        if is_move {
            for (n, _) in &capture_fields {
                if let Some(slot) = self.lookup(n) {
                    if !is_copy_type(&self.resolve_alias(&slot.ty)) {
                        self.mark_moved(n, pos);
                    }
                }
            }
        }

        let result_ty = Type::Named(closure_struct_name.clone());
        (
            ExprKind::StructLit {
                type_name: closure_struct_name,
                fields: init_fields,
            },
            result_ty,
        )
    }

    fn infer_type_args(
        &self,
        type_params: &[String],
        params: &[Param],
        arg_tys: &[Type],
    ) -> Option<Vec<Type>> {
        let mut bindings: HashMap<String, Type> = HashMap::new();
        let pset: std::collections::HashSet<&String> = type_params.iter().collect();
        let n = params.len().min(arg_tys.len());
        for i in 0..n {
            unify_for_inference(&params[i].ty, &arg_tys[i], &pset, &mut bindings);
        }
        let mut out = Vec::with_capacity(type_params.len());
        for tp in type_params {
            match bindings.get(tp) {
                Some(t) => out.push(t.clone()),
                None => return None,
            }
        }
        Some(out)
    }

    fn check_call(&mut self, callee: Expr, args: Vec<Expr>, pos: Pos) -> (Expr, Type) {
        if let ExprKind::Member(_, _) = &callee.kind {
            return self.check_method_call(callee, args, pos);
        }

        // Aliasing check: collect what each arg borrows.
        // For source variable `x`, track if it's borrowed mut or shared in this call.
        // Reject &x + &mut x in the same call, or &mut x + &mut x.
        {
            let mut shared: Vec<String> = Vec::new();
            let mut mutable: Vec<String> = Vec::new();
            for a in &args {
                if let ExprKind::Unary(op, inner) = &a.kind {
                    if let ExprKind::Ident(n) = &inner.kind {
                        match op {
                            UnaryOp::AddrOf => shared.push(n.clone()),
                            UnaryOp::AddrOfMut => mutable.push(n.clone()),
                            _ => {}
                        }
                    }
                }
            }
            for m in &mutable {
                let dup_mut = mutable.iter().filter(|x| *x == m).count();
                if dup_mut > 1 {
                    self.error(format!(
                        "cannot borrow `{}` as mutable more than once in the same call",
                        m
                    ));
                    break;
                }
                if shared.iter().any(|s| s == m) {
                    self.error(format!(
                        "cannot borrow `{}` as both mutable and immutable in the same call",
                        m
                    ));
                    break;
                }
            }
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

        // Closure call: variable holding a closure struct → call its lifted method
        if let ExprKind::Ident(n) = &callee.kind {
            if let Some(slot) = self.lookup(n) {
                let resolved = self.resolve_alias(&slot.ty);
                if let Type::Named(struct_name) = &resolved {
                    if self.closure_struct_names.contains(struct_name) {
                        let lifted_name = format!("{}_call", struct_name);
                        let closure_ref = Expr::new(
                            ExprKind::Unary(
                                UnaryOp::AddrOf,
                                Box::new(callee.clone()),
                            ),
                            pos,
                        );
                        let mut full_args = Vec::with_capacity(args.len() + 1);
                        full_args.push(closure_ref);
                        full_args.extend(args.into_iter());
                        let new_callee = Expr::new(ExprKind::Ident(lifted_name), pos);
                        return self.check_call(new_callee, full_args, pos);
                    }
                }
            }
        }

        // Indirect call: variable holding fn pointer
        if let ExprKind::Ident(n) = &callee.kind {
            if let Some(slot) = self.lookup(n) {
                let resolved = self.resolve_alias(&slot.ty);
                if let Type::FnPtr { params, ret } = resolved {
                    let mut args_new = Vec::with_capacity(args.len());
                    let mut arg_tys = Vec::with_capacity(args.len());
                    for a in args {
                        let (a_new, a_ty) = self.check_expr(a);
                        args_new.push(a_new);
                        arg_tys.push(a_ty);
                    }
                    if params.len() != args_new.len() {
                        self.error(format!(
                            "indirect call: expected {} argument(s), got {}",
                            params.len(),
                            args_new.len()
                        ));
                    } else {
                        for (i, p_ty) in params.iter().enumerate() {
                            let saved = self.current_pos;
                            self.current_pos = args_new[i].pos;
                            self.expect_type(p_ty, &arg_tys[i], &format!("argument {} of fn pointer", i + 1));
                            self.current_pos = saved;
                        }
                    }
                    let ret_ty = ret.map(|r| *r).unwrap_or_else(void_ty);
                    return (
                        Expr::new(ExprKind::Call(Box::new(callee), args_new), pos),
                        ret_ty,
                    );
                }
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

        // Generic fn call: infer type args, mangle name, queue monomorphization.
        if let Some(name) = &fn_name {
            if let Some(g) = self.generic_fns.get(name).cloned() {
                let mut inferred = self.infer_type_args(&g.type_params, &g.params, &arg_tys);
                // If inference from args failed and we have a return-type hint,
                // try unifying ret_type against the hint to bind type vars.
                if inferred.is_none() {
                    if let (Some(hint), Some(ret_ty)) = (
                        self.expected_ret_hint.clone(),
                        g.ret_type.clone(),
                    ) {
                        let mut bindings: HashMap<String, Type> = HashMap::new();
                        let pset: std::collections::HashSet<&String> =
                            g.type_params.iter().collect();
                        unify_for_inference(&ret_ty, &hint, &pset, &mut bindings);
                        let mut all = Vec::with_capacity(g.type_params.len());
                        let mut complete = true;
                        for tp in &g.type_params {
                            match bindings.get(tp) {
                                Some(t) => all.push(t.clone()),
                                None => { complete = false; break; }
                            }
                        }
                        if complete { inferred = Some(all); }
                    }
                }
                if let Some(args_ty) = inferred {
                    let mangled = mono_mangle(name, &args_ty);
                    if !self.mono_done.contains(&mangled) {
                        self.mono_done.insert(mangled.clone());
                        self.mono_queue.push((name.clone(), args_ty.clone()));
                    }
                    // Substitute params/ret for type-check at this call site.
                    let subst: HashMap<String, Type> = g.type_params.iter()
                        .cloned()
                        .zip(args_ty.iter().cloned())
                        .collect();
                    let new_params: Vec<Param> = g.params.iter().map(|p| Param {
                        name: p.name.clone(),
                        ty: subst_type(&p.ty, &subst),
                        pos: p.pos,
                    }).collect();
                    let new_ret = g.ret_type.as_ref().map(|t| subst_type(t, &subst));
                    if new_params.len() != args_new.len() {
                        self.error(format!(
                            "`{}`: expected {} argument(s), got {}",
                            name, new_params.len(), args_new.len()
                        ));
                    } else {
                        for (i, p) in new_params.iter().enumerate() {
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
                    let mangled_callee = Expr::new(ExprKind::Ident(mangled), pos);
                    return (
                        Expr::new(ExprKind::Call(Box::new(mangled_callee), args_new), pos),
                        new_ret.unwrap_or_else(void_ty),
                    );
                } else {
                    self.error(format!(
                        "could not infer type arguments for generic call `{}`",
                        name
                    ));
                }
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
                            // Reject passing owned value as `*T` argument (would double-free).
                            if let ExprKind::Ident(n) = &args_new[i].kind {
                                let is_ptr_param = matches!(
                                    self.resolve_alias(&p.ty),
                                    Type::Pointer(_)
                                );
                                if is_ptr_param {
                                    if let Some(slot) = self.lookup(n) {
                                        if slot.is_owned {
                                            self.error(format!(
                                                "cannot move owned value `{}` into `*T` parameter (auto-drop conflict)",
                                                n
                                            ));
                                        }
                                    }
                                }
                            }
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
            let resolved_ty = self.resolve_alias(&a_ty);
            let (spec, transformed) = format_spec_for(&resolved_ty, a_new, arg_pos);
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
                    let resolved_ty = self.resolve_alias(&a_ty);
            let (spec, transformed) = format_spec_for(&resolved_ty, a_new, arg_pos);
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

        // Special form: arena.create(Type) -> Arena_alloc(arena, sizeof(Type)) as *Type
        //               arena.create(Type, n) -> Arena_alloc(arena, sizeof(Type) * n) as *Type
        // Lets users skip the sizeof + cast boilerplate.
        if method == "create" && (args.len() == 1 || args.len() == 2) {
            let resolved = self.resolve_alias(&obj_ty);
            let is_arena_ptr = matches!(
                &resolved,
                Type::Pointer(inner) if matches!(inner.as_ref(), Type::Named(n) if n == "Arena")
            );
            if is_arena_ptr {
                if let ExprKind::Ident(type_name) = &args[0].kind {
                    if self.structs.contains_key(type_name) || is_known_primitive(type_name) {
                        let target_ty = Type::Named(type_name.clone());
                        let sizeof_expr = Expr::new(ExprKind::Sizeof(target_ty.clone()), pos);

                        let size_arg = if args.len() == 2 {
                            // sizeof(T) * count
                            let count_expr = args.into_iter().nth(1).unwrap();
                            let (count_new, _) = self.check_expr(count_expr);
                            Expr::new(
                                ExprKind::Binary(
                                    BinaryOp::Mul,
                                    Box::new(sizeof_expr),
                                    Box::new(count_new),
                                ),
                                pos,
                            )
                        } else {
                            sizeof_expr
                        };

                        let alloc_callee = Expr::new(
                            ExprKind::Ident("Arena_alloc".into()),
                            pos,
                        );
                        let alloc_call = Expr::new(
                            ExprKind::Call(
                                Box::new(alloc_callee),
                                vec![obj_new, size_arg],
                            ),
                            pos,
                        );
                        let result_ty = Type::Pointer(Box::new(target_ty));
                        let cast = Expr::new(
                            ExprKind::Cast(Box::new(alloc_call), result_ty.clone()),
                            pos,
                        );
                        return (cast, result_ty);
                    }
                }
            }
        }

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
        let obj_ty = self.resolve_alias(&obj_ty);

        if let Type::Slice(_) = &obj_ty {
            if field == "len" {
                return (
                    Expr::new(ExprKind::Member(Box::new(obj_new), field), pos),
                    Type::Named("usize".into()),
                );
            }
            if field == "data" {
                if let Type::Slice(inner) = &obj_ty {
                    return (
                        Expr::new(ExprKind::Member(Box::new(obj_new), field), pos),
                        Type::Pointer(inner.clone()),
                    );
                }
            }
            self.error(format!(
                "slice has no field `{}` (only `.len` and `.data`)",
                field
            ));
            return (
                Expr::new(ExprKind::Member(Box::new(obj_new), field), pos),
                error_ty(),
            );
        }

        let (final_obj, struct_name) = match &obj_ty {
            Type::Named(n) => (obj_new, n.clone()),
            Type::Pointer(inner) | Type::Borrow(inner) | Type::BorrowMut(inner) => match inner.as_ref() {
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
        for (fname, mut fexpr) in fields {
            // Propagate generic-struct hint from field type into nested
            // StructLit / New so `Box { ... }` inside `Pair<int, Box<int>>` works.
            if let Some(f) = struct_fields.iter().find(|f| f.name == fname) {
                if let Type::Named(expected_named) = &f.ty {
                    let rewrite = match &mut fexpr.kind {
                        ExprKind::StructLit { type_name: tn, .. } => Some(tn),
                        ExprKind::New { type_name: tn, .. } => Some(tn),
                        _ => None,
                    };
                    if let Some(tn) = rewrite {
                        if self.generic_structs.contains_key(&*tn)
                            && expected_named.starts_with(&format!("{}__", tn))
                        {
                            *tn = expected_named.clone();
                        }
                    }
                }
            }
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
        let exp = self.resolve_alias(expected);
        let act = self.resolve_alias(actual);
        if !types_compat(&exp, &act) {
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
    if let (Type::Named(e), Type::Named(a)) = (expected, actual) {
        if is_integer_primitive(e) && is_integer_primitive(a) {
            return true;
        }
        if is_float_primitive(e) && is_float_primitive(a) {
            return true;
        }
    }
    if is_null_ptr(actual) && matches!(expected, Type::Pointer(_) | Type::Borrow(_) | Type::BorrowMut(_)) {
        return true;
    }
    if is_null_ptr(expected) && matches!(actual, Type::Pointer(_) | Type::Borrow(_) | Type::BorrowMut(_)) {
        return true;
    }
    if is_void_ptr(actual) && matches!(expected, Type::Pointer(_) | Type::Borrow(_) | Type::BorrowMut(_)) {
        return true;
    }
    if is_void_ptr(expected) && matches!(actual, Type::Pointer(_) | Type::Borrow(_) | Type::BorrowMut(_)) {
        return true;
    }
    // *T, &T, &mut T are interconvertible at this v1 stage
    // (full move/borrow semantics arrive in step 2-3)
    let expected_inner = pointer_like_inner(expected);
    let actual_inner = pointer_like_inner(actual);
    if let (Some(ei), Some(ai)) = (expected_inner, actual_inner) {
        if types_compat(ei, ai) {
            return true;
        }
    }
    false
}

fn pointer_like_inner(t: &Type) -> Option<&Type> {
    match t {
        Type::Pointer(inner) => Some(inner),
        Type::Borrow(inner) => Some(inner),
        Type::BorrowMut(inner) => Some(inner),
        _ => None,
    }
}

pub fn unify_for_inference(
    pat: &Type,
    actual: &Type,
    type_params: &std::collections::HashSet<&String>,
    bindings: &mut HashMap<String, Type>,
) {
    if let Type::Named(n) = pat {
        if type_params.contains(n) {
            bindings.entry(n.clone()).or_insert_with(|| actual.clone());
            return;
        }
    }
    match (pat, actual) {
        (Type::Pointer(a), Type::Pointer(b))
        | (Type::Pointer(a), Type::Borrow(b))
        | (Type::Pointer(a), Type::BorrowMut(b))
        | (Type::Borrow(a), Type::Borrow(b))
        | (Type::Borrow(a), Type::Pointer(b))
        | (Type::Borrow(a), Type::BorrowMut(b))
        | (Type::BorrowMut(a), Type::BorrowMut(b))
        | (Type::BorrowMut(a), Type::Pointer(b))
        | (Type::Slice(a), Type::Slice(b))
        | (Type::Chan(a), Type::Chan(b)) => {
            unify_for_inference(a, b, type_params, bindings);
        }
        (Type::Generic { name: pn, args: pa }, Type::Generic { name: an, args: aa })
            if pn == an && pa.len() == aa.len() =>
        {
            for (pi, ai) in pa.iter().zip(aa.iter()) {
                unify_for_inference(pi, ai, type_params, bindings);
            }
        }
        _ => {}
    }
}

fn subst_type(t: &Type, subst: &HashMap<String, Type>) -> Type {
    match t {
        Type::Named(n) => {
            if let Some(rep) = subst.get(n) {
                return rep.clone();
            }
            t.clone()
        }
        Type::Generic { name, args } => Type::Generic {
            name: name.clone(),
            args: args.iter().map(|a| subst_type(a, subst)).collect(),
        },
        Type::Pointer(inner) => Type::Pointer(Box::new(subst_type(inner, subst))),
        Type::Borrow(inner) => Type::Borrow(Box::new(subst_type(inner, subst))),
        Type::BorrowMut(inner) => Type::BorrowMut(Box::new(subst_type(inner, subst))),
        Type::Chan(inner) => Type::Chan(Box::new(subst_type(inner, subst))),
        Type::Slice(inner) => Type::Slice(Box::new(subst_type(inner, subst))),
        Type::FnPtr { params, ret } => Type::FnPtr {
            params: params.iter().map(|p| subst_type(p, subst)).collect(),
            ret: ret.as_ref().map(|r| Box::new(subst_type(r, subst))),
        },
    }
}

fn mono_mangle(base: &str, args: &[Type]) -> String {
    let mut out = String::from(base);
    out.push_str("__");
    for (i, a) in args.iter().enumerate() {
        if i > 0 { out.push('_'); }
        out.push_str(&a.mangle());
    }
    out
}

fn subst_stmt(stmt: Stmt, subst: &HashMap<String, Type>) -> Stmt {
    let kind = match stmt.kind {
        StmtKind::Let { name, ty, value, is_mut, is_owned } => StmtKind::Let {
            name,
            ty: ty.map(|t| subst_type(&t, subst)),
            value: value.map(|v| subst_expr(v, subst)),
            is_mut,
            is_owned,
        },
        StmtKind::Const { name, ty, value } => StmtKind::Const {
            name,
            ty: ty.map(|t| subst_type(&t, subst)),
            value: subst_expr(value, subst),
        },
        StmtKind::Block(stmts) => StmtKind::Block(
            stmts.into_iter().map(|s| subst_stmt(s, subst)).collect()
        ),
        StmtKind::If { cond, then_block, else_block } => StmtKind::If {
            cond: subst_expr(cond, subst),
            then_block: then_block.into_iter().map(|s| subst_stmt(s, subst)).collect(),
            else_block: else_block.map(|b| b.into_iter().map(|s| subst_stmt(s, subst)).collect()),
        },
        StmtKind::While { cond, body } => StmtKind::While {
            cond: subst_expr(cond, subst),
            body: body.into_iter().map(|s| subst_stmt(s, subst)).collect(),
        },
        StmtKind::For { init, cond, step, body } => StmtKind::For {
            init: init.map(|s| Box::new(subst_stmt(*s, subst))),
            cond: cond.map(|c| subst_expr(c, subst)),
            step: step.map(|s| subst_expr(s, subst)),
            body: body.into_iter().map(|s| subst_stmt(s, subst)).collect(),
        },
        StmtKind::Return(v) => StmtKind::Return(v.map(|e| subst_expr(e, subst))),
        StmtKind::Expr(e) => StmtKind::Expr(subst_expr(e, subst)),
        StmtKind::Spawn(e) => StmtKind::Spawn(subst_expr(e, subst)),
        StmtKind::Defer(e) => StmtKind::Defer(subst_expr(e, subst)),
        StmtKind::Match { scrutinee, arms } => StmtKind::Match {
            scrutinee: subst_expr(scrutinee, subst),
            arms: arms.into_iter().map(|a| MatchArm {
                pattern: a.pattern,
                body: a.body.into_iter().map(|s| subst_stmt(s, subst)).collect(),
            }).collect(),
        },
        other => other,
    };
    Stmt { kind, pos: stmt.pos, is_pub: stmt.is_pub, source_file: stmt.source_file }
}

fn subst_expr(expr: Expr, subst: &HashMap<String, Type>) -> Expr {
    let kind = match expr.kind {
        ExprKind::Unary(op, inner) => ExprKind::Unary(op, Box::new(subst_expr(*inner, subst))),
        ExprKind::Binary(op, l, r) => ExprKind::Binary(
            op,
            Box::new(subst_expr(*l, subst)),
            Box::new(subst_expr(*r, subst)),
        ),
        ExprKind::Assign(l, op, r) => ExprKind::Assign(
            Box::new(subst_expr(*l, subst)),
            op,
            Box::new(subst_expr(*r, subst)),
        ),
        ExprKind::Call(c, args) => ExprKind::Call(
            Box::new(subst_expr(*c, subst)),
            args.into_iter().map(|a| subst_expr(a, subst)).collect(),
        ),
        ExprKind::Index(o, i) => ExprKind::Index(
            Box::new(subst_expr(*o, subst)),
            Box::new(subst_expr(*i, subst)),
        ),
        ExprKind::Member(o, name) => ExprKind::Member(Box::new(subst_expr(*o, subst)), name),
        ExprKind::Cast(inner, t) => ExprKind::Cast(
            Box::new(subst_expr(*inner, subst)),
            subst_type(&t, subst),
        ),
        ExprKind::Sizeof(t) => ExprKind::Sizeof(subst_type(&t, subst)),
        ExprKind::StructLit { type_name, fields } => ExprKind::StructLit {
            type_name,
            fields: fields.into_iter().map(|(n, v)| (n, subst_expr(v, subst))).collect(),
        },
        ExprKind::New { type_name, fields } => ExprKind::New {
            type_name,
            fields: fields.into_iter().map(|(n, v)| (n, subst_expr(v, subst))).collect(),
        },
        ExprKind::ArrayLit { elements, elem_type, as_slice } => ExprKind::ArrayLit {
            elements: elements.into_iter().map(|e| subst_expr(e, subst)).collect(),
            elem_type: elem_type.map(|t| subst_type(&t, subst)),
            as_slice,
        },
        ExprKind::AddrOfTemp { value, ty } => ExprKind::AddrOfTemp {
            value: Box::new(subst_expr(*value, subst)),
            ty: subst_type(&ty, subst),
        },
        ExprKind::MacroCall { name, args } => ExprKind::MacroCall {
            name,
            args: args.into_iter().map(|a| subst_expr(a, subst)).collect(),
        },
        ExprKind::EnumCtor { enum_name, variant, args } => ExprKind::EnumCtor {
            enum_name,
            variant,
            args: args.into_iter().map(|a| subst_expr(a, subst)).collect(),
        },
        ExprKind::FnExpr { params, ret_type, body, is_move } => ExprKind::FnExpr {
            params: params.into_iter().map(|p| Param {
                name: p.name,
                ty: subst_type(&p.ty, subst),
                pos: p.pos,
            }).collect(),
            ret_type: ret_type.map(|t| subst_type(&t, subst)),
            body: body.into_iter().map(|s| subst_stmt(s, subst)).collect(),
            is_move,
        },
        other => other,
    };
    Expr { kind, pos: expr.pos }
}

pub fn type_name(t: &Type) -> String {
    match t {
        Type::Named(n) => n.clone(),
        Type::Generic { name, args } => {
            let a = args.iter().map(type_name).collect::<Vec<_>>().join(", ");
            format!("{}<{}>", name, a)
        }
        Type::Pointer(inner) => format!("*{}", type_name(inner)),
        Type::Borrow(inner) => format!("&{}", type_name(inner)),
        Type::BorrowMut(inner) => format!("&mut {}", type_name(inner)),
        Type::Chan(inner) => format!("chan<{}>", type_name(inner)),
        Type::Slice(inner) => format!("[]{}", type_name(inner)),
        Type::FnPtr { params, ret } => {
            let p = params.iter().map(type_name).collect::<Vec<_>>().join(", ");
            match ret {
                Some(r) => format!("fn({}) -> {}", p, type_name(r)),
                None => format!("fn({})", p),
            }
        }
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

fn arena_ptr() -> Type {
    Type::Pointer(Box::new(Type::Named("Arena".into())))
}

fn is_known_primitive(name: &str) -> bool {
    matches!(
        name,
        "int" | "float" | "bool" | "char" | "string"
            | "uint" | "long" | "ulong"
            | "i8" | "i16" | "i32" | "i64"
            | "u8" | "u16" | "u32" | "u64"
            | "usize" | "isize"
            | "f32" | "f64"
    )
}

fn is_integer_primitive(name: &str) -> bool {
    matches!(
        name,
        "int" | "uint" | "long" | "ulong"
            | "i8" | "i16" | "i32" | "i64"
            | "u8" | "u16" | "u32" | "u64"
            | "usize" | "isize" | "char"
    )
}

fn is_float_primitive(name: &str) -> bool {
    matches!(name, "float" | "f32" | "f64")
}

fn collect_free_idents(
    stmt: &Stmt,
    bound: &mut std::collections::HashSet<String>,
    captures: &mut Vec<String>,
    seen: &mut std::collections::HashSet<String>,
) {
    match &stmt.kind {
        StmtKind::Let { name, value, .. } => {
            if let Some(v) = value {
                collect_free_in_expr(v, bound, captures, seen);
            }
            bound.insert(name.clone());
        }
        StmtKind::Const { name, value, .. } => {
            collect_free_in_expr(value, bound, captures, seen);
            bound.insert(name.clone());
        }
        StmtKind::Expr(e) => collect_free_in_expr(e, bound, captures, seen),
        StmtKind::Return(v) => {
            if let Some(e) = v { collect_free_in_expr(e, bound, captures, seen); }
        }
        StmtKind::If { cond, then_block, else_block } => {
            collect_free_in_expr(cond, bound, captures, seen);
            for s in then_block { collect_free_idents(s, bound, captures, seen); }
            if let Some(b) = else_block {
                for s in b { collect_free_idents(s, bound, captures, seen); }
            }
        }
        StmtKind::While { cond, body } => {
            collect_free_in_expr(cond, bound, captures, seen);
            for s in body { collect_free_idents(s, bound, captures, seen); }
        }
        StmtKind::For { init, cond, step, body } => {
            if let Some(s) = init { collect_free_idents(s, bound, captures, seen); }
            if let Some(c) = cond { collect_free_in_expr(c, bound, captures, seen); }
            if let Some(s) = step { collect_free_in_expr(s, bound, captures, seen); }
            for s in body { collect_free_idents(s, bound, captures, seen); }
        }
        StmtKind::Block(stmts) => {
            let saved = bound.clone();
            for s in stmts { collect_free_idents(s, bound, captures, seen); }
            *bound = saved;
        }
        StmtKind::Defer(e) => collect_free_in_expr(e, bound, captures, seen),
        StmtKind::Spawn(e) => collect_free_in_expr(e, bound, captures, seen),
        StmtKind::Match { scrutinee, arms } => {
            collect_free_in_expr(scrutinee, bound, captures, seen);
            for a in arms {
                for s in &a.body { collect_free_idents(s, bound, captures, seen); }
            }
        }
        _ => {}
    }
}

fn collect_free_in_expr(
    expr: &Expr,
    bound: &std::collections::HashSet<String>,
    captures: &mut Vec<String>,
    seen: &mut std::collections::HashSet<String>,
) {
    match &expr.kind {
        ExprKind::Ident(name) => {
            if !bound.contains(name) && !seen.contains(name) {
                // Skip names that look like fn names or types — we'll let the typer
                // resolve them later. Heuristic: lowercase first char + not a known
                // primitive means it's a candidate capture.
                if !is_known_primitive(name) {
                    captures.push(name.clone());
                    seen.insert(name.clone());
                }
            }
        }
        ExprKind::Unary(_, e) => collect_free_in_expr(e, bound, captures, seen),
        ExprKind::Binary(_, l, r) => {
            collect_free_in_expr(l, bound, captures, seen);
            collect_free_in_expr(r, bound, captures, seen);
        }
        ExprKind::Assign(l, _, r) => {
            collect_free_in_expr(l, bound, captures, seen);
            collect_free_in_expr(r, bound, captures, seen);
        }
        ExprKind::Call(c, args) => {
            collect_free_in_expr(c, bound, captures, seen);
            for a in args { collect_free_in_expr(a, bound, captures, seen); }
        }
        ExprKind::Index(o, i) => {
            collect_free_in_expr(o, bound, captures, seen);
            collect_free_in_expr(i, bound, captures, seen);
        }
        ExprKind::Member(o, _) => collect_free_in_expr(o, bound, captures, seen),
        ExprKind::Cast(e, _) => collect_free_in_expr(e, bound, captures, seen),
        ExprKind::StructLit { fields, .. } | ExprKind::New { fields, .. } => {
            for (_, v) in fields { collect_free_in_expr(v, bound, captures, seen); }
        }
        ExprKind::ArrayLit { elements, .. } => {
            for e in elements { collect_free_in_expr(e, bound, captures, seen); }
        }
        ExprKind::AddrOfTemp { value, .. } => collect_free_in_expr(value, bound, captures, seen),
        ExprKind::MacroCall { args, .. } => {
            for a in args { collect_free_in_expr(a, bound, captures, seen); }
        }
        ExprKind::EnumCtor { args, .. } => {
            for a in args { collect_free_in_expr(a, bound, captures, seen); }
        }
        _ => {}
    }
}

fn rewrite_captures_stmt(stmt: Stmt, captured: &std::collections::HashSet<String>) -> Stmt {
    let pos = stmt.pos;
    let is_pub = stmt.is_pub;
    let source_file = stmt.source_file.clone();
    let kind = match stmt.kind {
        StmtKind::Let { name, ty, value, is_mut, is_owned } => StmtKind::Let {
            name,
            ty,
            value: value.map(|v| rewrite_captures_expr(v, captured)),
            is_mut,
            is_owned,
        },
        StmtKind::Const { name, ty, value } => StmtKind::Const {
            name,
            ty,
            value: rewrite_captures_expr(value, captured),
        },
        StmtKind::Expr(e) => StmtKind::Expr(rewrite_captures_expr(e, captured)),
        StmtKind::Return(v) => StmtKind::Return(v.map(|e| rewrite_captures_expr(e, captured))),
        StmtKind::If { cond, then_block, else_block } => StmtKind::If {
            cond: rewrite_captures_expr(cond, captured),
            then_block: then_block.into_iter().map(|s| rewrite_captures_stmt(s, captured)).collect(),
            else_block: else_block.map(|b| b.into_iter().map(|s| rewrite_captures_stmt(s, captured)).collect()),
        },
        StmtKind::While { cond, body } => StmtKind::While {
            cond: rewrite_captures_expr(cond, captured),
            body: body.into_iter().map(|s| rewrite_captures_stmt(s, captured)).collect(),
        },
        StmtKind::For { init, cond, step, body } => StmtKind::For {
            init: init.map(|b| Box::new(rewrite_captures_stmt(*b, captured))),
            cond: cond.map(|c| rewrite_captures_expr(c, captured)),
            step: step.map(|s| rewrite_captures_expr(s, captured)),
            body: body.into_iter().map(|s| rewrite_captures_stmt(s, captured)).collect(),
        },
        StmtKind::Block(stmts) => StmtKind::Block(
            stmts.into_iter().map(|s| rewrite_captures_stmt(s, captured)).collect(),
        ),
        StmtKind::Defer(e) => StmtKind::Defer(rewrite_captures_expr(e, captured)),
        StmtKind::Spawn(e) => StmtKind::Spawn(rewrite_captures_expr(e, captured)),
        StmtKind::Match { scrutinee, arms } => StmtKind::Match {
            scrutinee: rewrite_captures_expr(scrutinee, captured),
            arms: arms.into_iter().map(|a| MatchArm {
                pattern: a.pattern,
                body: a.body.into_iter().map(|s| rewrite_captures_stmt(s, captured)).collect(),
            }).collect(),
        },
        other => other,
    };
    Stmt { kind, pos, is_pub, source_file }
}

fn rewrite_captures_expr(expr: Expr, captured: &std::collections::HashSet<String>) -> Expr {
    let pos = expr.pos;
    let kind = match expr.kind {
        ExprKind::Ident(name) if captured.contains(&name) => {
            // Replace `name` with `self.name`
            ExprKind::Member(
                Box::new(Expr::new(ExprKind::Ident("self".into()), pos)),
                name,
            )
        }
        ExprKind::Unary(op, inner) => ExprKind::Unary(op, Box::new(rewrite_captures_expr(*inner, captured))),
        ExprKind::Binary(op, l, r) => ExprKind::Binary(
            op,
            Box::new(rewrite_captures_expr(*l, captured)),
            Box::new(rewrite_captures_expr(*r, captured)),
        ),
        ExprKind::Assign(l, op, r) => ExprKind::Assign(
            Box::new(rewrite_captures_expr(*l, captured)),
            op,
            Box::new(rewrite_captures_expr(*r, captured)),
        ),
        ExprKind::Call(c, args) => ExprKind::Call(
            Box::new(rewrite_captures_expr(*c, captured)),
            args.into_iter().map(|a| rewrite_captures_expr(a, captured)).collect(),
        ),
        ExprKind::Index(o, i) => ExprKind::Index(
            Box::new(rewrite_captures_expr(*o, captured)),
            Box::new(rewrite_captures_expr(*i, captured)),
        ),
        ExprKind::Member(o, f) => ExprKind::Member(
            Box::new(rewrite_captures_expr(*o, captured)),
            f,
        ),
        ExprKind::Cast(e, t) => ExprKind::Cast(Box::new(rewrite_captures_expr(*e, captured)), t),
        ExprKind::StructLit { type_name, fields } => ExprKind::StructLit {
            type_name,
            fields: fields.into_iter().map(|(n, v)| (n, rewrite_captures_expr(v, captured))).collect(),
        },
        ExprKind::New { type_name, fields } => ExprKind::New {
            type_name,
            fields: fields.into_iter().map(|(n, v)| (n, rewrite_captures_expr(v, captured))).collect(),
        },
        ExprKind::ArrayLit { elements, elem_type, as_slice } => ExprKind::ArrayLit {
            elements: elements.into_iter().map(|e| rewrite_captures_expr(e, captured)).collect(),
            elem_type,
            as_slice,
        },
        ExprKind::AddrOfTemp { value, ty } => ExprKind::AddrOfTemp {
            value: Box::new(rewrite_captures_expr(*value, captured)),
            ty,
        },
        ExprKind::MacroCall { name, args } => ExprKind::MacroCall {
            name,
            args: args.into_iter().map(|a| rewrite_captures_expr(a, captured)).collect(),
        },
        ExprKind::EnumCtor { enum_name, variant, args } => ExprKind::EnumCtor {
            enum_name,
            variant,
            args: args.into_iter().map(|a| rewrite_captures_expr(a, captured)).collect(),
        },
        other => other,
    };
    Expr::new(kind, pos)
}

fn is_copy_type(t: &Type) -> bool {
    match t {
        Type::Named(n) => is_known_primitive(n),
        Type::Generic { .. } => false,
        Type::Borrow(_) => true,
        Type::FnPtr { .. } => true,
        Type::Pointer(_) => false,
        Type::BorrowMut(_) => false,
        Type::Chan(_) => false,
        Type::Slice(_) => true,
    }
}

fn is_always_visible(kind: &StmtKind) -> bool {
    matches!(
        kind,
        StmtKind::Impl { .. } | StmtKind::ExternFn { .. } | StmtKind::ExternType { .. }
            | StmtKind::CInclude(_) | StmtKind::CLink(_)
    )
}

fn format_spec_for(ty: &Type, arg: Expr, pos: Pos) -> (&'static str, Expr) {
    match ty {
        Type::Named(n) => match n.as_str() {
            "int" | "i8" | "i16" | "i32"  => ("%d", arg),
            "i64" | "isize" | "long"      => ("%lld", arg),
            "uint" | "u8" | "u16" | "u32" => ("%u", arg),
            "u64" | "usize" | "ulong"     => ("%llu", arg),
            "float" | "f32" | "f64"       => ("%g", arg),
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
        Type::Pointer(_) | Type::Borrow(_) | Type::BorrowMut(_) | Type::Chan(_) | Type::FnPtr { .. } => ("%p", arg),
        Type::Slice(_) => ("%p", arg),
        Type::Generic { .. } => ("%p", arg),
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
