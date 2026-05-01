use crate::ast::{Program, StmtKind};
use crate::fmt as glide_fmt;
use crate::parser::Parser as GlideParser;
use crate::typer::{type_name, DeclInfo, DeclKind, RefKind, SymbolIndex, Typer};
use crate::types::Type;
use std::collections::{HashMap, HashSet};
use std::path::{Path, PathBuf};
use std::sync::Mutex;
use tower_lsp::jsonrpc::Result as LspResult;
use tower_lsp::lsp_types::*;
use tower_lsp::{Client, LanguageServer, LspService, Server};

struct DocumentState {
    text: String,
    program: Option<Program>,
    index: SymbolIndex,
    imported_modules: HashSet<String>,
}

pub struct Backend {
    client: Client,
    docs: Mutex<HashMap<Url, DocumentState>>,
}

impl Backend {
    fn new(client: Client) -> Self {
        Self {
            client,
            docs: Mutex::new(HashMap::new()),
        }
    }

    async fn refresh(&self, uri: Url, source: String) {
        let base_dir = uri
            .to_file_path()
            .ok()
            .and_then(|p| p.parent().map(|d| d.to_path_buf()));
        let analysis = analyze(&source, base_dir.as_deref());
        self.docs.lock().unwrap().insert(
            uri.clone(),
            DocumentState {
                text: source,
                program: analysis.program,
                index: analysis.index,
                imported_modules: analysis.imported_modules,
            },
        );
        self.client.publish_diagnostics(uri, analysis.diagnostics, None).await;
    }
}

#[tower_lsp::async_trait]
impl LanguageServer for Backend {
    async fn initialize(&self, _: InitializeParams) -> LspResult<InitializeResult> {
        Ok(InitializeResult {
            capabilities: ServerCapabilities {
                text_document_sync: Some(TextDocumentSyncCapability::Kind(
                    TextDocumentSyncKind::FULL,
                )),
                hover_provider: Some(HoverProviderCapability::Simple(true)),
                definition_provider: Some(OneOf::Left(true)),
                document_formatting_provider: Some(OneOf::Left(true)),
                completion_provider: Some(CompletionOptions {
                    resolve_provider: Some(false),
                    trigger_characters: Some(vec![".".into(), ":".into()]),
                    ..Default::default()
                }),
                ..Default::default()
            },
            server_info: Some(ServerInfo {
                name: "glide-lsp".to_string(),
                version: Some(env!("CARGO_PKG_VERSION").to_string()),
            }),
        })
    }

    async fn initialized(&self, _: InitializedParams) {
        self.client
            .log_message(MessageType::INFO, "glide-lsp ready")
            .await;
    }

    async fn did_open(&self, params: DidOpenTextDocumentParams) {
        let uri = params.text_document.uri.clone();
        self.refresh(uri, params.text_document.text).await;
    }

    async fn did_change(&self, mut params: DidChangeTextDocumentParams) {
        if let Some(change) = params.content_changes.pop() {
            let uri = params.text_document.uri.clone();
            self.refresh(uri, change.text).await;
        }
    }

    async fn did_save(&self, params: DidSaveTextDocumentParams) {
        let uri = params.text_document.uri;
        let text = self.docs.lock().unwrap().get(&uri).map(|d| d.text.clone());
        if let Some(text) = text {
            self.refresh(uri, text).await;
        }
    }

    async fn did_close(&self, params: DidCloseTextDocumentParams) {
        let uri = params.text_document.uri;
        self.docs.lock().unwrap().remove(&uri);
        self.client.publish_diagnostics(uri, Vec::new(), None).await;
    }

    async fn hover(&self, params: HoverParams) -> LspResult<Option<Hover>> {
        let uri = params.text_document_position_params.text_document.uri;
        let pos = params.text_document_position_params.position;
        let docs = self.docs.lock().unwrap();
        let Some(doc) = docs.get(&uri) else { return Ok(None) };

        let line = (pos.line as usize) + 1;
        let col = (pos.character as usize) + 1;

        if let Some(r) = doc.index.ref_at(line, col) {
            let detail = match r.kind {
                RefKind::Function => match doc.index.decls.iter().find(|d| Some(d.pos) == r.decl_pos) {
                    Some(d) => d.detail.clone(),
                    None => format!("{}: {}", r.name, type_name(&r.ty)),
                },
                _ => format!("{}: {}", r.name, type_name(&r.ty)),
            };
            return Ok(Some(Hover {
                contents: HoverContents::Scalar(MarkedString::LanguageString(LanguageString {
                    language: "glide".into(),
                    value: detail,
                })),
                range: None,
            }));
        }

        if let Some(d) = decl_at(&doc.index, line, col) {
            return Ok(Some(Hover {
                contents: HoverContents::Scalar(MarkedString::LanguageString(LanguageString {
                    language: "glide".into(),
                    value: d.detail.clone(),
                })),
                range: None,
            }));
        }

        Ok(None)
    }

