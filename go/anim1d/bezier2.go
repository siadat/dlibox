// Copyright 2016 Marc-Antoine Ruel. All rights reserved.
// Use of this source code is governed under the Apache License, Version 2.0
// that can be found in the LICENSE file.

package anim1d

// cubicBezier returns [0, 65535] for input `x` based on the cubic bezier curve
// (x0, y0), (x1, y1).
func cubicBezier(x0, y0, x1, y1, x uint16) uint16 {
	/*
		t := uint32(x)
		for i := 0; i < 5; i++ {
			t2 := t * t
			t3 := t2 * t
			d := 1 - t
			d2 := d * d

			nx := 3*d2*t*x0 + 3*d*t2*x1 + t3
			dxdt := 3*d2*x0 + 6*d*t*(x1-x0) + 3*t2*(1-x1)
			if dxdt == 0 {
				break
			}

			t -= (nx - x) / dxdt
			if t <= 0 || t >= 1 {
				break
			}
		}
		if t < 0 {
			t = 0
		}
		if t > 1 {
			t = 1
		}

		// Solve for y using t.
		t2 := t * t
		t3 := t2 * t
		d := 1 - t
		d2 := d * d
		y := 3*d2*t*y0 + 3*d*t2*y1 + t3

		return y
	*/

}

// https://www.niksula.hut.fi/~hkankaan/Homepages/bezierfast.html
func cubicBezierIncremental(x0, y0, x1, y1, x float64, steps int) float64 {
	t = 1. / float64(steps)
	t2 := t * t
	f := x0
	fd := 3 * (y0 - x0) * t
	fdd_per_2 := 3 * (x0 - 2*y0 + x1) * t2
	fddd_per_2 := 3 * (3*(y0-x1) + y1 - x0) * t2 * t
	fddd := fddd_per_2 + fddd_per_2
	fdd := fdd_per_2 + fdd_per_2
	fddd_per_6 := fddd_per_2 * (1. / 3.)
	for loop := 0; loop < steps; loop++ {
		//yield loop/(steps), f
		//drawBezierpoint(f);
		f += fd + fdd_per_2 + fddd_per_6
		fd += fdd + fddd_per_2
		fdd += fddd
		fdd_per_2 += fddd_per_2
	}
}
