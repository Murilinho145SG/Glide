const W: i32 = 4096;
const H: i32 = 4096;
const MAX_ITER: i32 = 200;

fn mandel(cr: f64, ci: f64) -> i32 {
    let mut zr = 0.0f64;
    let mut zi = 0.0f64;
    for i in 0..MAX_ITER {
        let zr2 = zr * zr;
        let zi2 = zi * zi;
        if zr2 + zi2 > 4.0 { return i; }
        let new_zr = zr2 - zi2 + cr;
        zi = 2.0 * zr * zi + ci;
        zr = new_zr;
    }
    MAX_ITER
}

fn main() {
    let mut total: i32 = 0;
    for y in 0..H {
        let ci = (y as f64 / H as f64) * 2.0 - 1.0;
        for x in 0..W {
            let cr = (x as f64 / W as f64) * 3.0 - 2.0;
            total = total.wrapping_add(mandel(cr, ci));
        }
    }
    println!("mandel total = {}", total);
}
