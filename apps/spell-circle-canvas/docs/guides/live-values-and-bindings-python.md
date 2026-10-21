# Live values and bindings in Python

An instrument panel that follows one number: a dial whose needle sweeps
and trembles, a meter bar, a peak lamp that lights only near the top,
a row of twelve lights that step on one at a time, and three tracks whose
pucks fold, swell and overshoot. Every mark reads
the same live value, each through its own binding, and one line of
Python writes that value. The whole file is
`apps/grimoire/sketches/python_bindings.py`; open it with
`Grimoire python_bindings.py` and edit it live.

## One number, many readings

A live value is a cell. `motion.animatable(x)` makes one; every copy
reads and writes the same cell; a property handed it is connected to the
number, not given a snapshot of it. `motion.bind(value, ...)` is a
second live value that reads the first through a fixed list of stages
whenever it is read, so a binding costs nothing until something paints.

| Stage | What it does to the number |
| --- | --- |
| `from_`, `clampFrom` | maps the source's range onto 0 to 1, and clamps there when set |
| `alternate` | there and back: a triangle |
| `envelope` | the shape the 0-to-1 phase takes |
| `ease` | a curve |
| `quantize` | that many evenly spaced levels |
| `reverse` | one minus the value |
| `to` | 0 to 1 onto the output range |
| `wrap` | the output folded into a range |
| `wiggle` | smooth noise, in the output's own units |
| `clamp` | the output bounded |

The stages run in that order whatever order you name them in, so a
binding reads the way it runs.

## Step 1 — the source

The number comes from an `Oscillator`: one repeating signal with a
wave, a rate in hertz, a starting phase in cycles, an amplitude and the
centre it swings about. Read at a time, it answers the same value every
time it is asked.

```python
SOURCE = Oscillator(wave=Wave.Sine, hertz=0.17, phase=0.6, amplitude=0.46, centre=0.5)
```

A timer reads it every frame and writes the live value. That is the only
line in the sketch that moves anything.

```python
        self.level = motion.animatable(SOURCE.at(0))
        ctx.engine.timer(lambda delta, elapsed: self.level.set(SOURCE.at(elapsed)))
```

The panel's ranges are named once, as values:

```python
SWEEP = (-135, 135)  # the needle's travel, in degrees
FULL_SCALE = Range(0.0, 1.0)  # what the source reads from empty to full
```

`Range(low, high)` and a plain pair are interchangeable wherever a
binding takes a range. A `Range` is worth naming when several bindings
share it, as `FULL_SCALE` is here: change what the source reads from
empty to full, and every mark rescales.

## Step 2 — a dial, with a tremble

The needle is a dial-sized layer turned about the dial's centre. Its
rotation is the level mapped from full scale onto the sweep, with a
wiggle on top:

```python
        .rotate(
            bind(
                level,
                from_=FULL_SCALE,
                to=SWEEP,
                wiggle=Wiggle(amount=1.6, frequency=9, seed=4),
            )
        )
```

The wiggle is in the output's units — up to 1.6 degrees — and it is
sampled at the level's normalised phase, not at the clock. So the needle
trembles while the level moves and holds still when it holds still, the
way a real meter's needle does. The seed picks the noise; the same seed
trembles the same way in every still.

## Step 3 — a meter, and a lamp from an envelope

The bar is scaled from its left edge by the level, clamped so a source
that overshoots full scale cannot push the bar out of its trough:

```python
        .transformOrigin("0%", "50%")
        .scaleX(bind(level, from_=FULL_SCALE, clamp=(0, 1)))
```

The peak lamp is an envelope. `envelope.trapezoid(rise_start,
hold_start, hold_end, fall_end)` names four positions on the 0-to-1
phase: dark until the first, rising to full by the second, held to the
third, falling to dark at the fourth. With the corners at 0.8, 0.86, 1
and 1, the lamp is dark for most of the travel, rises between 0.8 and
0.86 and is fully lit above:

