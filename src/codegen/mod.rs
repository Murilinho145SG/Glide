use crate::ast::*;
use crate::types::Type;
use std::collections::HashMap;

#[derive(Debug, Clone)]
pub struct CodegenError {
    pub message: String,
}

pub struct EmitResult {
    pub c_source: String,
    pub needs_pthread: bool,
}

impl std::fmt::Display for CodegenError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "codegen error: {}", self.message)
    }
}

#[derive(Clone)]
#[allow(dead_code)]
struct FnSig {
    params: Vec<Param>,
    ret_type: Option<Type>,
}

pub struct Codegen {
    out: String,
    indent: usize,
    fn_sigs: HashMap<String, FnSig>,
    spawn_count: usize,
    spawn_stubs: String,
    uses_concurrency: bool,
    new_count: usize,
    match_count: usize,
    known_enums: std::collections::HashSet<String>,
    enum_infos: HashMap<String, Vec<(String, Vec<Type>)>>,
    defer_stack: Vec<Vec<Expr>>,
}

impl Codegen {
    pub fn new() -> Self {
        Self {
            out: String::new(),
            indent: 0,
            fn_sigs: HashMap::new(),
            spawn_count: 0,
            spawn_stubs: String::new(),
            uses_concurrency: false,
            new_count: 0,
            match_count: 0,
            known_enums: std::collections::HashSet::new(),
            enum_infos: HashMap::new(),
            defer_stack: Vec::new(),
        }
    }

    pub fn emit_program(mut self, program: &Program) -> Result<EmitResult, CodegenError> {
        let all_fns = collect_all_fns(program);

        for (name, params, ret_type) in &all_fns {
            self.fn_sigs.insert(
                name.clone(),
                FnSig { params: params.clone(), ret_type: ret_type.clone() },
            );
        }

        self.uses_concurrency = program_uses_concurrency(program);

        self.write_prelude();

        for stmt in program {
            if let StmtKind::Enum { name, variants } = &stmt.kind {
                self.known_enums.insert(name.clone());
                let info: Vec<(String, Vec<Type>)> = variants
                    .iter()
                    .map(|v| (v.name.clone(), v.fields.clone()))
                    .collect();
                self.enum_infos.insert(name.clone(), info);
            }
        }

        let mut any_struct = false;
        for stmt in program {
            if let StmtKind::Struct { name, .. } = &stmt.kind {
                self.push(&format!("typedef struct {} {};\n", name, name));
                any_struct = true;
            }
            if let StmtKind::Enum { name, .. } = &stmt.kind {
                self.push(&format!("typedef struct {} {};\n", name, name));
                any_struct = true;
            }
        }
        if any_struct { self.push("\n"); }

        for stmt in program {
            if let StmtKind::Struct { name, fields } = &stmt.kind {
                self.emit_struct_def(name, fields)?;
            }
            if let StmtKind::Enum { name, variants } = &stmt.kind {
                self.emit_enum_def(name, variants)?;
            }
        }

        if self.uses_concurrency {
            let chan_types = collect_chan_inner_types(program);
            if !chan_types.is_empty() {
                self.emit_chan_runtime(&chan_types);
            }
        }

        for (name, params, ret_type) in &all_fns {
            self.emit_fn_signature(name, params, ret_type.as_ref())?;
            self.push(";\n");
        }
        self.push("\n");

        let saved_out = std::mem::take(&mut self.out);
        for stmt in program {
            match &stmt.kind {
                StmtKind::Struct { .. } => continue,
                _ => {
                    self.emit_top_level(stmt)?;
                    self.push("\n");
                }
            }
        }
        let defs = std::mem::replace(&mut self.out, saved_out);

        if !self.spawn_stubs.is_empty() {
            self.out.push_str(&self.spawn_stubs);
        }
        self.out.push_str(&defs);

        Ok(EmitResult {
            c_source: self.out,
            needs_pthread: self.uses_concurrency,
        })
    }

    fn emit_struct_def(&mut self, name: &str, fields: &[Field]) -> Result<(), CodegenError> {
        self.push(&format!("struct {} {{\n", name));
        self.indent += 1;
        for f in fields {
            self.write_indent();
            let ty = self.type_to_c(&f.ty);
            self.push(&ty);
            self.push(" ");
            self.push(&f.name);
            self.push(";\n");
        }
        self.indent -= 1;
        self.push("};\n\n");
        Ok(())
    }

    fn emit_enum_def(&mut self, name: &str, variants: &[EnumVariant]) -> Result<(), CodegenError> {
        let has_payload = variants.iter().any(|v| !v.fields.is_empty());

        self.push(&format!("struct {} {{\n", name));
        self.push("    int tag;\n");
        if has_payload {
            self.push("    union {\n");
            for v in variants {
                if v.fields.is_empty() { continue; }
                if v.fields.len() == 1 {
                    let ty = self.type_to_c(&v.fields[0]);
                    self.push(&format!("        {} v_{};\n", ty, v.name));
                } else {
                    self.push(&format!("        struct {{\n"));
                    for (i, f) in v.fields.iter().enumerate() {
                        let ty = self.type_to_c(f);
                        self.push(&format!("            {} f{};\n", ty, i));
                    }
                    self.push(&format!("        }} v_{};\n", v.name));
                }
            }
            self.push("    } data;\n");
        }
        self.push("};\n");
        for (i, v) in variants.iter().enumerate() {
            let upper = enum_tag_name(name, &v.name);
            self.push(&format!("#define {} {}\n", upper, i));
        }
        self.push("\n");
        Ok(())
    }

