---
kind: verb
library: SigilCompose
name: travel
qualified: sigil::compose::Element::travel
header: sigilcompose/core/verbs/Transform.h
group: Transform
python: sigil.compose.Element.travel
status: stable
---

# travel

Ride a curve instead of two lanes: the node's position IS a point on a
path, and one animatable float says where along it.

## Description

```cpp
dot.travel({.path = shapes::circle(),
            .t = bind(&phase).target(0, 1),
            .lookAhead = 0.02f});   // auto-orient along the tangent
```

**The curve is resolved against the PARENT's box**, because the frame
the node moves in is what gives a path a size: a circle on a small dot
inside a large card is an orbit of the card.

**The transform origin is the point that lands on the curve**, and it
is already the pivot `rotate`, `scale` and the skews turn about, so the
point on the curve is fixed under all of them.

**It outranks `translateX` and `translateY`** while a path is engaged,
and hands the same lanes back when it is dropped. Paint-only, like the
lanes it outranks: a travelling node never relayouts.

**`lookAhead` turns the node to face along the curve.** It ADDS to
`rotate`, so an authored spin composes on top of the banking. Zero, the
default, leaves orientation alone.

`MotionPath` is where the value's own rules live: wrapping on a closed
curve and clamping on an open one, arc length as the one
parameterisation, and what a path of no length does.

## See also

`translateX`, `rotate`, `transformOrigin`, [`shape`](shape.md) for the
generators a path is made from, and `MotionPath` for the value this
takes.