    async fn goto_definition(
        &self,
        params: GotoDefinitionParams,
    ) -> LspResult<Option<GotoDefinitionResponse>> {
        let uri = params.text_document_position_params.text_document.uri;
        let pos = params.text_document_position_params.position;
        let docs = self.docs.lock().unwrap();
        let Some(doc) = docs.get(&uri) else { return Ok(None) };

        let line = (pos.line as usize) + 1;
        let col = (pos.character as usize) + 1;

        let r = match doc.index.ref_at(line, col) {
            Some(r) => r,
            None => return Ok(None),
        };
        let Some(decl_pos) = r.decl_pos else { return Ok(None) };

        let target_line = decl_pos.line.saturating_sub(1) as u32;
        let target_col = decl_pos.column.saturating_sub(1) as u32;
        let name_len = r.name.chars().count() as u32;

        Ok(Some(GotoDefinitionResponse::Scalar(Location {
            uri,
            range: Range {
                start: Position { line: target_line, character: target_col },
                end: Position { line: target_line, character: target_col + name_len },
            },
        })))
    }

    async fn formatting(
        &self,
        params: DocumentFormattingParams,
    ) -> LspResult<Option<Vec<TextEdit>>> {
        let uri = params.text_document.uri;
        let docs = self.docs.lock().unwrap();
        let Some(doc) = docs.get(&uri) else { return Ok(None) };

        let program = match &doc.program {
            Some(p) => p,
            None => return Ok(None),
        };

        let formatted = glide_fmt::format(program);
        if formatted == doc.text {
            return Ok(Some(Vec::new()));
        }

        let end_line = doc.text.lines().count() as u32;
        let edits = vec![TextEdit {
            range: Range {
                start: Position { line: 0, character: 0 },
                end: Position { line: end_line + 1, character: 0 },
            },
            new_text: formatted,
        }];
        Ok(Some(edits))
    }

    async fn completion(
        &self,
        params: CompletionParams,
    ) -> LspResult<Option<CompletionResponse>> {
        let uri = params.text_document_position.text_document.uri;
        let pos = params.text_document_position.position;
        let docs = self.docs.lock().unwrap();
        let Some(doc) = docs.get(&uri) else { return Ok(None) };

        let line_idx = pos.line as usize;
        let col_idx = pos.character as usize;
        let line_text = doc.text.lines().nth(line_idx).unwrap_or("");
        let prefix: String = line_text.chars().take(col_idx).collect();

        let import_anchor = compute_import_anchor(&doc.text);

        let items = if let Some(ctx) = parse_member_access(&prefix) {
            field_completions(
                &doc.index,
                ctx,
                line_idx + 1,
                &doc.imported_modules,
                &import_anchor,
            )
        } else {
            symbol_completions(&doc.index, &doc.imported_modules, &import_anchor)
        };

        Ok(Some(CompletionResponse::Array(items)))
    }

    async fn shutdown(&self) -> LspResult<()> {
        Ok(())
    }
}

struct Analysis {
    diagnostics: Vec<Diagnostic>,
    program: Option<Program>,
    index: SymbolIndex,
    imported_modules: HashSet<String>,
}

fn analyze(source: &str, base_dir: Option<&Path>) -> Analysis {
    let mut parser = GlideParser::new(source);
    let parsed = parser.parse_program();
    let program = match parsed {
        Err(e) => {
            return Analysis {
                diagnostics: vec![make_diagnostic(e.line, e.column, &e.message)],
                program: None,
                index: SymbolIndex::default(),
                imported_modules: HashSet::new(),
            };
        }
        Ok(p) => p,
    };

    let mut typer = Typer::new();
    let mut imported_modules: HashSet<String> = HashSet::new();

    for stmt in &program {
        if let StmtKind::Import(p) = &stmt.kind {
            imported_modules.insert(p.clone());
        }
    }

    if let Some(dir) = base_dir {
        let mut loaded = HashSet::new();

        for (path, module_name) in discover_std_files(dir) {
            preload_module(&path, Some(module_name), &mut loaded, &mut typer);
        }

        for stmt in &program {
            if let StmtKind::Import(p) = &stmt.kind {
                if let Some(target) = resolve_import(dir, p) {
                    preload_module(&target, Some(p.clone()), &mut loaded, &mut typer);
                }
            }
        }
    }

    let (result, index) = typer.check(program);
    let (diagnostics, program) = match result {
        Ok(p) => (Vec::new(), Some(p)),
        Err(errors) => {
            let diags: Vec<_> = errors
                .iter()
                .map(|e| make_diagnostic(e.line, e.column, &e.message))
                .collect();
            (diags, None)
        }
    };

    Analysis { diagnostics, program, index, imported_modules }
}