    fn write_prelude(&mut self) {
        self.push("/* generated by glide");
        if self.uses_concurrency {
            self.push(" -- compile with: gcc out.c -lpthread -o out");
        }
        self.push(" */\n");
        self.push("#include <stdio.h>\n");
        self.push("#include <stdlib.h>\n");
        self.push("#include <stdbool.h>\n");
        self.push("#include <stddef.h>\n");
        self.push("#include <string.h>\n");
        self.push("#include <stdarg.h>\n");
        self.push("#include <ctype.h>\n");
        self.push("#include <math.h>\n");
        if self.uses_concurrency {
            self.push("#include <pthread.h>\n");
        }
        self.push("\n");
        self.push(STDLIB_C);
        self.push("\n");
    }

    fn emit_chan_runtime(&mut self, chan_inner_types: &[Type]) {
        for inner in chan_inner_types {
            let mangled = inner.mangle();
            self.push(&format!(
                "typedef struct __glide_chan_{m}_t __glide_chan_{m}_t;\n",
                m = mangled
            ));
        }
        self.push("\n");

        for inner in chan_inner_types {
            let mangled = inner.mangle();
            let elem_c = self.type_to_c(inner);
            self.push(&format!(
"struct __glide_chan_{m}_t {{
    pthread_mutex_t mu;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
    {t}*            buf;
    int             cap;
    int             len;
    int             head;
    int             tail;
    int             closed;
}};

static __glide_chan_{m}_t* __glide_make_chan_{m}(int cap) {{
    __glide_chan_{m}_t* c = (__glide_chan_{m}_t*)malloc(sizeof(__glide_chan_{m}_t));
    if (!c) return NULL;
    pthread_mutex_init(&c->mu, NULL);
    pthread_cond_init(&c->not_empty, NULL);
    pthread_cond_init(&c->not_full, NULL);
    c->cap    = cap > 0 ? cap : 1;
    c->buf    = ({t}*)malloc(sizeof({t}) * (size_t)c->cap);
    c->len    = 0;
    c->head   = 0;
    c->tail   = 0;
    c->closed = 0;
    return c;
}}

static void __glide_send_{m}(__glide_chan_{m}_t* c, {t} v) {{
    pthread_mutex_lock(&c->mu);
    while (c->len == c->cap && !c->closed) {{
        pthread_cond_wait(&c->not_full, &c->mu);
    }}
    if (c->closed) {{
        pthread_mutex_unlock(&c->mu);
        return; /* drop on closed channel */
    }}
    c->buf[c->tail] = v;
    c->tail = (c->tail + 1) % c->cap;
    c->len++;
    pthread_cond_signal(&c->not_empty);
    pthread_mutex_unlock(&c->mu);
}}

static {t} __glide_recv_{m}(__glide_chan_{m}_t* c) {{
    pthread_mutex_lock(&c->mu);
    while (c->len == 0 && !c->closed) {{
        pthread_cond_wait(&c->not_empty, &c->mu);
    }}
    {t} v = ({t}){{0}};
    if (c->len > 0) {{
        v = c->buf[c->head];
        c->head = (c->head + 1) % c->cap;
        c->len--;
        pthread_cond_signal(&c->not_full);
    }}
    pthread_mutex_unlock(&c->mu);
    return v;
}}

static void __glide_close_{m}(__glide_chan_{m}_t* c) {{
    pthread_mutex_lock(&c->mu);
    c->closed = 1;
    pthread_cond_broadcast(&c->not_empty);
    pthread_cond_broadcast(&c->not_full);
    pthread_mutex_unlock(&c->mu);
}}

",
                m = mangled,
                t = elem_c,
            ));
        }
    }

    fn emit_top_level(&mut self, stmt: &Stmt) -> Result<(), CodegenError> {
        match &stmt.kind {
            StmtKind::Fn { name, params, ret_type, body } => {
                self.emit_fn_def(name, params, ret_type.as_ref(), body)
            }
            StmtKind::Let { .. } | StmtKind::Const { .. } => self.emit_stmt(stmt),
            StmtKind::Struct { name, fields } => self.emit_struct_def(name, fields),
            StmtKind::Interface { .. } | StmtKind::Import(_) | StmtKind::Enum { .. } => Ok(()),
            StmtKind::Impl { methods, .. } => {
                for m in methods {
                    self.emit_top_level(m)?;
                    self.push("\n");
                }
                Ok(())
            }
            other => Err(self.err(format!(
                "unsupported top-level statement: {:?}",
                std::mem::discriminant(other)
            ))),
        }
    }

    fn emit_fn_signature(
        &mut self,
        name: &str,
        params: &[Param],
        ret_type: Option<&Type>,
    ) -> Result<(), CodegenError> {
        if name == "main" {
            if !params.is_empty() {
                return Err(self.err("`fn main` must take no parameters".into()));
            }
            self.push("int main(int __glide_main_argc, char** __glide_main_argv)");
            return Ok(());
        }

        let ret_c = match ret_type {
            Some(t) => self.type_to_c(t),
            None => "void".to_string(),
        };
        self.push(&ret_c);
        self.push(" ");
        self.push(name);
        self.push("(");
        if params.is_empty() {
            self.push("void");
        } else {
            for (i, p) in params.iter().enumerate() {
                if i > 0 { self.push(", "); }
                let ty = self.type_to_c(&p.ty);
                self.push(&ty);
                self.push(" ");
                self.push(&p.name);
            }
        }
        self.push(")");
        Ok(())
    }

    fn emit_deferred_in_reverse(&mut self) -> Result<(), CodegenError> {
        if let Some(top) = self.defer_stack.last().cloned() {
            for expr in top.iter().rev() {
                self.write_indent();
                self.emit_expr(expr)?;
                self.push(";\n");
            }
        }
        Ok(())
    }

    fn emit_fn_def(
        &mut self,
        name: &str,
        params: &[Param],
        ret_type: Option<&Type>,
        body: &[Stmt],
    ) -> Result<(), CodegenError> {
        let is_main = name == "main";
        if is_main {
            if let Some(t) = ret_type {
                let s = self.type_to_c(t);
                if s != "int" {
                    return Err(self.err(format!(
                        "`fn main` must return `int` or nothing, got `{}`",
                        s
                    )));
                }
            }
        }

        self.emit_fn_signature(name, params, ret_type)?;
        self.push(" {\n");
        self.indent += 1;
        if is_main {
            self.write_indent();
            self.push("__glide_args_init(__glide_main_argc, __glide_main_argv);\n");
        }
        self.defer_stack.push(Vec::new());
        for s in body {
            self.emit_stmt(s)?;
        }
        self.emit_deferred_in_reverse()?;
        self.defer_stack.pop();
        if is_main && ret_type.is_none() {
            self.write_indent();
            self.push("return 0;\n");
        }
        self.indent -= 1;
        self.push("}\n");
        Ok(())
    }

    fn emit_stmt(&mut self, stmt: &Stmt) -> Result<(), CodegenError> {
        match &stmt.kind {
            StmtKind::Let { name, ty, value } => {
                self.write_indent();
                let resolved = self.resolve_let_type(ty.as_ref(), value.as_ref())?;
                self.push(&resolved);
                self.push(" ");
                self.push(name);
                if let Some(v) = value {
                    self.push(" = ");
                    self.emit_expr(v)?;
                }
                self.push(";\n");
            }
            StmtKind::Const { name, ty, value } => {
                self.write_indent();
                let resolved = self.resolve_let_type(ty.as_ref(), Some(value))?;
                self.push("const ");
                self.push(&resolved);
                self.push(" ");
                self.push(name);
                self.push(" = ");
                self.emit_expr(value)?;
                self.push(";\n");
            }
            StmtKind::Fn { .. } => {
                return Err(self.err(
                    "nested `fn` is not supported (declare at top level)".into(),
                ));
            }
            StmtKind::Struct { .. } => {
                return Err(self.err(
                    "nested `struct` is not supported (declare at top level)".into(),
                ));
            }
            StmtKind::Block(stmts) => {
                self.write_indent();
                self.push("{\n");
                self.indent += 1;
                for s in stmts {
                    self.emit_stmt(s)?;
                }
                self.indent -= 1;
                self.write_indent();
                self.push("}\n");
            }
            StmtKind::If { cond, then_block, else_block } => {
                self.write_indent();
                self.push("if (");
                self.emit_expr(cond)?;
                self.push(") {\n");
                self.indent += 1;
                for s in then_block {
                    self.emit_stmt(s)?;
                }
                self.indent -= 1;
                self.write_indent();
                self.push("}");
                if let Some(else_stmts) = else_block {
                    if else_stmts.len() == 1 {
                        if let StmtKind::If { .. } = &else_stmts[0].kind {
                            self.push(" else ");
                            let saved = self.indent;
                            self.indent = 0;
                            self.emit_stmt(&else_stmts[0])?;
                            self.indent = saved;
                            return Ok(());
                        }
                    }
                    self.push(" else {\n");
                    self.indent += 1;
                    for s in else_stmts {
                        self.emit_stmt(s)?;
                    }
                    self.indent -= 1;
                    self.write_indent();
                    self.push("}\n");
                } else {
                    self.push("\n");
                }
            }
            StmtKind::While { cond, body } => {
                self.write_indent();
                self.push("while (");
                self.emit_expr(cond)?;
                self.push(") {\n");
                self.indent += 1;
                for s in body {
                    self.emit_stmt(s)?;
                }
                self.indent -= 1;
                self.write_indent();
                self.push("}\n");
            }
            StmtKind::For { init, cond, step, body } => {
                self.write_indent();
                self.push("for (");
                if let Some(init_stmt) = init {
                    self.emit_for_init(init_stmt)?;
                }
                self.push("; ");
                if let Some(c) = cond {
                    self.emit_expr(c)?;
                }
                self.push("; ");
                if let Some(s) = step {
                    self.emit_expr(s)?;
                }
                self.push(") {\n");
                self.indent += 1;
                for s in body {
                    self.emit_stmt(s)?;
                }
                self.indent -= 1;
                self.write_indent();
                self.push("}\n");
            }
            StmtKind::Break => {
                self.write_indent();
                self.push("break;\n");
            }
            StmtKind::Continue => {
                self.write_indent();
                self.push("continue;\n");
            }
            StmtKind::Return(value) => {
                if let Some(v) = value {
                    let tmp = format!("__glide_ret_{}", self.match_count + 1);
                    self.match_count += 1;
                    self.write_indent();
                    self.push(&format!("__typeof__("));
                    self.emit_expr(v)?;
                    self.push(&format!(") {} = ", tmp));
                    self.emit_expr(v)?;
                    self.push(";\n");
                    let stack: Vec<Vec<Expr>> = self.defer_stack.clone();
                    for level in stack.iter().rev() {
                        for e in level.iter().rev() {
                            self.write_indent();
                            self.emit_expr(e)?;
                            self.push(";\n");
                        }
                    }
                    self.write_indent();
                    self.push(&format!("return {};\n", tmp));
                } else {
                    let stack: Vec<Vec<Expr>> = self.defer_stack.clone();
                    for level in stack.iter().rev() {
                        for e in level.iter().rev() {
                            self.write_indent();
                            self.emit_expr(e)?;
                            self.push(";\n");
                        }
                    }
                    self.write_indent();
                    self.push("return;\n");
                }
            }
            StmtKind::Expr(e) => {
                self.write_indent();
                self.emit_expr(e)?;
                self.push(";\n");
            }
            StmtKind::Spawn(e) => self.emit_spawn(e)?,
            StmtKind::Defer(e) => {
                if let Some(top) = self.defer_stack.last_mut() {
                    top.push(e.clone());
                }
            }
            StmtKind::Interface { .. } | StmtKind::Impl { .. } => {
                return Err(self.err("`interface`/`impl` only allowed at top level".into()));
            }
            StmtKind::Import(_) => {
                return Err(self.err("`import` should have been resolved before codegen".into()));
            }
            StmtKind::Enum { .. } => {
                return Err(self.err("`enum` only allowed at top level".into()));
            }
            StmtKind::Match { scrutinee, arms } => {
                self.emit_match(scrutinee, arms)?;
            }
        }
        Ok(())
    }

    fn emit_match(&mut self, scrutinee: &Expr, arms: &[MatchArm]) -> Result<(), CodegenError> {
        self.match_count += 1;
        let var = format!("__glide_match_{}", self.match_count);

        let enum_name: Option<String> = arms.iter().find_map(|a| {
            if let PatternKind::Variant { enum_name: Some(n), .. } = &a.pattern.kind {
                Some(n.clone())
            } else {
                None
            }
        });

        self.write_indent();
        self.push("{\n");
        self.indent += 1;
        self.write_indent();
        if let Some(en) = &enum_name {
            self.push(&format!("{} {} = ", en, var));
        } else {
            self.push(&format!("__typeof__("));
            self.emit_expr(scrutinee)?;
            self.push(&format!(") {} = ", var));
        }
        self.emit_expr(scrutinee)?;
        self.push(";\n");

        if let Some(enum_name) = enum_name {
            self.write_indent();
            self.push(&format!("switch ({}.tag) {{\n", var));
            self.indent += 1;
            for arm in arms {
                self.emit_match_arm(arm, &var, &enum_name)?;
            }
            self.indent -= 1;
            self.write_indent();
            self.push("}\n");
        } else {
            for arm in arms {
                self.emit_match_arm_value(arm, &var)?;
            }
        }

        self.indent -= 1;
        self.write_indent();
        self.push("}\n");
        Ok(())
    }

    fn emit_match_arm(&mut self, arm: &MatchArm, var: &str, enum_name: &str) -> Result<(), CodegenError> {
        match &arm.pattern.kind {
            PatternKind::Wildcard | PatternKind::Bind(_) => {
                self.write_indent();
                self.push("default: {\n");
                self.indent += 1;
                if let PatternKind::Bind(name) = &arm.pattern.kind {
                    self.write_indent();
                    let ty = self.match_scrutinee_type_string(enum_name);
                    self.push(&format!("{} {} = {};\n", ty, name, var));
                }
                for s in &arm.body { self.emit_stmt(s)?; }
                self.write_indent();
                self.push("break;\n");
                self.indent -= 1;
                self.write_indent();
                self.push("}\n");
            }
            PatternKind::Variant { variant, bindings, .. } => {
                self.write_indent();
                self.push(&format!("case {}: {{\n", enum_tag_name(enum_name, variant)));
                self.indent += 1;
                for (i, b) in bindings.iter().enumerate() {
                    if let PatternKind::Bind(bname) = &b.kind {
                        self.write_indent();
                        let ty = self.variant_field_type(enum_name, variant, i);
                        if bindings.len() == 1 {
                            self.push(&format!("{} {} = {}.data.v_{};\n", ty, bname, var, variant));
                        } else {
                            self.push(&format!("{} {} = {}.data.v_{}.f{};\n", ty, bname, var, variant, i));
                        }
                    }
                }
                for s in &arm.body { self.emit_stmt(s)?; }
                self.write_indent();
                self.push("break;\n");
                self.indent -= 1;
                self.write_indent();
                self.push("}\n");
            }
            PatternKind::Literal(_) => {
                return Err(self.err("literal patterns not yet supported in enum match".into()));
            }
        }
        Ok(())
    }

    fn emit_match_arm_value(&mut self, arm: &MatchArm, var: &str) -> Result<(), CodegenError> {
        match &arm.pattern.kind {
            PatternKind::Wildcard => {
                self.write_indent();
                self.push("{\n");
                self.indent += 1;
                for s in &arm.body { self.emit_stmt(s)?; }
                self.indent -= 1;
                self.write_indent();
                self.push("}\n");
                Ok(())
            }
            PatternKind::Bind(name) => {
                self.write_indent();
                self.push(&format!("{{\n"));
                self.indent += 1;
                self.write_indent();
                self.push(&format!("__typeof__({}) {} = {};\n", var, name, var));
                for s in &arm.body { self.emit_stmt(s)?; }
                self.indent -= 1;
                self.write_indent();
                self.push("}\n");
                Ok(())
            }
            PatternKind::Literal(lit) => {
                self.write_indent();
                self.push(&format!("if ({} == ", var));
                self.emit_expr(lit)?;
                self.push(") {\n");
                self.indent += 1;
                for s in &arm.body { self.emit_stmt(s)?; }
                self.indent -= 1;
                self.write_indent();
                self.push("}\n");
                Ok(())
            }
            PatternKind::Variant { .. } => {
                Err(self.err("variant pattern requires the scrutinee to be an enum".into()))
            }
        }
    }

    fn match_scrutinee_type_string(&self, enum_name: &str) -> String {
        enum_name.into()
    }

    fn variant_field_type(&self, enum_name: &str, variant: &str, idx: usize) -> String {
        if let Some(info) = self.enum_infos.get(enum_name) {
            if let Some(v) = info.iter().find(|v| v.0 == variant) {
                if let Some(ty) = v.1.get(idx) {
                    return self.type_to_c(ty);
                }
            }
        }
        "void*".into()
    }

    fn emit_spawn(&mut self, expr: &Expr) -> Result<(), CodegenError> {
        let (callee, args) = match &expr.kind {
            ExprKind::Call(c, a) => (c.as_ref(), a),
            _ => return Err(self.err("`spawn` requires a function call".into())),
        };
        let fn_name = match &callee.kind {
            ExprKind::Ident(n) => n.clone(),
            _ => return Err(self.err(
                "`spawn` requires a direct function name".into(),
            )),
        };
        let sig = self.fn_sigs.get(&fn_name).cloned().ok_or_else(|| self.err(format!(
            "unknown function `{}` in spawn",
            fn_name
        )))?;
        if sig.params.len() != args.len() {
            return Err(self.err(format!(
                "`spawn {}`: expected {} arg(s), got {}",
                fn_name, sig.params.len(), args.len()
            )));
        }

        let stub_id = self.spawn_count;
        self.spawn_count += 1;
        let args_struct = format!("__glide_spawn_args_{}", stub_id);
        let stub_name   = format!("__glide_spawn_stub_{}", stub_id);

        let mut stub = String::new();
        stub.push_str(&format!("struct {} {{\n", args_struct));
        for p in &sig.params {
            let t = self.type_to_c(&p.ty);
            stub.push_str(&format!("    {} {};\n", t, p.name));
        }
        stub.push_str("};\n");
        stub.push_str(&format!("static void* {}(void* arg_) {{\n", stub_name));
        stub.push_str(&format!(
            "    struct {}* args = (struct {}*)arg_;\n",
            args_struct, args_struct
        ));
        stub.push_str(&format!("    {}(", fn_name));
        for (i, p) in sig.params.iter().enumerate() {
            if i > 0 { stub.push_str(", "); }
            stub.push_str(&format!("args->{}", p.name));
        }
        stub.push_str(");\n");
        stub.push_str("    free(args);\n");
        stub.push_str("    return NULL;\n");
        stub.push_str("}\n\n");
        self.spawn_stubs.push_str(&stub);

        self.write_indent();
        self.push("{\n");
        self.indent += 1;
        self.write_indent();
        self.push(&format!(
            "struct {}* __args = (struct {}*)malloc(sizeof(struct {}));\n",
            args_struct, args_struct, args_struct
        ));
        for (i, p) in sig.params.iter().enumerate() {
            self.write_indent();
            self.push(&format!("__args->{} = ", p.name));
            self.emit_expr(&args[i])?;
            self.push(";\n");
        }
        self.write_indent();
        self.push("pthread_t __tid;\n");
        self.write_indent();
        self.push(&format!(
            "pthread_create(&__tid, NULL, {}, __args);\n",
            stub_name
        ));
        self.write_indent();
        self.push("pthread_detach(__tid);\n");
        self.indent -= 1;
        self.write_indent();
        self.push("}\n");

        Ok(())
    }

    fn emit_for_init(&mut self, stmt: &Stmt) -> Result<(), CodegenError> {
        match &stmt.kind {
            StmtKind::Let { name, ty, value } => {
                let resolved = self.resolve_let_type(ty.as_ref(), value.as_ref())?;
                self.push(&resolved);
                self.push(" ");
                self.push(name);
                if let Some(v) = value {
                    self.push(" = ");
                    self.emit_expr(v)?;
                }
            }
            StmtKind::Expr(e) => self.emit_expr(e)?,
            other => {
                return Err(self.err(format!(
                    "unsupported `for` init: {:?}",
                    std::mem::discriminant(other)
                )));
            }
        }
        Ok(())
    }

    fn emit_expr(&mut self, expr: &Expr) -> Result<(), CodegenError> {
        match &expr.kind {
            ExprKind::Int(n) => self.push(&n.to_string()),
            ExprKind::Float(f) => {
                let s = format!("{}", f);
                self.push(&s);
                if !s.contains('.') && !s.contains('e') && !s.contains('E') {
                    self.push(".0");
                }
            }
            ExprKind::String(s) => {
                self.push("\"");
                let escaped = escape_for_c_string(s);
                self.push(&escaped);
                self.push("\"");
            }
            ExprKind::Char(c) => {
                self.push("'");
                let escaped = escape_for_c_char(*c);
                self.push(&escaped);
                self.push("'");
            }
            ExprKind::Bool(b) => self.push(if *b { "true" } else { "false" }),
            ExprKind::Null => self.push("NULL"),
            ExprKind::Ident(name) => self.push(name),

            ExprKind::Unary(op, inner) => {
                self.push("(");
                match op {
                    UnaryOp::Neg     => { self.push("-"); self.emit_expr(inner)?; }
                    UnaryOp::Not     => { self.push("!"); self.emit_expr(inner)?; }
                    UnaryOp::BitNot  => { self.push("~"); self.emit_expr(inner)?; }
                    UnaryOp::Deref   => { self.push("*"); self.emit_expr(inner)?; }
                    UnaryOp::AddrOf  => { self.push("&"); self.emit_expr(inner)?; }
                    UnaryOp::PostInc => { self.emit_expr(inner)?; self.push("++"); }
                    UnaryOp::PostDec => { self.emit_expr(inner)?; self.push("--"); }
                }
                self.push(")");
            }

            ExprKind::Binary(op, l, r) => {
                self.push("(");
                self.emit_expr(l)?;
                self.push(" ");
                self.push(binop_to_c(op));
                self.push(" ");
                self.emit_expr(r)?;
                self.push(")");
            }

            ExprKind::Assign(lhs, op, rhs) => {
                self.push("(");
                self.emit_expr(lhs)?;
                self.push(" ");
                self.push(assignop_to_c(op));
                self.push(" ");
                self.emit_expr(rhs)?;
                self.push(")");
            }

            ExprKind::Call(callee, args) => {
                self.emit_expr(callee)?;
                self.push("(");
                for (i, a) in args.iter().enumerate() {
                    if i > 0 { self.push(", "); }
                    self.emit_expr(a)?;
                }
                self.push(")");
            }

            ExprKind::Index(obj, idx) => {
                self.emit_expr(obj)?;
                self.push("[");
                self.emit_expr(idx)?;
                self.push("]");
            }

            ExprKind::Member(obj, name) => {
                self.emit_expr(obj)?;
                self.push(".");
                self.push(name);
            }

            ExprKind::Cast(inner, target) => {
                let t = self.type_to_c(target);
                self.push("((");
                self.push(&t);
                self.push(")");
                self.emit_expr(inner)?;
                self.push(")");
            }

            ExprKind::Sizeof(target) => {
                // sizeof returns size_t in C; cast back to int to match the typer.
                let t = self.type_to_c(target);
                self.push("((int)sizeof(");
                self.push(&t);
                self.push("))");
            }

            ExprKind::StructLit { type_name, fields } => {
                self.push("(");
                self.push(type_name);
                self.push("){");
                for (i, (fname, val)) in fields.iter().enumerate() {
                    if i > 0 { self.push(", "); }
                    self.push(".");
                    self.push(fname);
                    self.push(" = ");
                    self.emit_expr(val)?;
                }
                self.push("}");
            }

            ExprKind::New { type_name, fields } => {
                self.new_count += 1;
                let var = format!("__glide_new_p_{}", self.new_count);
                self.push("(__extension__ ({ ");
                self.push(type_name);
                self.push(&format!("* {} = (", var));
                self.push(type_name);
                self.push(&format!("*)malloc(sizeof({})); *{} = (", type_name, var));
                self.push(type_name);
                self.push("){");
                for (i, (fname, val)) in fields.iter().enumerate() {
                    if i > 0 { self.push(", "); }
                    self.push(".");
                    self.push(fname);
                    self.push(" = ");
                    self.emit_expr(val)?;
                }
                self.push(&format!("}}; {}; }}))", var));
            }

            ExprKind::MacroCall { name, .. } => {
                return Err(self.err(format!(
                    "macro `{}!` was not expanded by the typer",
                    name
                )));
            }

            ExprKind::Path { ty, member } => {
                self.push(ty);
                self.push("_");
                self.push(member);
            }

            ExprKind::AddrOfTemp { value, ty } => {
                let c_ty = self.type_to_c(ty);
                self.push("(&((");
                self.push(&c_ty);
                self.push("){");
                self.emit_expr(value)?;
                self.push("}))");
            }

            ExprKind::EnumCtor { enum_name, variant, args } => {
                self.push("((");
                self.push(enum_name);
                self.push("){ .tag = ");
                self.push(&enum_tag_name(enum_name, variant));
                if !args.is_empty() {
                    self.push(&format!(", .data.v_{} = ", variant));
                    if args.len() == 1 {
                        self.emit_expr(&args[0])?;
                    } else {
                        self.push("{ ");
                        for (i, a) in args.iter().enumerate() {
                            if i > 0 { self.push(", "); }
                            self.push(&format!(".f{} = ", i));
                            self.emit_expr(a)?;
                        }
                        self.push(" }");
                    }
                }
                self.push(" })");
            }

            ExprKind::ArrayLit { elements, elem_type } => {
                let elem_c = match elem_type {
                    Some(t) => self.type_to_c(t),
                    None => return Err(self.err(
                        "array literal type was not resolved by the typer".into(),
                    )),
                };
                self.push("((");
                self.push(&elem_c);
                self.push("[]){");
                for (i, e) in elements.iter().enumerate() {
                    if i > 0 { self.push(", "); }
                    self.emit_expr(e)?;
                }
                self.push("})");
            }
        }
        Ok(())
    }

    fn type_to_c(&self, ty: &Type) -> String {
        match ty {
            Type::Named(name) => match name.as_str() {
                "int"    => "int".into(),
                "float"  => "double".into(),
                "bool"   => "bool".into(),
                "char"   => "char".into(),
                "string" => "const char*".into(),
                "void"   => "void".into(),
                other    => other.into(),
            },
            Type::Pointer(inner) => format!("{}*", self.type_to_c(inner)),
            Type::Chan(inner) => format!("__glide_chan_{}_t*", inner.mangle()),
        }
    }

    fn resolve_let_type(
        &self,
        annotated: Option<&Type>,
        value: Option<&Expr>,
    ) -> Result<String, CodegenError> {
        if let Some(t) = annotated {
            return Ok(self.type_to_c(t));
        }
        let v = value.ok_or_else(|| self.err(
            "let without type annotation requires an initializer".into(),
        ))?;
        let inferred = infer_literal_type(v).ok_or_else(|| self.err(
            "cannot infer type for non-literal initializer; add a type annotation".into(),
        ))?;
        Ok(self.type_to_c(&inferred))
    }

    fn push(&mut self, s: &str) {
        self.out.push_str(s);
    }

    fn write_indent(&mut self) {
        for _ in 0..self.indent {
            self.out.push_str("    ");
        }
    }

    fn err(&self, message: String) -> CodegenError {
        CodegenError { message }
    }
}

