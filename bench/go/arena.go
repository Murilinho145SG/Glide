package main

import "fmt"

type Node struct {
	val  int32
	next *Node
}

func main() {
	const N int32 = 5_000_000

	nodes := make([]Node, N)
	var head *Node = nil
	for i := int32(0); i < N; i++ {
		n := &nodes[i]
		n.val = i
		n.next = head
		head = n
	}

	var total int32 = 0
	for c := head; c != nil; c = c.next {
		total += c.val
	}

	fmt.Printf("arena total = %d\n", total)
}
