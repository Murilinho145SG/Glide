#include <stdio.h>

static int fib(int n) {
    if (n < 2) return n;
    return fib(n - 1) + fib(n - 2);
}

int main(void) {
    int r = fib(40);
    printf("fib(40) = %d\n", r);
    return 0;
}
