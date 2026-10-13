# Springs and oscillators in Python

A panel that snaps between two places, overshoots, rings and comes to
rest; its colour settling from cool to warm on a slower spring that
never crosses; a flash on the place it lands and a wobble that dies
away; three oscillators at one rate in three waves; and a readout of
the three questions that all sound like "has it stopped?". The whole
file is `apps/grimoire/sketches/python_springs.py`; open it with
`Grimoire python_springs.py` and edit it live.

## A function of time, or a state

Every other motion value is a function of a time: read a tween, an
oscillator or a `decay` at a second and it answers the same number every
time. A spring is not. It is a position and a velocity, stepped towards
a target that is allowed to move, and that velocity is the point: when
the target jumps mid-flight, a tween can only start a new ramp from
where the value stands and loses its speed; a spring keeps its speed and
turns.

| Reach for | When |
| --- | --- |
| `Oscillator` | something periodic that a time alone decides: a breath, a sway, a blink |
| `decay`, `flash` | a response to an event, read from the event's age |
| `Spring` | a value chasing a target that can move, which must keep its momentum |

## Step 1 — two springs, named by feel

`SpringParameters` takes a `period` — how long the spring would take to
ring once with no damping, which is how fast it is — and a `damping`
ratio: under 1 it overshoots and rings, at 1 it arrives as fast as it can
without crossing, over 1 it crawls in. The two are independent, so
re-timing a bounce keeps its shape.

```python
SNAP = SpringParameters(period=0.6, damping=0.45)  # rings, then rests
SETTLE = SpringParameters(period=0.9, damping=1.0)  # arrives without crossing
```

The springs themselves are plain values, held by the sketch beside the
live values the page reads:

```python
        self.travel = Spring(value=PLACES[0])  # the place, with its own velocity
        self.warmth = Spring()  # 0 cool, 1 warm
        self.place = motion.animatable(PLACES[0])
        self.tint = motion.animatable(compose.Fill.color(COOL))
```

## Step 2 — step them from a timer

A timer runs every frame and hands its callback the step and the elapsed
seconds. The target flips between the two places every `DWELL` seconds;
each spring is stepped towards its own target by exactly the step, and
the result is written to the live values.

```python
    def step(self, delta: float, elapsed: float) -> None:
        toward = 1 - int(elapsed / DWELL) % 2  # which place the panel is sent to
        target = PLACES[toward]
        before = self.travel.value - target
        self.travel = self.travel.step(target, delta, SNAP)
        crossed = before * (self.travel.value - target) < 0
        if crossed and abs(self.travel.velocity) > HIT_SPEED:
            self.hit_at = elapsed
        self.warmth = self.warmth.step(toward, delta, SETTLE)

        self.place.set(self.travel.value)
        self.tint.set(
            compose.Fill.color(material.lerpOklab(COOL, WARM, self.warmth.value))
        )
```

`step` hands back a new spring rather than changing the old one, so
`self.travel = self.travel.step(...)` is the whole update. It is solved
in closed form, not integrated, so one step of any size is exact: sixty
small steps and one large one land on the same place.

The colour is the second spring's value, 0 to 1, mixed between two
colours in OKLab, where a mix between two saturated colours stays bright
instead of passing through a muddy band. A live colour cannot fill a
box: the live value is a fill, `compose.Fill.color(...)`, which is what
`.fill(self.tint)` reads.

## Step 3 — a hit: `flash` and `decay`

A hit is an event, and what follows it is a function of its age. The
step above records when the panel last crossed its target fast enough
to count; then:

```python
        age = elapsed - self.hit_at
        self.strikes[toward].set(flash(age, 0.03, 0.3, rest=0.2))
        self.strikes[1 - toward].set(0.2)
        self.shake.set(5 * decay(age, 0.2) * math.sin(age * 40))
```

`flash(age, attack, timeConstant, rest)` rises to 1 over the attack and
falls back to its rest by the time constant: the outline of the place
the panel is landing on flares and fades to a dim 20 per cent. `decay(age,
timeConstant)` is the falling half alone, and here it scales a fast
sine into a wobble of up to five degrees that dies within half a second.
Neither keeps any state but the moment of the hit, so a still at any
second reads them exactly.

## Step 4 — oscillators

An `Oscillator` is one repeating signal: a wave, a rate, a starting
phase, an amplitude and a centre. Every wave is stated on the same
phase and swings on the same range, so swapping one for another keeps
the timing and changes only the feel:

```python
WAVES = [
    Oscillator(wave=Wave.Sine, hertz=0.6, amplitude=1.0),
    Oscillator(wave=Wave.Triangle, hertz=0.6, amplitude=1.0),
    Oscillator(wave=Wave.Square, hertz=0.6, amplitude=1.0, duty=0.3),
]
```

The timer reads each at the elapsed time into a live value, and each dot
follows its value through a binding:

```python
        for swing, oscillator in zip(self.swings, WAVES):
            swing.set(oscillator.at(elapsed))
```

```python
                .translateY(bind(swing, from_=(-1, 1), to=(36, -36))),
```

## Step 5 — the three words for stillness

"Is it still moving?" is three questions, and answering one with another
is how a page that has come to rest goes on repainting for ever. The
readout asks all three every frame and re-renders its slot only when an
answer changes:

```python
        answers = (
            self.place.isRunning(),
            self.engine.isRunning(),
            self.travel.isSettled(target),
        )
```

- **Declared** — `Animatable.isRunning()`: could this value move? A live
  value always could, so it answers yes for its whole life.
- **Running** — `Engine.isRunning()`: is the engine moving anything now?
  A timer that runs every frame keeps it yes even when every number it
  writes is the same.
- **Settled** — `Spring.isSettled(target)`: has the spring provably come
  to rest, within half a pixel and at no speed to speak of? An
  exponential approach never exactly arrives, so rest is a tolerance on
  the distance and the speed together: a spring passing through its
  target at speed is not resting on it.

Only the third can ever say "it stopped", which is why the readout
flips to yes about a second and a half after each move while the other
two never change. A sketch that wants to sleep once its spring has
settled stops writing; `return False` from the timer cancels it.

## Step 6 — run it

```sh
Grimoire python_springs.py                               # live
Grimoire python_springs.py --frame springs.png           # its own moment, 0.3 s: the first overshoot
Grimoire python_springs.py --frame springs.png --at 2.8  # landed, settled
```

## What to change next

- **A target that moves.** Send the panel to the pointer instead of to a
  place; the spring turns towards each new reading without losing speed.
- **Feel by numbers.** `damping=0.2` rings for a long time; `period=0.25`
  snaps; the two do not interfere.

## The fine print

- **The spring belongs to the sketch.** It is a value the timer replaces
  each step; nothing in the tree holds it. The tree holds the live
  values the timer writes.
- **The timer is the clock.** Stepping by the timer's `delta` keeps the
  spring on the session's clock, so a capture stepped from zero lands on
  the same overshoot every time.
