package main

import "fmt"

type Node struct {
	val  int32
	next *Node
}

func main() {
	const N int32 = 5_000_000

	var head *Node = nil
	for i := int32(0); i < N; i++ {
		head = &Node{val: i, next: head}
	}

	var total int32 = 0
	for c := head; c != nil; c = c.next {
		total += c.val
	}

	fmt.Printf("alloc total = %d\n", total)
}
