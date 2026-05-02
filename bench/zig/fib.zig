const std = @import("std");

fn fib(n: i32) i32 {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

pub fn main() void {
    const r = fib(40);
    std.debug.print("fib(40) = {d}\n", .{r});
}
