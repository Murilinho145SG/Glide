package main

import "sync"

func worker(wg *sync.WaitGroup) {
	defer wg.Done()
}

func main() {
	const n = 100000
	var wg sync.WaitGroup
	wg.Add(n)
	for i := 0; i < n; i++ {
		go worker(&wg)
	}
	wg.Wait()
}
