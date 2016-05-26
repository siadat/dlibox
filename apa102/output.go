// Copyright 2016 Marc-Antoine Ruel. All rights reserved.
// Use of this source code is governed under the Apache License, Version 2.0
// that can be found in the LICENSE file.

package apa102

import (
	"image/color"
	"io"
	"time"

	"github.com/maruel/dotstar/anim1d"
	"github.com/maruel/dotstar/rpi"
)

type DotStar struct {
	// Gamma correction then power limiter.
	RedGamma   float32
	RedMax     float32
	GreenGamma float32
	GreenMax   float32
	BlueGamma  float32
	BlueMax    float32
	AmpPerLED  float32
	AmpBudget  float32

	w   io.WriteCloser
	buf []byte
}

func (d *DotStar) Close() error {
	w := d.w
	d.w = nil
	return w.Close()
}

const maxIn = float32(0xFFFF)
const maxOut = float32(0x1EE1)
const lowCut = 30 * 255

// TODO(maruel): The +10 is completely random and needs to be properly
// calculated.
const rampOffset = (float32(lowCut)/255. + 10.) / maxIn
const lowCutf = float32(lowCut) / maxIn
const klow = lowCutf*lowCutf*lowCutf + rampOffset

// Converts input from [0, 0xFFFF] as intensity to lightness on a scale of
// [0, 0x1EE1].
//
// It's the reverse of lightness; https://en.wikipedia.org/wiki/Lightness
func processRamp(l uint32) uint32 {
	// Linear [0->0] to [30*255->30].
	if l < lowCut {
		return uint32(float32(l)/255. + 0.4)
	}
	// Range [lowCut/maxIn, 1]
	y := float32(l) / maxIn
	y = y * y * y
	// Range [(lowCut/maxIn)^3, 1]. We need to realign to [lowCut/255, 1] then
	// scale to maxOut.
	return uint32((y+klow)/(1+klow)*maxOut + 0.4)
}

// ColorToAPA102 converts a color into the 4 bytes needed to control an APA-102
// LED.
//
// The response as seen by the human eye is very non-linear. The APA-102
// provides an overall brightness PWM but it is relatively slower and results
// in human visible flicker. On the other hand the minimal color (1/255) is
// still too intense at full brightness, so for very dark color, it is worth
// using the overall brightness PWM. The goal is to use brightness!=31 as
// little as possible.
//
// Global brightness frequency is 580Hz and color frequency at 19.2kHz.
// https://cpldcpu.wordpress.com/2014/08/27/apa102/
// Both are multiplicative, so brightness@50% and color@50% means an effective
// 25% duty cycle but it is not properly distributed, which is the main problem.
//
// It is unclear to me if brightness is exactly in 1/31 increment as I don't
// have an oscilloscope to confirm. Same for color in 1/255 increment.
//
// Each channel duty cycle ramps from 100% to 1/(31*255) == 1/7905.
//
// Return brighness, blue, green, red.
func ColorToAPA102(c color.NRGBA) (byte, byte, byte, byte) {
	// Evaluate alpha.
	r, g, b, _ := c.RGBA()

	r2 := processRamp(r)
	g2 := processRamp(g)
	b2 := processRamp(b)
	if r2 <= 255 && g2 <= 255 && b2 <= 255 {
		return byte(0xE0 + 1), byte(b2), byte(g2), byte(r2)
	} else if r2 <= 511 && g2 <= 511 && b2 <= 511 {
		return byte(0xE0 + 2), byte(b2 >> 1), byte(g2 >> 1), byte(r2 >> 1)
	} else if r2 <= 1023 && g2 <= 1023 && b2 <= 1023 {
		return byte(0xE0 + 4), byte((b2 + 2) >> 2), byte((g2 + 2) >> 2), byte((r2 + 2) >> 2)
	} else {
		// In this case we need to use a ramp of 255-1 even for lower colors.
		return byte(0xE0 + 31), byte((b2 + 15) / 31), byte((g2 + 15) / 31), byte((r2 + 15) / 31)
	}
}

func (d *DotStar) Write(pixels []color.NRGBA) error {
	// https://cpldcpu.files.wordpress.com/2014/08/apa-102c-super-led-specifications-2014-en.pdf
	numLights := len(pixels)
	// End frames are needed to be able to push enough SPI clock signals due to
	// internal half-delay of data signal from each individual LED. See
	// https://cpldcpu.wordpress.com/2014/11/30/understanding-the-apa102-superled/
	l := 4*(numLights+1) + numLights/2/8 + 1
	if len(d.buf) != l {
		d.buf = make([]byte, l)
		// It is not necessary to set the end frames to 0xFFFFFFFF.
		// Set end frames right away.
		//s := d.buf[4+4*numLights:]
		//for i := range s {
		//	s[i] = 0xFF
		//}
	}
	// Start frame is all zeros. Just skip it.
	s := d.buf[4 : 4+4*numLights]
	// TODO(maruel): Calculate power in duty cycle of each channel.
	power := 0
	for i := range pixels {
		s[4*i], s[4*i+1], s[4*i+2], s[4*i+3] = ColorToAPA102(pixels[i])
		//power += p
	}
	if d.AmpBudget != 0 {
		powerF := float32(power) * d.AmpPerLED / 255.
		if powerF > d.AmpBudget {
			ratio := d.AmpBudget / powerF
			for i := range s {
				if i%4 != 0 {
					s[i] = anim1d.FloatToUint8(float32(s[i]) * ratio)
				}
			}
		}
	}
	_, err := d.w.Write(d.buf)
	return err
}

