const std = @import("std");

const Node = struct {
    val: i32,
    next: ?*Node,
};

pub fn main() !void {
    const N: i32 = 5_000_000;
    const allocator = std.heap.c_allocator;

    var head: ?*Node = null;
    var i: i32 = 0;
    while (i < N) : (i += 1) {
        const n = try allocator.create(Node);
        n.* = .{ .val = i, .next = head };
        head = n;
    }

    var total: i32 = 0;
    var cur = head;
    while (cur) |c| {
        total +%= c.val;
        cur = c.next;
    }

    cur = head;
    while (cur) |c| {
        const nx = c.next;
        allocator.destroy(c);
        cur = nx;
    }

    std.debug.print("alloc total = {d}\n", .{total});
}
