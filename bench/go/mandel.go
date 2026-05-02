package main

import "fmt"

const (
	W        = 4096
	H        = 4096
	MAX_ITER = 200
)

func mandel(cr, ci float64) int32 {
	var zr, zi float64 = 0.0, 0.0
	for i := int32(0); i < MAX_ITER; i++ {
		zr2 := zr * zr
		zi2 := zi * zi
		if zr2+zi2 > 4.0 {
			return i
		}
		newZr := zr2 - zi2 + cr
		zi = 2.0*zr*zi + ci
		zr = newZr
	}
	return MAX_ITER
}

func main() {
	var total int32 = 0
	for y := int32(0); y < H; y++ {
		ci := (float64(y)/float64(H))*2.0 - 1.0
		for x := int32(0); x < W; x++ {
			cr := (float64(x)/float64(W))*3.0 - 2.0
			total += mandel(cr, ci)
		}
	}
	fmt.Printf("mandel total = %d\n", total)
}
