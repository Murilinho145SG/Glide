#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum Type {
    Named(String),
    Pointer(Box<Type>),
    Borrow(Box<Type>),
    BorrowMut(Box<Type>),
    Chan(Box<Type>),
    Slice(Box<Type>),
    FnPtr {
        params: Vec<Type>,
        ret: Option<Box<Type>>,
    },
}

impl Type {
    pub fn mangle(&self) -> String {
        match self {
            Type::Named(n) => n.clone(),
            Type::Pointer(inner) => format!("{}_ptr", inner.mangle()),
            Type::Borrow(inner) => format!("{}_ref", inner.mangle()),
            Type::BorrowMut(inner) => format!("{}_refmut", inner.mangle()),
            Type::Chan(inner) => format!("chan_{}", inner.mangle()),
            Type::Slice(inner) => format!("slice_{}", inner.mangle()),
            Type::FnPtr { params, ret } => {
                let p = params.iter().map(|t| t.mangle()).collect::<Vec<_>>().join("_");
                let r = ret.as_ref().map(|t| t.mangle()).unwrap_or_else(|| "void".into());
                format!("fn_{}_to_{}", p, r)
            }
        }
    }
}