impl Default for Codegen {
    fn default() -> Self {
        Self::new()
    }
}

fn enum_tag_name(enum_name: &str, variant: &str) -> String {
    format!("__GLIDE_TAG_{}_{}", enum_name.to_uppercase(), variant.to_uppercase())
}

fn binop_to_c(op: &BinaryOp) -> &'static str {
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

fn assignop_to_c(op: &AssignOp) -> &'static str {
    match op {
        AssignOp::Assign => "=",
        AssignOp::AddAssign => "+=", AssignOp::SubAssign => "-=",
        AssignOp::MulAssign => "*=", AssignOp::DivAssign => "/=",
        AssignOp::ModAssign => "%=",
        AssignOp::BitAndAssign => "&=", AssignOp::BitOrAssign => "|=",
        AssignOp::BitXorAssign => "^=",
        AssignOp::ShlAssign => "<<=", AssignOp::ShrAssign => ">>=",
    }
}

fn program_uses_concurrency(program: &Program) -> bool {
    program.iter().any(stmt_uses_concurrency)
}

fn collect_chan_inner_types(program: &Program) -> Vec<Type> {
    let mut found = std::collections::BTreeMap::<String, Type>::new();
    for stmt in program {
        collect_in_stmt(stmt, &mut found);
    }
    found.into_values().collect()
}

