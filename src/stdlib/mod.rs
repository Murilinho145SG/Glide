use crate::ast::Program;
use crate::parser::Parser;

const VECTOR_SRC: &str = include_str!("../../stdlib/vector.glide");
const HASHMAP_SRC: &str = include_str!("../../stdlib/hashmap.glide");

pub fn prelude() -> Program {
    let mut out = Vec::new();
    for (name, src) in [("vector", VECTOR_SRC), ("hashmap", HASHMAP_SRC)] {
        let mut parser = Parser::new(src);
        match parser.parse_program() {
            Ok(p) => out.extend(p),
            Err(e) => {
                panic!("internal error: stdlib/{}.glide failed to parse: {}", name, e);
            }
        }
    }
    out
}
