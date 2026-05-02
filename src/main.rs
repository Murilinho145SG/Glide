use glide::ast::{Program, StmtKind};
use glide::codegen::{Codegen, EmitResult};
use glide::fmt;
use glide::parser::Parser;
use glide::typer::Typer;
use std::collections::HashSet;
use std::path::{Path, PathBuf};
use std::process::Command;

fn main() {
    let argv: Vec<String> = std::env::args().collect();

    if argv.len() < 2 {
        print_usage();
        std::process::exit(1);
    }

    match argv[1].as_str() {
        "--help" | "-h" | "help" => {
            print_usage();
        }
        "emit" => {
            let file = require_file_arg(&argv, "emit");
            let result = compile_to_c(&file);
            print!("{}", result.c_source);
        }
        "build" => {
            let file = require_file_arg(&argv, "build");
            let output = parse_o_flag(&argv).unwrap_or_else(|| default_output_name(&file));
            let mode = parse_build_mode(&argv);
            let result = compile_to_c(&file);
            invoke_gcc(&result, Path::new(&output), mode);
            eprintln!("compiled {} -> {}", file, output);
        }
        "run" => {
            let file = require_file_arg(&argv, "run");
            let mode = parse_build_mode(&argv);
            let result = compile_to_c(&file);
            let exe = temp_exe_path(&file);
            invoke_gcc(&result, &exe, mode);
            let status = Command::new(&exe).status().unwrap_or_else(|e| {
                eprintln!("glide: failed to run {}: {}", exe.display(), e);
                let _ = std::fs::remove_file(&exe);
                std::process::exit(1);
            });
            let _ = std::fs::remove_file(&exe);
            std::process::exit(status.code().unwrap_or(0));
        }
        "fmt" => {
            run_fmt(&argv);
        }
        "lsp" => {
            glide::lsp::serve_stdio();
        }
        unknown => {
            eprintln!("glide: unknown subcommand `{}`\n", unknown);
            print_usage();
            std::process::exit(1);
        }
    }
}

fn print_usage() {
    eprintln!("usage: glide <subcommand> [args]");
    eprintln!();
    eprintln!("subcommands:");
    eprintln!("  emit  <file>                       generate C source to stdout");
    eprintln!("  build <file> [-o out] [--release]  compile to a native executable");
    eprintln!("  run   <file> [--release]           build and run, forwarding the exit code");
    eprintln!("  fmt   <file> [--write]             pretty-print to stdout, or rewrite the file");
    eprintln!("  lsp                                start the LSP server on stdio");
    eprintln!();
    eprintln!("build flags:");
    eprintln!("  --release   use -O3 -flto -DNDEBUG");
    eprintln!("  --debug     use -O0 -g");
}

fn require_file_arg(argv: &[String], sub: &str) -> String {
    match argv.get(2) {
        Some(s) if !s.starts_with('-') => s.clone(),
        _ => {
            eprintln!("glide: `{}` requires a source file argument", sub);
            std::process::exit(1);
        }
    }
}

fn run_fmt(argv: &[String]) {
    let file = require_file_arg(argv, "fmt");
    let write_in_place = argv.iter().skip(2).any(|a| a == "--write" || a == "-w");

    let source = std::fs::read_to_string(&file).unwrap_or_else(|e| {
        eprintln!("glide: could not read {}: {}", file, e);
        std::process::exit(1);
    });

    let mut parser = Parser::new(&source);
    let program = match parser.parse_program() {
        Ok(p) => p,
        Err(e) => {
            eprintln!("{}: {}", file, e);
            std::process::exit(1);
        }
    };

    let formatted = fmt::format(&program);

    if write_in_place {
        if formatted == source {
            return;
        }
        if let Err(e) = std::fs::write(&file, &formatted) {
            eprintln!("glide: could not write {}: {}", file, e);
            std::process::exit(1);
        }
    } else {
        print!("{}", formatted);
    }
}

fn parse_o_flag(argv: &[String]) -> Option<String> {
    let mut i = 0;
    while i < argv.len() {
        if argv[i] == "-o" {
            return argv.get(i + 1).cloned();
        }
        i += 1;
    }
    None
}

fn compile_to_c(filename: &str) -> EmitResult {
    let mut loaded = HashSet::new();
    let program = load_program_with_origin(Path::new(filename), &mut loaded, true);

    let typer = Typer::new();
    let (result, _index) = typer.check(program);
    let typed = match result {
        Ok(p) => p,
        Err(errors) => {
            for e in &errors {
                eprintln!("{}: {}", filename, e);
            }
            eprintln!(
                "({} type error{})",
                errors.len(),
                if errors.len() == 1 { "" } else { "s" }
            );
            std::process::exit(1);
        }
    };

    let codegen = Codegen::new();
    match codegen.emit_program(&typed) {
        Ok(r) => r,
        Err(e) => {
            eprintln!("{}: {}", filename, e);
            std::process::exit(1);
        }
    }
}