fn collect_in_stmt(stmt: &Stmt, out: &mut std::collections::BTreeMap<String, Type>) {
    match &stmt.kind {
        StmtKind::Let { ty, .. } | StmtKind::Const { ty, .. } => {
            if let Some(t) = ty { collect_in_type(t, out); }
        }
        StmtKind::Fn { params, ret_type, body, .. } => {
            for p in params { collect_in_type(&p.ty, out); }
            if let Some(t) = ret_type { collect_in_type(t, out); }
            for s in body { collect_in_stmt(s, out); }
        }
        StmtKind::Struct { fields, .. } => {
            for f in fields { collect_in_type(&f.ty, out); }
        }
        StmtKind::Block(stmts) => for s in stmts { collect_in_stmt(s, out); },
        StmtKind::If { then_block, else_block, .. } => {
            for s in then_block { collect_in_stmt(s, out); }
            if let Some(b) = else_block {
                for s in b { collect_in_stmt(s, out); }
            }
        }
        StmtKind::While { body, .. } => for s in body { collect_in_stmt(s, out); },
        StmtKind::For { init, body, .. } => {
            if let Some(s) = init { collect_in_stmt(s, out); }
            for s in body { collect_in_stmt(s, out); }
        }
        _ => {}
    }
}

fn collect_in_type(ty: &Type, out: &mut std::collections::BTreeMap<String, Type>) {
    match ty {
        Type::Chan(inner) => {
            out.insert(inner.mangle(), (**inner).clone());
            collect_in_type(inner, out);
        }
        Type::Pointer(inner) => collect_in_type(inner, out),
        Type::Named(_) => {}
    }
}

