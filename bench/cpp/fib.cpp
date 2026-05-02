#include <cstdio>

static int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int main() {
    int r = fib(40);
    std::printf("fib(40) = %d\n", r);
    return 0;
}
