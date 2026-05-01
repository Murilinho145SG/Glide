#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum Type {
    Named(String),
    Pointer(Box<Type>),
    Chan(Box<Type>),
}

impl Type {
    pub fn mangle(&self) -> String {
        match self {
            Type::Named(n) => n.clone(),
            Type::Pointer(inner) => format!("{}_ptr", inner.mangle()),
            Type::Chan(inner) => format!("chan_{}", inner.mangle()),
        }
    }
}
