use glide::codegen::{Codegen, EmitResult};
use glide::fmt;
use glide::parser::Parser;
use glide::typer::Typer;
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
            let result = compile_to_c(&file);
            invoke_gcc(&result, Path::new(&output));
            eprintln!("compiled {} -> {}", file, output);
        }
        "run" => {
            let file = require_file_arg(&argv, "run");
            let result = compile_to_c(&file);
            let exe = temp_exe_path(&file);
            invoke_gcc(&result, &exe);
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
    eprintln!("  emit  <file>             generate C source to stdout");
    eprintln!("  build <file> [-o out]    compile to a native executable");
    eprintln!("  run   <file>             build and run, forwarding the exit code");
    eprintln!("  fmt   <file> [--write]   pretty-print to stdout, or rewrite the file");
    eprintln!("  lsp                      start the LSP server on stdio");
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
    let source = std::fs::read_to_string(filename).unwrap_or_else(|e| {
        eprintln!("glide: could not read {}: {}", filename, e);
        std::process::exit(1);
    });

    let mut parser = Parser::new(&source);
    let program = match parser.parse_program() {
        Ok(p) => p,
        Err(e) => {
            eprintln!("{}: {}", filename, e);
            std::process::exit(1);
        }
    };

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

fn invoke_gcc(result: &EmitResult, output: &Path) {
    let tmp_dir = std::env::temp_dir();
    let c_path = tmp_dir.join(format!("glide-{}.c", std::process::id()));
    if let Err(e) = std::fs::write(&c_path, &result.c_source) {
        eprintln!("glide: failed to write temp C file {}: {}", c_path.display(), e);
        std::process::exit(1);
    }

    let mut cmd = Command::new("gcc");
    cmd.arg(&c_path).arg("-o").arg(output);
    if result.needs_pthread {
        cmd.arg("-lpthread");
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
