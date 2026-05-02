#include <cstdio>

constexpr int W = 4096;
constexpr int H = 4096;
constexpr int MAX_ITER = 200;

static int mandel(double cr, double ci) {
    double zr = 0.0, zi = 0.0;
    for (int i = 0; i < MAX_ITER; i++) {
        double zr2 = zr * zr;
        double zi2 = zi * zi;
        if (zr2 + zi2 > 4.0) return i;
        double new_zr = zr2 - zi2 + cr;
        zi = 2.0 * zr * zi + ci;
        zr = new_zr;
    }
    return MAX_ITER;
}

int main() {
    int total = 0;
    for (int y = 0; y < H; y++) {
        double ci = (static_cast<double>(y) / H) * 2.0 - 1.0;
        for (int x = 0; x < W; x++) {
            double cr = (static_cast<double>(x) / W) * 3.0 - 2.0;
            total += mandel(cr, ci);
        }
    }
    std::printf("mandel total = %d\n", total);
    return 0;
}