fn preload_module(
    path: &Path,
    module: Option<String>,
    loaded: &mut HashSet<PathBuf>,
    typer: &mut Typer,
) {
    let canon = std::fs::canonicalize(path).unwrap_or_else(|_| path.to_path_buf());
    if !loaded.insert(canon) {
        return;
    }
    let Ok(source) = std::fs::read_to_string(path) else { return };
    let mut parser = GlideParser::new(&source);
    let Ok(program) = parser.parse_program() else { return };

    let dir = path.parent().map(Path::to_path_buf).unwrap_or_else(|| PathBuf::from("."));
    for stmt in &program {
        if let StmtKind::Import(p) = &stmt.kind {
            if let Some(target) = resolve_import(&dir, p) {
                preload_module(&target, Some(p.clone()), loaded, typer);
            }
        }
    }
    typer.pre_register_module(&program, module);
}

fn discover_std_files(start_dir: &Path) -> Vec<(PathBuf, String)> {
    let mut out = Vec::new();
    for ancestor in start_dir.ancestors() {
        let std_dir = ancestor.join("std");
        if std_dir.is_dir() {
            walk_glide_files(ancestor, &std_dir, &mut out);
            break;
        }
    }
    out
}

fn walk_glide_files(workspace_root: &Path, dir: &Path, out: &mut Vec<(PathBuf, String)>) {
    let Ok(entries) = std::fs::read_dir(dir) else { return };
    for entry in entries.flatten() {
        let path = entry.path();
        if path.is_dir() {
            walk_glide_files(workspace_root, &path, out);
        } else if path.extension().and_then(|e| e.to_str()) == Some("glide") {
            if let Ok(rel) = path.strip_prefix(workspace_root) {
                let module = rel
                    .with_extension("")
                    .to_string_lossy()
                    .replace('\\', "/");
                out.push((path.clone(), module));
            }
        }
    }
}

fn resolve_import(base_dir: &Path, import_path: &str) -> Option<PathBuf> {
    let p = Path::new(import_path);
    if p.is_absolute() {
        return Some(p.to_path_buf());
    }
    let direct = base_dir.join(p);
    if direct.is_file() {
        return Some(direct);
    }
    let with_ext = direct.with_extension("glide");
    if with_ext.is_file() {
        return Some(with_ext);
    }
    let mod_file = direct.join("mod.glide");
    if mod_file.is_file() {
        return Some(mod_file);
    }
    for ancestor in base_dir.ancestors() {
        let std_dir = ancestor.join("std");
        if !std_dir.is_dir() {
            continue;
        }
        let candidate = ancestor.join(p);
        if candidate.is_file() {
            return Some(candidate);
        }
        let with_ext = candidate.with_extension("glide");
        if with_ext.is_file() {
            return Some(with_ext);
        }
    }
    None
}

fn decl_at(index: &SymbolIndex, line: usize, col: usize) -> Option<&DeclInfo> {
    for d in &index.decls {
        if d.pos.line == line
            && col >= d.pos.column
            && col < d.pos.column + d.name.chars().count()
        {
            return Some(d);
        }
    }
    None
}

enum MemberContext {
    Ident { col: usize },
    Literal(Type),
}

struct ImportAnchor {
    line: u32,
    leading_blank: bool,
    trailing_blank: bool,
}

impl ImportAnchor {
    fn text_edit_for(&self, module: &str) -> TextEdit {
        let mut text = String::new();
        if self.leading_blank {
            text.push('\n');
        }
        text.push_str(&format!("import \"{}\";\n", module));
        if self.trailing_blank {
            text.push('\n');
        }
        TextEdit {
            range: Range {
                start: Position { line: self.line, character: 0 },
                end: Position { line: self.line, character: 0 },
            },
            new_text: text,
        }
    }
}

