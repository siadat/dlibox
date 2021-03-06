// Copyright 2016 Marc-Antoine Ruel. All rights reserved.
// Use of this source code is governed under the Apache License, Version 2.0
// that can be found in the LICENSE file.

package anim1d

// Color shows a single color on all lights. It knows how to renders itself
// into a frame.
//
// If you want a single dot, use a Frame of length one.
type Color struct {
	R, G, B uint8
}

// Add adds two color together with saturation, mixing according to the alpha
// channel.
func (c *Color) Add(d Color) {
	r := uint16(c.R) + uint16(d.R)
	if r > 255 {
		r = 255
	}
	c.R = uint8(r)
	g := uint16(c.G) + uint16(d.G)
	if g > 255 {
		g = 255
	}
	c.G = uint8(g)
	b := uint16(c.B) + uint16(d.B)
	if b > 255 {
		b = 255
	}
	c.B = uint8(b)
}

// Mix blends the second color with the first.
//
// gradient 0 means pure 'c', gradient 255 means pure 'd'.
func (c *Color) Mix(d Color, gradient uint8) {
	grad := uint16(gradient)
	grad1 := 255 - grad
	// unit test confirms the values cannot overflow.
	c.R = uint8(((uint16(c.R)+1)*grad1 + (uint16(d.R)+1)*grad) >> 8)
	c.G = uint8(((uint16(c.G)+1)*grad1 + (uint16(d.G)+1)*grad) >> 8)
	c.B = uint8(((uint16(c.B)+1)*grad1 + (uint16(d.B)+1)*grad) >> 8)
}

func (c *Color) NextFrame(pixels Frame, timeMS uint32) {
	for i := range pixels {
		pixels[i] = *c
	}
}

// Frame is a strip of colors. It knows how to renders itself into a frame
// (which is recursive).
type Frame []Color

func (f Frame) NextFrame(pixels Frame, timeMS uint32) {
	copy(pixels, f)
}

// Mix blends the second frame with the first.
//
// gradient 0 means pure 'f', gradient 255 means pure 'b'.
func (f Frame) Mix(b Frame, gradient uint8) {
	for i := range f {
		f[i].Mix(b[i], gradient)
	}
}

// reset() always resets the buffer to black.
func (f *Frame) reset(l int) {
	if len(*f) != l {
		*f = make(Frame, l)
	} else {
		s := *f
		for i := range s {
			s[i] = Color{}
		}
	}
}

func (f Frame) isEqual(rhs Frame) bool {
	if len(f) != len(rhs) {
		return false
	}
	for j, p := range f {
		if p != rhs[j] {
			return false
		}
	}
	return true
}

// MakeRainbow renders rainbow colors.
type Rainbow struct {
	buf Frame
}

func (r *Rainbow) NextFrame(pixels Frame, timeMS uint32) {
	if len(r.buf) != len(pixels) {
		r.buf.reset(len(pixels))
		const start = 380
		const end = 781
		const delta = end - start
		scale := logn(2)
		step := 1. / float32(len(pixels))
		for i := range pixels {
			j := log1p(float32(len(pixels)-i-1)*step) / scale
			r.buf[i] = waveLength2RGB(int(start + delta*(1-j)))
		}
	}
	copy(pixels, r.buf)
}

// waveLengthToRGB returns a color over a rainbow.
//
// This code was inspired by public domain code on the internet.
func waveLength2RGB(w int) (c Color) {
	switch {
	case w < 380:
	case w < 420:
		// Red peaks at 1/3 at 420.
		c.R = uint8(196 - (170*(440-w))/(440-380))
		c.B = uint8(26 + (229*(w-380))/(420-380))
	case w < 440:
		c.R = uint8((0x89 * (440 - w)) / (440 - 420))
		c.B = 255
	case w < 490:
		c.G = uint8((255 * (w - 440)) / (490 - 440))
		c.B = 255
	case w < 510:
		c.G = 255
		c.B = uint8((255 * (510 - w)) / (510 - 490))
	case w < 580:
		c.R = uint8((255 * (w - 510)) / (580 - 510))
		c.G = 255
	case w < 645:
		c.R = 255
		c.G = uint8((255 * (645 - w)) / (645 - 580))
	case w < 700:
		c.R = 255
	case w < 781:
		c.R = uint8(26 + (229*(780-w))/(780-700))
	default:
	}
	return
}

// Repeated repeats a Frame to fill the pixels.
type Repeated struct {
	Frame Frame
}

func (r *Repeated) NextFrame(pixels Frame, timeMS uint32) {
	if len(pixels) == 0 || len(r.Frame) == 0 {
		return
	}
	for i := 0; i < len(pixels); i += len(r.Frame) {
		copy(pixels[i:], r.Frame)
	}
}
