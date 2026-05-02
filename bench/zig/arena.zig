const std = @import("std");

const Node = struct {
    val: i32,
    next: ?*Node,
};

pub fn main() !void {
    const N: i32 = 5_000_000;

    var arena = std.heap.ArenaAllocator.init(std.heap.c_allocator);
    defer arena.deinit();
    const allocator = arena.allocator();

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

    std.debug.print("arena total = {d}\n", .{total});
}
