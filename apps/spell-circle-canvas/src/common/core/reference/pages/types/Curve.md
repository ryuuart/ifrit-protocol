---
kind: type
library: SigilCore
name: Curve
qualified: sigil::core::curve::Curve
group: Compute
status: stable
---

# Curve

A SHAPED CURVE AS A COMPARABLE VALUE: the shape and the numbers that
shape it, side by side, so a curve carrying parameters can still be
proved the same curve.

Everything that reshapes a unit position reaches for one of these — an
animation's easing, a colour ramp's walk, a keyed track's segment — and
every one of them also has to answer whether two of its values are the
same, because the thing holding the curve is memoised on that answer. A
curve written as a lambda cannot answer: two lambdas built a frame
apart from identical text are different objects, so a record holding
one is unequal to itself and never prunes. A curve written as a bare
function pointer can answer, but then it has nowhere to keep the
overshoot, the period or the four control numbers that give it its
character.

This is the shape and the numbers kept apart. The shape is a
captureless function — a pointer, therefore comparable — and the
numbers are four floats beside it, so two curves are equal when they
are the same shape at the same settings, and a memo keyed on one can be
skipped.

## The arithmetic is the contract

The house shapes are the Penner equations, spelled here rather than
reached for in an animation dependency, because a colour ramp and a
point cook read the same curve as an animation does and neither of them
links one. Two callers asking for the same shape at the same numbers
get the same float.

## The escape hatch

`sigil::core::curve::Curve::shape` is the shape as a captureless
function over the parameter block: a pointer, so two of them can be
compared. Null answers `t` unchanged, which is the identity ramp.

A caller's own curve fits by being written there: a captureless lambda
over the parameter block is comparable for free, and reads its own
numbers out of `parameters` exactly as the house shapes do. A body that
must capture is a lambda again, and unequal to everything — which is
correct, since nothing can prove two of them alike.

`sigil::core::curve::Curve::parameters` is what the shape reads. Four
because the widest house curve — a cubic Bezier's two control points —
takes four; the rest leave the tail at zero, and a zero is as
comparable as any other number.

## The smooth ramp

`sigil::core::curve::smoothstep` is the Hermite S-curve, `t²(3 − 2t)`:
eased at both ends, symmetric, and the one shape the house set
otherwise lacks — back, elastic and bounce all overshoot, and none of
them is the plain smooth ramp a wipe, a fade edge or a gloss ring
wants.

A plain function rather than a `Curve`, because it has no parameters:
call it directly on a normalised value, or hand `&smoothstep` anywhere
a float→float function is wanted, where it compares by its own address.
The input is NOT clamped — the polynomial turns back on itself outside
[0, 1].

## The CSS curve

`sigil::core::curve::cubicBezier` is that curve by its own
definition: the cubic Bezier through (0,0), (x1,y1), (x2,y2), (1,1),
evaluated as y at the x the caller asks for.
`cubicBezier(0.25, 0.1, 0.25, 1)` is `ease`, the CSS default, and the
whole point of having this is that a design handed over as a CSS timing
function can be spelled as it was written instead of matched by eye
against the nearest house curve.

x is solved by bisection rather than Newton: the curve is monotonic in
x for control points in [0, 1], so a fixed number of halvings is exact
to well under a pixel and cannot fail to converge on a degenerate curve
the way a derivative-based solve can.

The four control numbers ARE the identity: two curves compare equal
when they were asked for at the same numbers.

## See also

- `compute/Curve.h` — the header: `Curve`, `smoothstep`, `cubicBezier`
- `COMPUTE.md` — the chapter the house shapes are catalogued in
