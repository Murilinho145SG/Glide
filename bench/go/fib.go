package main

import "fmt"

func fib(n int32) int32 {
	if n < 2 {
		return n
	}
	return fib(n-1) + fib(n-2)
}

func main() {
	r := fib(40)
	fmt.Printf("fib(40) = %d\n", r)
}