func (d *DotStar) MinDelay() time.Duration {
	// As per APA102-C spec, it's max refresh rate is 400hz.
	// https://en.wikipedia.org/wiki/Flicker_fusion_threshold is a recommended
	// reading.
	return time.Second / 400
}

// MakeDotStar returns a strip that communicates over SPI to APA102 LEDs.
//
// This is generally what you want once the hardware is connected.
func MakeDotStar() (*DotStar, error) {
	// The speed must be high, as there's 32 bits sent per LED, creating a
	// staggered effect. See
	// https://cpldcpu.wordpress.com/2014/11/30/understanding-the-apa102-superled/
	w, err := rpi.MakeSPI("", 10000000)
	if err != nil {
		return nil, err
	}
	return &DotStar{
		RedGamma:   1.,
		RedMax:     0.5,
		GreenGamma: 1.,
		GreenMax:   0.5,
		BlueGamma:  1.,
		BlueMax:    0.5,
		AmpPerLED:  .02,
		AmpBudget:  9.,
		w:          w,
	}, err
}

//

/*
// intensityLimiter limits the maximum intensity. Does this by scaling the
// alpha channel.
type intensityLimiter struct {
	Child Pattern
	Max   int // Maximum value between 0 (off) to 255 (full intensity).
}

func (i *intensityLimiter) NextFrame(pixels []color.NRGBA, sinceStart time.Duration) {
	i.Child.NextFrame(pixels, sinceStart)
	for j := range pixels {
		pixels[j].A = uint8((int(pixels[j].A) + i.Max - 1) * 255 / i.Max)
	}
}

// powerLimiter limits the maximum power draw (in Amp).
//
// It does this by scaling -each- the alpha channel but only when too much LEDs
// are lit, which would cause too much Amperes to be drawn. This means when
// only a subset of the strip is lit, all colors can be used but when all the
// strip is used, the intensity is limited.
//
// TODO(maruel): Calculate the actual power draw per channel.
// TODO(maruel): Check if the draw is linear to the intensity value per channel.
// TODO(maruel): This should only be done once alpha has been evaluated.
// TODO(maruel): This shoudl only be done after gamma correction (?)
type powerLimiter struct {
	Child     Pattern
	AmpPerLED float32
	AmpBudget    float32
}

func (p *powerLimiter) NextFrame(pixels []color.NRGBA, sinceStart time.Duration) {
	p.Child.NextFrame(pixels, sinceStart)
	power := 0.
	for _, c := range pixels {
		cR, cG, cB, _ := c.RGBA()
		power += float32(cR>>8+cG>>8+cB>>8) * p.AmpPerLED
	}
	if power > p.AmpBudget {
		// We only need to scale down the alpha as long as we treat each channel as
		// having the same power budget.
		for i := range pixels {
			pixels[i].A = FloatToUint8(float32(pixels[i].A) * power / p.AmpBudget)
		}
	}
}

// gammaCorrection corrects the intensity of each channel and 'applies' the
// alpha channel.
//
// TODO(maruel): The alpha channel should be dropped after this? As the alpha
// correction is linear.
//
// For example, the green channel will likely be much brighter than red and
// blue.
//
// '*Max' value are what should be considered 1.0, when it's deemed not
// necessary to use the channel at full intensity. This is useful as this can
// limit the amperage used by the LED strip, which is a concern for longer
// strips.
type gammaCorrection struct {
	Child      Pattern
	RedGamma   float32
	RedMax     float32
	GreenGamma float32
	GreenMax   float32
	BlueGamma  float32
	BlueMax    float32
}

func (g *gammaCorrection) NextFrame(pixels []color.NRGBA, sinceStart time.Duration) {
	g.Child.NextFrame(pixels, sinceStart)
	for i := range pixels {
		pixels[i].R = FloatToUint8(255. * math.Pow(float32(pixels[i].R)/255.*g.RedMax, 1/g.RedGamma))
		pixels[i].G = FloatToUint8(255. * math.Pow(float32(pixels[i].G)/255.*g.GreenMax, 1/g.GreenGamma))
		pixels[i].B = FloatToUint8(255. * math.Pow(float32(pixels[i].B)/255.*g.BlueMax, 1/g.BlueGamma))
	}
}
*/