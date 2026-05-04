package main

import "fmt"

func producer(c chan int, n int) {
	for i := 0; i < n; i++ {
		c <- i
	}
	close(c)
}

func main() {
	const n = 1000000
	c := make(chan int, 1024)
	go producer(c, n)
	sum := 0
	for v := range c {
		sum += v
	}
	fmt.Println("done sum=", sum)
}
