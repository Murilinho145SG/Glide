const std = @import("std");

const W: i32 = 4096;
const H: i32 = 4096;
const MAX_ITER: i32 = 200;

fn mandel(cr: f64, ci: f64) i32 {
    var zr: f64 = 0.0;
    var zi: f64 = 0.0;
    var i: i32 = 0;
    while (i < MAX_ITER) : (i += 1) {
        const zr2 = zr * zr;
        const zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) return i;
        const new_zr = zr2 - zi2 + cr;
        zi = 2.0 * zr * zi + ci;
        zr = new_zr;
    }
    return MAX_ITER;
}

pub fn main() void {
    var total: i32 = 0;
    var y: i32 = 0;
    while (y < H) : (y += 1) {
        const ci = (@as(f64, @floatFromInt(y)) / @as(f64, @floatFromInt(H))) * 2.0 - 1.0;
        var x: i32 = 0;
        while (x < W) : (x += 1) {
            const cr = (@as(f64, @floatFromInt(x)) / @as(f64, @floatFromInt(W))) * 3.0 - 2.0;
            total +%= mandel(cr, ci);
        }
    }
    std.debug.print("mandel total = {d}\n", .{total});
}