fn compute_import_anchor(text: &str) -> ImportAnchor {
    let lines: Vec<&str> = text.lines().collect();
    let mut last_import: Option<usize> = None;
    for (i, line) in lines.iter().enumerate() {
        if line.trim_start().starts_with("import ") {
            last_import = Some(i);
        } else if last_import.is_some() && !line.trim().is_empty() {
            break;
        }
    }

    if let Some(idx) = last_import {
        return ImportAnchor {
            line: (idx + 1) as u32,
            leading_blank: false,
            trailing_blank: false,
        };
    }

    let next_non_blank = lines
        .iter()
        .position(|l| !l.trim().is_empty());

    let trailing_blank = next_non_blank
        .map(|i| !lines.get(i).map_or(true, |l| l.trim().is_empty()))
        .unwrap_or(false);

    ImportAnchor {
        line: 0,
        leading_blank: false,
        trailing_blank,
    }
}

fn parse_member_access(prefix: &str) -> Option<MemberContext> {
    let trimmed = prefix.trim_end_matches(|c: char| c.is_alphanumeric() || c == '_');
    if !trimmed.ends_with('.') {
        return None;
    }
    let before_dot = trimmed[..trimmed.len() - 1].trim_end();

    if before_dot.ends_with('"') {
        return Some(MemberContext::Literal(Type::Named("string".into())));
    }
    if before_dot.ends_with('\'') {
        return Some(MemberContext::Literal(Type::Named("char".into())));
    }

    let bytes = before_dot.as_bytes();
    let mut end = before_dot.len();
    while end > 0 {
        let c = bytes[end - 1] as char;
        if c.is_alphanumeric() || c == '_' {
            end -= 1;
            continue;
        }
        break;
    }
    if end == before_dot.len() {
        return None;
    }
    let name = &before_dot[end..];

    if name == "true" || name == "false" {
        return Some(MemberContext::Literal(Type::Named("bool".into())));
    }
    if name.chars().all(|c| c.is_ascii_digit()) {
        return Some(MemberContext::Literal(Type::Named("int".into())));
    }

    let col = before_dot[..end].chars().count() + 1;
    Some(MemberContext::Ident { col })
}

fn field_completions(
    index: &SymbolIndex,
    ctx: MemberContext,
    line: usize,
    imported: &HashSet<String>,
    import_anchor: &ImportAnchor,
) -> Vec<CompletionItem> {
    let ty = match ctx {
        MemberContext::Ident { col } => match index.ref_at(line, col) {
            Some(r) => r.ty.clone(),
            None => return Vec::new(),
        },
        MemberContext::Literal(t) => t,
    };

    let mut items = Vec::new();
    let mut seen: HashSet<String> = HashSet::new();

    if let Some(s_name) = struct_from_type(&ty) {
        if let Some(fields) = index.structs.get(&s_name) {
            for f in fields {
                if seen.insert(f.name.clone()) {
                    items.push(CompletionItem {
                        label: f.name.clone(),
                        kind: Some(CompletionItemKind::FIELD),
                        detail: Some(type_name(&f.ty)),
                        ..Default::default()
                    });
                }
            }
        }
    }

    let full_prefix = ty.mangle();
    let stripped = method_type_prefix_for_completion(&ty);

    let prefixes: Vec<String> = {
        let mut v = Vec::new();
        v.push(format!("__glide_{}_", full_prefix));
        v.push(format!("{}_", full_prefix));
        if let Some(p) = &stripped {
            if p != &full_prefix {
                v.push(format!("__glide_{}_", p));
                v.push(format!("{}_", p));
            }
        }
        v
    };

    for d in &index.decls {
        if !matches!(d.kind, DeclKind::Fn) {
            continue;
        }
        for prefix in &prefixes {
            if let Some(rest) = d.name.strip_prefix(prefix.as_str()) {
                if !rest.is_empty() && seen.insert(rest.to_string()) {
                    items.push(method_completion(
                        rest,
                        &d.detail,
                        d.module.as_deref(),
                        imported,
                        import_anchor,
                    ));
                }
                break;
            }
        }
    }

    items
}

fn method_completion(
    name: &str,
    detail: &str,
    module: Option<&str>,
    imported: &HashSet<String>,
    import_anchor: &ImportAnchor,
) -> CompletionItem {
    let needs_import = module
        .filter(|m| !imported.contains(*m))
        .map(str::to_string);

    let display_detail = match (module, &needs_import) {
        (Some(m), Some(_)) => format!("{}  (auto-import \"{}\")", detail, m),
        (Some(m), None) => format!("{}  (from \"{}\")", detail, m),
        _ => detail.to_string(),
    };

    let additional_edits = needs_import
        .as_ref()
        .map(|m| vec![import_anchor.text_edit_for(m)]);

    CompletionItem {
        label: name.into(),
        kind: Some(CompletionItemKind::METHOD),
        detail: Some(display_detail),
        additional_text_edits: additional_edits,
        ..Default::default()
    }
}

