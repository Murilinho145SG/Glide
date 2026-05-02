#include <stdio.h>

#define W 4096
#define H 4096
#define MAX_ITER 200

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

int main(void) {
    int total = 0;
    for (int y = 0; y < H; y++) {
        double ci = ((double)y / (double)H) * 2.0 - 1.0;
        for (int x = 0; x < W; x++) {
            double cr = ((double)x / (double)W) * 3.0 - 2.0;
            total += mandel(cr, ci);
        }
    }
    printf("mandel total = %d\n", total);
    return 0;
}