fn stmt_uses_concurrency(stmt: &Stmt) -> bool {
    match &stmt.kind {
        StmtKind::Spawn(_) => true,
        StmtKind::Fn { params, ret_type, body, .. } => {
            params.iter().any(|p| ty_has_chan(&p.ty))
                || ret_type.as_ref().map_or(false, ty_has_chan)
                || body.iter().any(stmt_uses_concurrency)
        }
        StmtKind::Let { ty, value, .. } => {
            ty.as_ref().map_or(false, ty_has_chan)
                || value.as_ref().map_or(false, expr_uses_chan)
        }
        StmtKind::Const { ty, value, .. } => {
            ty.as_ref().map_or(false, ty_has_chan) || expr_uses_chan(value)
        }
        StmtKind::Block(stmts) => stmts.iter().any(stmt_uses_concurrency),
        StmtKind::If { cond, then_block, else_block } => {
            expr_uses_chan(cond)
                || then_block.iter().any(stmt_uses_concurrency)
                || else_block
                    .as_ref()
                    .map_or(false, |b: &Vec<Stmt>| b.iter().any(stmt_uses_concurrency))
        }
        StmtKind::While { cond, body } => {
            expr_uses_chan(cond) || body.iter().any(stmt_uses_concurrency)
        }
        StmtKind::For { init, cond, step, body } => {
            init.as_ref().map_or(false, |s| stmt_uses_concurrency(s))
                || cond.as_ref().map_or(false, expr_uses_chan)
                || step.as_ref().map_or(false, expr_uses_chan)
                || body.iter().any(stmt_uses_concurrency)
        }
        StmtKind::Return(v) => v.as_ref().map_or(false, expr_uses_chan),
        StmtKind::Expr(e) => expr_uses_chan(e),
        StmtKind::Struct { fields, .. } => fields.iter().any(|f| ty_has_chan(&f.ty)),
        StmtKind::Break | StmtKind::Continue => false,
        StmtKind::Interface { .. } | StmtKind::Import(_) | StmtKind::Enum { .. } => false,
        StmtKind::Impl { methods, .. } => methods.iter().any(stmt_uses_concurrency),
        StmtKind::Match { scrutinee, arms } => {
            expr_uses_chan(scrutinee)
                || arms.iter().any(|a| a.body.iter().any(stmt_uses_concurrency))
        }
        StmtKind::Defer(e) => expr_uses_chan(e),
    }
}