fn struct_from_type(ty: &Type) -> Option<String> {
    match ty {
        Type::Named(n) => match n.as_str() {
            "int" | "float" | "bool" | "char" | "string" | "void" | "__null__" | "__error__" => None,
            other => Some(other.into()),
        },
        Type::Pointer(inner) => struct_from_type(inner),
        Type::Chan(_) => None,
    }
}

fn method_type_prefix_for_completion(ty: &Type) -> Option<String> {
    match ty {
        Type::Named(n) => Some(n.clone()),
        Type::Pointer(inner) => method_type_prefix_for_completion(inner),
        Type::Chan(_) => Some("chan".into()),
    }
}

fn symbol_completions(
    index: &SymbolIndex,
    imported: &HashSet<String>,
    import_anchor: &ImportAnchor,
) -> Vec<CompletionItem> {
    let mut items = Vec::new();

    for kw in KEYWORDS {
        items.push(CompletionItem {
            label: (*kw).into(),
            kind: Some(CompletionItemKind::KEYWORD),
            ..Default::default()
        });
    }
    for ty in PRIMITIVE_TYPES {
        items.push(CompletionItem {
            label: (*ty).into(),
            kind: Some(CompletionItemKind::CLASS),
            ..Default::default()
        });
    }

    let mut seen_globals: HashSet<String> = HashSet::new();
    for d in &index.decls {
        if d.name.starts_with("__glide_") || d.name.contains('_') && d.module.is_some() {
            // skip mangled stdlib + impl methods at the top level (only show via member access)
        }
        let kind = match d.kind {
            DeclKind::Fn => CompletionItemKind::FUNCTION,
            DeclKind::Struct => CompletionItemKind::STRUCT,
            DeclKind::Const => CompletionItemKind::CONSTANT,
            DeclKind::Let => CompletionItemKind::VARIABLE,
            DeclKind::Param => CompletionItemKind::VARIABLE,
        };
        if !matches!(d.kind, DeclKind::Fn | DeclKind::Struct | DeclKind::Const) {
            continue;
        }
        if d.name.starts_with("__glide_") {
            continue;
        }
        if d.name.contains('_') && d.module.is_some() && matches!(d.kind, DeclKind::Fn) {
            // looks like an impl method (e.g. string_blue) — skip in top-level completion
            continue;
        }
        if !seen_globals.insert(d.name.clone()) {
            continue;
        }
        let needs_import = d.module.as_deref()
            .filter(|m| !imported.contains(*m))
            .map(str::to_string);

        let display_detail = match (&d.module, &needs_import) {
            (Some(m), Some(_)) => format!("{}  (auto-import \"{}\")", d.detail, m),
            (Some(m), None) => format!("{}  (from \"{}\")", d.detail, m),
            _ => d.detail.clone(),
        };
        let additional_edits = needs_import
            .as_ref()
            .map(|m| vec![import_anchor.text_edit_for(m)]);

        items.push(CompletionItem {
            label: d.name.clone(),
            kind: Some(kind),
            detail: Some(display_detail),
            additional_text_edits: additional_edits,
            ..Default::default()
        });
    }
    items
}

const KEYWORDS: &[&str] = &[
    "fn", "let", "const", "struct",
    "interface", "impl", "import",
    "if", "else", "while", "for",
    "break", "continue", "return",
    "spawn", "chan",
    "true", "false", "null",
    "as", "sizeof", "new",
];

const PRIMITIVE_TYPES: &[&str] = &["int", "float", "bool", "char", "string", "void"];

fn make_diagnostic(line: usize, column: usize, message: &str) -> Diagnostic {
    let line = line.saturating_sub(1) as u32;
    let col = column.saturating_sub(1) as u32;
    Diagnostic {
        range: Range {
            start: Position { line, character: col },
            end: Position { line, character: col.saturating_add(1) },
        },
        severity: Some(DiagnosticSeverity::ERROR),
        code: None,
        code_description: None,
        source: Some("glide".to_string()),
        message: message.to_string(),
        related_information: None,
        tags: None,
        data: None,
    }
}

pub fn serve_stdio() {
    let rt = tokio::runtime::Builder::new_multi_thread()
        .enable_all()
        .build()
        .expect("failed to build tokio runtime");
    rt.block_on(async {
        let stdin = tokio::io::stdin();
        let stdout = tokio::io::stdout();
        let (service, socket) = LspService::new(Backend::new);
        Server::new(stdin, stdout, socket).serve(service).await;
    });
}
