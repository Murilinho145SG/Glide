struct Node {
    val: i32,
    next: Option<Box<Node>>,
}

impl Drop for Node {
    fn drop(&mut self) {
        let mut cur = self.next.take();
        while let Some(mut n) = cur {
            cur = n.next.take();
        }
    }
}

fn main() {
    const N: i32 = 5_000_000;

    let mut head: Option<Box<Node>> = None;
    for i in 0..N {
        head = Some(Box::new(Node { val: i, next: head }));
    }

    let mut total: i32 = 0;
    let mut cur = head.as_deref();
    while let Some(c) = cur {
        total = total.wrapping_add(c.val);
        cur = c.next.as_deref();
    }

    drop(head);

    println!("alloc total = {}", total);
}