fn ty_has_chan(ty: &Type) -> bool {
    match ty {
        Type::Chan(_) => true,
        Type::Pointer(inner) => ty_has_chan(inner),
        Type::Named(_) => false,
    }
}

fn expr_uses_chan(expr: &Expr) -> bool {
    match &expr.kind {
        ExprKind::Unary(_, e) => expr_uses_chan(e),
        ExprKind::Binary(_, l, r) => expr_uses_chan(l) || expr_uses_chan(r),
        ExprKind::Assign(l, _, r) => expr_uses_chan(l) || expr_uses_chan(r),
        ExprKind::Call(c, args) => expr_uses_chan(c) || args.iter().any(expr_uses_chan),
        ExprKind::Index(o, i) => expr_uses_chan(o) || expr_uses_chan(i),
        ExprKind::Member(o, _) => expr_uses_chan(o),
        ExprKind::Cast(e, t) => expr_uses_chan(e) || ty_has_chan(t),
        ExprKind::Sizeof(t) => ty_has_chan(t),
        ExprKind::StructLit { fields, .. } => fields.iter().any(|(_, v)| expr_uses_chan(v)),
        ExprKind::New { fields, .. } => fields.iter().any(|(_, v)| expr_uses_chan(v)),
        _ => false,
    }
}