fn load_program_with_origin(path: &Path, loaded: &mut HashSet<PathBuf>, is_root: bool) -> Program {
    let canon = std::fs::canonicalize(path).unwrap_or_else(|_| path.to_path_buf());
    if !loaded.insert(canon.clone()) {
        return Vec::new();
    }

    let source = std::fs::read_to_string(path).unwrap_or_else(|e| {
        eprintln!("glide: could not read {}: {}", path.display(), e);
        std::process::exit(1);
    });

    let mut parser = Parser::new(&source);
    let parsed = match parser.parse_program() {
        Ok(p) => p,
        Err(e) => {
            eprintln!("{}: {}", path.display(), e);
            std::process::exit(1);
        }
    };

    let dir = path.parent().map(Path::to_path_buf).unwrap_or_else(|| PathBuf::from("."));
    let source_file = if is_root { None } else { Some(canon) };
    let mut out = Vec::new();
    for mut stmt in parsed {
        if let StmtKind::Import(p) = &stmt.kind {
            let target = resolve_import(&dir, p);
            let sub = load_program_with_origin(&target, loaded, false);
            out.extend(sub);
        } else {
            stmt.source_file = source_file.clone();
            out.push(stmt);
        }
    }
    out
}

fn resolve_import(base_dir: &Path, import_path: &str) -> PathBuf {
    let p = Path::new(import_path);
    if p.is_absolute() {
        return p.to_path_buf();
    }
    let direct = base_dir.join(p);
    if direct.extension().is_some() {
        return direct;
    }
    let with_ext = direct.with_extension("glide");
    if with_ext.exists() {
        return with_ext;
    }
    let as_dir_mod = direct.join("mod.glide");
    if as_dir_mod.exists() {
        return as_dir_mod;
    }
    if let Ok(exe) = std::env::current_exe() {
        if let Some(root) = exe.parent().and_then(|d| d.ancestors().find(|p| p.join("std").is_dir())) {
            let std_path = root.join(p);
            if std_path.exists() {
                return std_path;
            }
            let std_with_ext = std_path.with_extension("glide");
            if std_with_ext.exists() {
                return std_with_ext;
            }
        }
    }
    if let Some(repo_root) = find_repo_root_with_std() {
        let std_path = repo_root.join(p);
        if std_path.exists() {
            return std_path;
        }
        let std_with_ext = std_path.with_extension("glide");
        if std_with_ext.exists() {
            return std_with_ext;
        }
    }
    with_ext
}

fn find_repo_root_with_std() -> Option<PathBuf> {
    let cwd = std::env::current_dir().ok()?;
    for ancestor in cwd.ancestors() {
        if ancestor.join("std").is_dir() && ancestor.join("std").join("mod.glide").exists() {
            return Some(ancestor.to_path_buf());
        }
        if ancestor.join("std").is_dir() {
            return Some(ancestor.to_path_buf());
        }
    }
    None
}

#[derive(Clone, Copy)]
enum BuildMode {
    Default,
    Release,
    Debug,
}

fn parse_build_mode(argv: &[String]) -> BuildMode {
    if argv.iter().any(|a| a == "--release") {
        BuildMode::Release
    } else if argv.iter().any(|a| a == "--debug") {
        BuildMode::Debug
    } else {
        BuildMode::Default
    }
}

fn invoke_gcc(result: &EmitResult, output: &Path, mode: BuildMode) {
    let tmp_dir = std::env::temp_dir();
    let c_path = tmp_dir.join(format!("glide-{}.c", std::process::id()));
    if let Err(e) = std::fs::write(&c_path, &result.c_source) {
        eprintln!("glide: failed to write temp C file {}: {}", c_path.display(), e);
        std::process::exit(1);
    }

    let mut cmd = Command::new("gcc");
    cmd.arg(&c_path);
    match mode {
        BuildMode::Default => { cmd.arg("-O2"); }
        BuildMode::Release => { cmd.args(["-O3", "-flto", "-DNDEBUG"]); }
        BuildMode::Debug   => { cmd.args(["-O0", "-g"]); }
    }
    cmd.arg("-o").arg(output);
    if result.needs_pthread {
        cmd.arg("-lpthread");
    }
    for lib in &result.link_libs {
        cmd.arg(format!("-l{}", lib));
    }

    let status = match cmd.status() {
        Ok(s) => s,
        Err(e) => {
            eprintln!("glide: failed to invoke gcc: {} (is gcc on your PATH?)", e);
            let _ = std::fs::remove_file(&c_path);
            std::process::exit(1);
        }
    };
    let _ = std::fs::remove_file(&c_path);

    if !status.success() {
        eprintln!("glide: gcc exited with status {}", status);
        std::process::exit(status.code().unwrap_or(1));
    }
}

fn temp_exe_path(source_file: &str) -> PathBuf {
    let stem = Path::new(source_file)
        .file_stem()
        .map(|s| s.to_string_lossy().to_string())
        .unwrap_or_else(|| "out".to_string());
    let name = format!("glide-{}-{}{}", std::process::id(), stem, exe_suffix());
    std::env::temp_dir().join(name)
}

fn default_output_name(source_file: &str) -> String {
    let stem = Path::new(source_file)
        .file_stem()
        .map(|s| s.to_string_lossy().to_string())
        .unwrap_or_else(|| "out".to_string());
    format!("{}{}", stem, exe_suffix())
}

fn exe_suffix() -> &'static str {
    if cfg!(windows) { ".exe" } else { "" }
}
