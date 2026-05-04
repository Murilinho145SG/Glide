package main

import (
	"fmt"
	"time"
)

func idle(c chan int) {
	<-c
}

func main() {
	const n = 100000
	c := make(chan int)
	for i := 0; i < n; i++ {
		go idle(c)
	}
	fmt.Println("spawned", n, "goroutines — sleeping 10s for RSS measurement")
	time.Sleep(10 * time.Second)
	fmt.Println("waking all")
	close(c)
}