fn infer_literal_type(expr: &Expr) -> Option<Type> {
    Some(match &expr.kind {
        ExprKind::Int(_)    => Type::Named("int".into()),
        ExprKind::Float(_)  => Type::Named("float".into()),
        ExprKind::Bool(_)   => Type::Named("bool".into()),
        ExprKind::Char(_)   => Type::Named("char".into()),
        ExprKind::String(_) => Type::Named("string".into()),
        _ => return None,
    })
}

fn escape_for_c_string(s: &str) -> String {
    let mut out = String::with_capacity(s.len());
    for c in s.chars() {
        match c {
            '\n' => out.push_str("\\n"),
            '\t' => out.push_str("\\t"),
            '\r' => out.push_str("\\r"),
            '\\' => out.push_str("\\\\"),
            '"'  => out.push_str("\\\""),
            '\0' => out.push_str("\\0"),
            c if (c as u32) < 0x20 => out.push_str(&format!("\\x{:02x}", c as u32)),
            c => out.push(c),
        }
    }
    out
}

fn escape_for_c_char(c: char) -> String {
    match c {
        '\n' => "\\n".into(),
        '\t' => "\\t".into(),
        '\r' => "\\r".into(),
        '\\' => "\\\\".into(),
        '\'' => "\\'".into(),
        '\0' => "\\0".into(),
        c if (c as u32) < 0x20 => format!("\\x{:02x}", c as u32),
        c => c.to_string(),
    }
}