```python
        .opacity(bind(level, envelope=envelope.trapezoid(0.8, 0.86, 1.0, 1.0)))
```

`envelope.cosine()` is a swell up and back, `envelope.square(duty)` a
pulse, and `envelope.shaped(curve)` any curve of yours.

## Step 4 — a stepped readout with `quantize`

Twelve lights, each one a binding over its own twelfth of the range.
`from_` picks the slice and `clampFrom` holds the result at 0 below it
and at 1 above it. `quantize=2` then allows only two levels, so a light
is off or on, never half lit. `to` lifts the off state to a dim 12 per
cent, so an unlit segment still shows where it is.

```python
def stepped(level: motion.Animatable) -> compose.Element:
    lights = [
        compose.box()
        .width(METER / SEGMENTS - 6)
        .height(30)
        .borderRadius(2)
        .fill(PEAK if index >= SEGMENTS - 2 else SIGNAL)
        .opacity(
            bind(
                level,
                from_=(index / SEGMENTS, (index + 1) / SEGMENTS),
                clampFrom=True,
                quantize=2,
                to=(0.12, 1.0),
            )
        )
        for index in range(SEGMENTS)
    ]
    return compose.box(*lights).row().gap(6)
```

One binding with `quantize=12` on a single needle would step it in
twelfths. A text readout of the number is different: the digits are
structure, not a property, so they belong in a slot re-rendered when the
shown value changes, not in a binding.

## Step 5 — the remaining stages, one track each

Three more readings of the same number, each a puck carried along a
track by one binding: `alternate` over the lower half of the range runs
the puck out and back while the source rises once, a cosine envelope
swells it out and home over the full range, and an overshooting ease
carries it past its stop near the top before it settles. Each track is
the label and the keywords of its binding, so a new stage is one more
line:

```python
SHAPES = [
    ("FOLD · ALTERNATE", dict(from_=(0.0, 0.5), alternate=True)),
    ("COSINE · ENVELOPE", dict(from_=FULL_SCALE, envelope=envelope.cosine())),
    ("OVERSHOOT · EASE", dict(from_=FULL_SCALE, ease=ease.outBack())),
]
```

```python
        .translateX(bind(level, to=(0, METER - PUCK), **stages))
```

## Step 6 — the same binding, fed from the wire

The bindings never ask where the number came from. To drive the panel
from a sender instead of the oscillator, take the timer out and write
the live value where arrivals are read. The `wardnet_live` console does
exactly this for its load meter: a `meter` message eases the live value
`self.load` to the load it carries, and the bar follows it through
`bind(self.load, to=(-METER_WIDTH, 0))`. Here, one line in a handler,
`self.level.set(message["load"].number())`, would connect every mark on
this panel to a sender; nothing in `dial`, `meter`, `stepped` or
`shaped` changes. A sender in units other than 0 to 1 changes
`FULL_SCALE` and nothing else.

This sketch keeps the oscillator so its still is the same on every
machine; a capture of the live sketch replays a recording for the same
reason.

## Step 7 — run it

```sh
Grimoire python_bindings.py                                # live
Grimoire python_bindings.py --frame panel.png              # its own moment, 3.4 s: near the peak
Grimoire python_bindings.py --frame panel.png --at 6.4     # near the trough
```

## What to change next

- **Another wave.** `Wave.Triangle` sweeps at a constant speed;
  `Wave.Square` with a `duty` jumps between two readings, and the wiggle
  stops between jumps.
- **A falling needle.** `reverse=True` reads the dial from the right.

## The fine print

- **Bindings are values.** They compare by their stages and by the cell
  their source reads, never by the number behind it, so a tree
  described again with the same bindings is the same tree.
- **Prefer the `ease` curves.** A curve in a binding is part of its
  identity. An `ease` value compares equal to itself; a curve written in
  Python, even the same function object, compares unequal and re-patches
  the node each time the tree is described again.
- **Live needs no declaration.** A property handed a live value is
  painted as live, so no cache freezes its first frame.
