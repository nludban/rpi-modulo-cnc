package libgcode

import "fmt"
import "strings"
import "strconv"

// [expr] -or- {expr} -or- expr
// reprap numbers: a:b:c for multiple extruders
// R = temperature
// ; comment -or- (comment)
// *checksum -or- crc

// A Number holds a parsed numeric value, preserving the number of
// digits after the decimal point.
type Number struct {
	value, scale int
}

func (n *Number) String() string {
	s := fmt.Sprintf("%v", n.value)
	if n.scale > 0 {
		r := []rune(s)
		a := r[:len(r) - n.scale - 1]
		b := []rune(r[len(r) - n.scale - 1:])
		r = make([]rune, len(r) + 1)
		copy(r, a)
		r = append(r, '.')
		r = append(r, b...)
		s = string(r)
	}
	return s
}

// ParseNumber creates a Number from a string
func ParseNumber(s string) *Number {
	// strip "+"
	s, _ = strings.CutPrefix(s, "+")

	// signed? "-"
	s, signed := strings.CutPrefix(s, "-")

	// ignore "_"
	s = strings.ReplaceAll(s, "_", "")

	// count digits after "."
	parts := strings.Split(s, ".")
	scale := 0
	if len(parts) == 2 {
		scale = len(parts[1])
	}
	// remove "."
	s = strings.Join(parts, "")

	value, _ := strconv.Atoi(s)
	if signed {
		value = -value
	}

	return &Number{value, scale}
}

// AsInteger returns the whole part of the Number
func (n *Number) AsInteger () int {
	v := n.value
	for _ = range n.scale {
		v /= 10
	}
	return v
}



// AsFloat returns the Number as a float64
func (n *Number) AsFloat () float64 {
	v := float64(n.value)
	for _ = range n.scale {
		v /= 10.0
	}
	return v
}


// G-codes
const (
	G0_RapidMove = 0
	G1_LinearMove = 1
	G2_ArcMoveLTU = 2 // Clockwise
	G3_ArcMoveRTU = 3 // Counter-clockwise
	G4_Dwell = 4
	// G5 cubic spline, G5.1 quadratic, G5.2 NURBS
	// G7 lathe diameter mode
	// G8 lathe radius mode
	//G10 set tool offset, workplace coords, tool temp
	G17_PlaneXY = 17
	G18_PlaneZX = 18
	G19_PlaneYZ = 19
	G20_Inches = 20
	G21_MM = 21
	//G28_Home = 28
	//G40_CompensationOff = 40
	//G53..59 + 59.1..3 coord system select, 1-9
	//G68 coordinate rotation, G69 cancel
	G90_AbsolutePositioning = 90
	G91_RelativePositioning = 91
	G92_SetPosition = 92
	//G92.1 - all axes to zero and erase G92 offsets (reset to zero)
	//G92.2 - all axes to zero
	//G92.3 - restore offsets
	//G93 feed rate inverse time mode
	//G94 feed rate units/minute
	//G95 - units per rev mode
)

// Fields (case insensitive, per NIST)
const (
	HasG = 1 << iota

	HasF	// Feed rate
	HasS	// (pause seconds) (power) (spindle rpm)
	// T = select tool
	// D = debug codes (reprap)
	HasP	// Parameter (pause ms) (tool no)

	HasX	// Position
	HasY
	HasZ

	HasI	// Arc center
	HasJ
	HasK

	HasE	// Extrude amount
	//HasH	// Endstop check or ignore

	HasM	// Machine commands

	HasN	// Line number

	HasA	// Rotation coord system
	HasB
	HasC

	HasU	// Aux cartesian coord system
	HasV
	HasW
)

type Command struct {
	Fields		int

	G		int
	M		int

	N		Number
	Comment		string

	F		Number
	X, Y, Z		Number
	I, J, K		Number
	A, B, C		Number
	E		Number
	U, V, W		Number
	S		Number
	T		Number
}

//--