type FnDecl = (String, Vec<Param>, Option<Type>);

fn collect_all_fns(program: &Program) -> Vec<FnDecl> {
    let mut out = Vec::new();
    for stmt in program {
        match &stmt.kind {
            StmtKind::Fn { name, params, ret_type, .. } => {
                out.push((name.clone(), params.clone(), ret_type.clone()));
            }
            StmtKind::Impl { methods, .. } => {
                for m in methods {
                    if let StmtKind::Fn { name, params, ret_type, .. } = &m.kind {
                        out.push((name.clone(), params.clone(), ret_type.clone()));
                    }
                }
            }
            _ => {}
        }
    }
    out
}

const STDLIB_C: &str = r#"
static const char* __glide_int_to_string(int n) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%d", n);
    char* out = (char*)malloc((size_t)len + 1);
    memcpy(out, buf, (size_t)len + 1);
    return out;
}

static double __glide_int_to_float(int n) {
    return (double)n;
}

static int __glide_int_abs(int n) {
    return n < 0 ? -n : n;
}

static const char* __glide_float_to_string(double f) {
    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%g", f);
    char* out = (char*)malloc((size_t)len + 1);
    memcpy(out, buf, (size_t)len + 1);
    return out;
}

static int __glide_float_to_int(double f) {
    return (int)f;
}

static double __glide_float_floor(double f) {
    long long i = (long long)f;
    return (double)((f < 0.0 && f != (double)i) ? i - 1 : i);
}

static double __glide_float_ceil(double f) {
    long long i = (long long)f;
    return (double)((f > 0.0 && f != (double)i) ? i + 1 : i);
}

static int __glide_string_len(const char* s) {
    return (int)strlen(s);
}

static bool __glide_string_eq(const char* a, const char* b) {
    return strcmp(a, b) == 0;
}

static char __glide_string_at(const char* s, int i) {
    return s[i];
}

static const char* __glide_string_concat(const char* a, const char* b) {
    size_t la = strlen(a);
    size_t lb = strlen(b);
    char* out = (char*)malloc(la + lb + 1);
    memcpy(out, a, la);
    memcpy(out + la, b, lb);
    out[la + lb] = 0;
    return out;
}

static const char* __glide_bool_to_string(bool b) {
    return b ? "true" : "false";
}

static int __glide_char_to_int(char c) {
    return (int)c;
}

static bool __glide_char_is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool __glide_char_is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static int    __glide_argc_g = 0;
static char** __glide_argv_g = NULL;

static void __glide_args_init(int argc, char** argv) {
    __glide_argc_g = argc;
    __glide_argv_g = argv;
}

static int args_count(void) { return __glide_argc_g; }

static const char* args_at(int i) {
    if (i < 0 || i >= __glide_argc_g) return "";
    return __glide_argv_g[i];
}

static const char* env_get(const char* name) {
    const char* v = getenv(name);
    return v ? v : "";
}

static void panic(const char* msg) {
    fprintf(stderr, "panic: %s\n", msg);
    exit(1);
}

static void __glide_assert(bool cond, const char* msg) {
    if (!cond) {
        fprintf(stderr, "%s\n", msg);
        exit(1);
    }
}

static const char* __glide_format(const char* fmt, ...) {
    va_list ap1, ap2;
    va_start(ap1, fmt);
    va_copy(ap2, ap1);
    int n = vsnprintf(NULL, 0, fmt, ap1);
    va_end(ap1);
    if (n < 0) { va_end(ap2); return ""; }
    char* out = (char*)malloc((size_t)n + 1);
    vsnprintf(out, (size_t)n + 1, fmt, ap2);
    va_end(ap2);
    return out;
}

static const char* read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    if (n < 0) { fclose(f); return ""; }
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)n + 1);
    size_t got = fread(buf, 1, (size_t)n, f);
    fclose(f);
    buf[got] = 0;
    return buf;
}

static bool write_file(const char* path, const char* content) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    size_t n = strlen(content);
    size_t wrote = fwrite(content, 1, n, f);
    fclose(f);
    return wrote == n;
}

static bool file_exists(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    fclose(f);
    return true;
}
"#;
