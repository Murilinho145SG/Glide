use std::ptr;

struct Arena {
    buf: Vec<u8>,
    used: usize,
}

impl Arena {
    fn new(cap: usize) -> Self {
        Self { buf: vec![0u8; cap], used: 0 }
    }
    fn alloc<T>(&mut self) -> *mut T {
        let align = std::mem::align_of::<T>();
        let size = std::mem::size_of::<T>();
        let off = (self.used + align - 1) & !(align - 1);
        if off + size > self.buf.len() { panic!("arena oom"); }
        let p = unsafe { self.buf.as_mut_ptr().add(off) as *mut T };
        self.used = off + size;
        p
    }
}

struct Node {
    val: i32,
    next: *mut Node,
}

fn main() {
    const N: i32 = 5_000_000;
    let mut a = Arena::new(N as usize * 16 + 64);

    let mut head: *mut Node = ptr::null_mut();
    for i in 0..N {
        let n = a.alloc::<Node>();
        unsafe {
            (*n).val = i;
            (*n).next = head;
        }
        head = n;
    }

    let mut total: i32 = 0;
    let mut cur = head;
    while !cur.is_null() {
        unsafe {
            total = total.wrapping_add((*cur).val);
            cur = (*cur).next;
        }
    }

    println!("arena total = {}", total);
}
