use crate::ast::Program;
use crate::fmt as glide_fmt;
use crate::parser::Parser as GlideParser;
use crate::typer::{type_name, DeclInfo, DeclKind, RefKind, SymbolIndex, Typer};
use crate::types::Type;
use std::collections::HashMap;
use std::sync::Mutex;
use tower_lsp::jsonrpc::Result as LspResult;
use tower_lsp::lsp_types::*;
use tower_lsp::{Client, LanguageServer, LspService, Server};

struct DocumentState {
    text: String,
    program: Option<Program>,
    index: SymbolIndex,
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
        let (diagnostics, program, index) = analyze(&source);
        self.docs.lock().unwrap().insert(
            uri.clone(),
            DocumentState { text: source, program, index },
        );
        self.client.publish_diagnostics(uri, diagnostics, None).await;
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

        let items = if let Some((obj_name, obj_col)) = parse_member_access(&prefix) {
            field_completions(&doc.index, obj_name, obj_col, line_idx + 1)
        } else {
            symbol_completions(&doc.index)
        };

        Ok(Some(CompletionResponse::Array(items)))
    }

    async fn shutdown(&self) -> LspResult<()> {
        Ok(())
    }
}

fn analyze(source: &str) -> (Vec<Diagnostic>, Option<Program>, SymbolIndex) {
    let mut parser = GlideParser::new(source);
    let parsed = parser.parse_program();
    match parsed {
        Err(e) => (
            vec![make_diagnostic(e.line, e.column, &e.message)],
            None,
            SymbolIndex::default(),
        ),
        Ok(program) => {
            let typer = Typer::new();
            let (result, index) = typer.check(program);
            match result {
                Ok(p) => (Vec::new(), Some(p), index),
                Err(errors) => {
                    let diags: Vec<_> = errors
                        .iter()
                        .map(|e| make_diagnostic(e.line, e.column, &e.message))
                        .collect();
                    (diags, None, index)
                }
            }
        }
    }
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

fn parse_member_access(prefix: &str) -> Option<(String, usize)> {
    let trimmed = prefix.trim_end();
    if !trimmed.ends_with('.') {
        return None;
    }
    let before_dot = &trimmed[..trimmed.len() - 1];
    let mut end = before_dot.len();
    let bytes = before_dot.as_bytes();
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
    let name = before_dot[end..].to_string();
    let col = before_dot[..end].chars().count() + 1;
    Some((name, col))
}

fn field_completions(
    index: &SymbolIndex,
    obj_name: String,
    obj_col: usize,
    line: usize,
) -> Vec<CompletionItem> {
    let mut struct_name: Option<String> = None;

    if let Some(r) = index.ref_at(line, obj_col) {
        struct_name = struct_from_type(&r.ty);
    }

    let Some(s_name) = struct_name else {
        let _ = obj_name;
        return Vec::new();
    };
    let Some(fields) = index.structs.get(&s_name) else {
        return Vec::new();
    };

    fields
        .iter()
        .map(|f| CompletionItem {
            label: f.name.clone(),
            kind: Some(CompletionItemKind::FIELD),
            detail: Some(type_name(&f.ty)),
            ..Default::default()
        })
        .collect()
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

fn symbol_completions(index: &SymbolIndex) -> Vec<CompletionItem> {
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
    for d in &index.decls {
        let kind = match d.kind {
            DeclKind::Fn => CompletionItemKind::FUNCTION,
            DeclKind::Struct => CompletionItemKind::STRUCT,
            DeclKind::Const => CompletionItemKind::CONSTANT,
            DeclKind::Let => CompletionItemKind::VARIABLE,
            DeclKind::Param => CompletionItemKind::VARIABLE,
        };
        if matches!(d.kind, DeclKind::Fn | DeclKind::Struct | DeclKind::Const) {
            items.push(CompletionItem {
                label: d.name.clone(),
                kind: Some(kind),
                detail: Some(d.detail.clone()),
                ..Default::default()
            });
        }
    }
    items
}

const KEYWORDS: &[&str] = &[
    "fn", "let", "const", "struct",
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
