package main

import "fmt"

func worker(id int, c chan int) {
	c <- id
}

func main() {
	const n = 100000
	c := make(chan int, 1024)
	for i := 0; i < n; i++ {
		go worker(i, c)
	}
	sum := 0
	for i := 0; i < n; i++ {
		sum += <-c
	}
	fmt.Println("sum", sum)
}
