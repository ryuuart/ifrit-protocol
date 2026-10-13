# Entrances and staggers in Python

A page that arrives the way a well-made web page does: a grid of cards
that opens out from its centre, a strip that wipes in column by column,
a row of dots that land in a seeded scatter, a queue whose rows arrive
one after another and later leave in reverse, playing their entrances
backwards, and a cursor that eases to each new stop. The whole file is
`apps/grimoire/sketches/python_entrances.py`; open it with
`Grimoire python_entrances.py` and edit it live.

## Entrance, exit or transition

An entrance and a transition are both `motion.animate`, and the
difference is one keyword. An exit is one word on the node.

| You write | It is | It plays |
| --- | --- | --- |
| `animate(from_=a, to=b)` | an entrance | once, when the node first mounts; described again, it starts nothing |
| `.exit()` on a keyed node | an exit | when a later description leaves the node out: every entrance under it backwards, and then the node goes |
| `animate(to=b)` | a transition | whenever a later description carries a different `to`, from wherever the value is |
| `ctx.engine.animate(values, tween)` | a run you hold | when you say so, and as often: it hands back a playback |

An entrance belongs to the node: it is how the node comes into the
world, and with `.exit()` the same motion played backwards is how it
leaves. When something must play again on a signal of its own, move
some other way when it leaves, or wait while it stays, put live values
on the engine instead and keep the playback.

## Step 1 — a grid that opens from its centre

`stagger` is a value, not a verb on the parent. It sits in the `delay`
of each card's own tween, and the host resolves it from the card's place
among its siblings when it lays them out. Given a grid, the distance it
counts is the distance across the grid; `StaggerFrom.Center` measures it
from the middle; `ease` says how the delays crowd, so `ease.inQuad`
packs the middle together and spreads the rim; `start` is added to every
child's value.

```python
def card(index: int) -> compose.Element:
    from_centre = stagger(
        0.12,
        grid=(COLUMNS, ROWS),
        from_=StaggerFrom.Center,
        ease=ease.inQuad,
        start=0.2,
    )
    return (
        compose.box(label(f"{index + 1:02d}", 11, INK))
        .width(104)
        .height(72)
        .padding(10)
        .borderRadius(8)
        .fill(ACCENTS[(index % COLUMNS + index // COLUMNS) % len(ACCENTS)])
        .opacity(motion.animate(from_=0, to=1, duration=0.6, delay=from_centre))
        .scale(
            motion.animate(
                from_=0.4, to=1, duration=0.8, delay=from_centre, ease=ease.outBack()
            )
        )
    )
```

Every card names `from_`, so every card's tween is an entrance. The
opacity and the scale share one stagger, so a card fades and grows on
the same beat, and the scale overshoots on `ease.outBack()` before it
settles. The cards are siblings in one wrapping row, which is what makes
them one run of 24 for the stagger to count:

```python
def card_grid() -> compose.Element:
    cards = [card(index) for index in range(COLUMNS * ROWS)]
    return (
        compose.box(*cards)
        .row()
        .flexWrap(compose.FlexWrap.Wrap)
        .gap(12)
        .width(COLUMNS * 104 + (COLUMNS - 1) * 12)
    )
```

## Step 2 — a wipe along one axis, a scatter from a seed

`axis` keeps one direction of a grid distance and drops the other. With
`StaggerAxis.Columns` every cell in a column shares a delay, so a block
of cells wipes in from the left as one edge:

```python
def wipe() -> compose.Element:
    by_columns = stagger(
        0.05,
        grid=(16, 3),
        from_=StaggerFrom.First,
        axis=StaggerAxis.Columns,
        start=0.25,
    )
```

`StaggerFrom.Random` deals the order at random, keyed on the count and
the seed: the same seed deals the same order on every frame, after every
reload and in every still, and a different seed deals an independent
one.

```python
def scatter() -> compose.Element:
    seeded = stagger(0.04, from_=StaggerFrom.Random, seed=11, start=0.35)
    dots = [
        compose.box()
        .width(14)
        .height(14)
        .borderRadius(7)
        .fill(ACCENTS[0])
        .translateY(motion.animate(from_=-18, to=0, duration=0.4, delay=seeded))
        .opacity(motion.animate(from_=0, to=1, duration=0.25, delay=seeded))
        for _ in range(24)
    ]
    return compose.box(*dots).row().gap(10)
```

## Step 3 — a queue that arrives, then leaves in reverse

Each row is keyed by its name and states its own entrance: it fades in
and slides home from 48 pixels to the right, on a stagger that brings
the rows in one after another. `.exit()` says the row leaves the way it
came.

```python
        .key(name)
        .opacity(
            motion.animate(
                from_=0, to=1, duration=0.45, delay=one_by_one, ease=ease.outCubic
            )
        )
        .translateX(
            motion.animate(
                from_=48, to=0, duration=0.45, delay=one_by_one, ease=ease.outCubic
            )
        )
        .exit()
```

The rows stand in a column of their own, so the stagger counts them as
one run of six, and the column keeps their height, so the cursor under
the queue stays where it is while the queue is empty.

```python
def queue(present: bool) -> compose.Element:
    rows = [queue_row(name, index) for index, name in enumerate(QUEUE)]
    return (
        compose.box(*(rows if present else []))
        .column()
        .gap(10)
        .height(len(QUEUE) * 48 + (len(QUEUE) - 1) * 10)
    )
```

Every three and a half seconds a timer describes the page again with
the queue present or empty. A row the description leaves out keeps its
place, plays its entrance backwards and goes when that lands: the rows
that left together leave as one run reversed, so the last row in is the
first out, on the mirror of the curve that brought it. The next turn
names them again, and they mount and arrive as they did the first time.

```python
    def turn(self) -> None:
        self.present = not self.present
        self.composer.render(self.page())
```

## Step 4 — a cursor that eases on change

The cursor is the transition row of the table. Its pill's `translateX`
names only `to`, and a timer re-renders the `cursor` slot with the next
stop. The pill is keyed, so the reconciler recognises it across renders,
sees `to` move, and eases there from wherever the pill is.

```python
        .key("pill")
        .translateX(motion.animate(to=stop * 56, duration=0.4, ease=ease.outCubic))
```

```python
    def move_cursor(self) -> None:
        self.stop = (self.stop + 1) % len(QUEUE)
        self.composer.renderSlot("cursor", cursor(self.stop))
```

`.transition(0.4)` on a node says the same thing once for every plain
value on it: a constant written there later ramps instead of jumping.

## Step 5 — a title whose glyphs rise

Text has its own entrance, glyph by glyph: `textFx.entrance` over one
of the stock glyph tweens — `rise`, `slide`, `pop`, `spinIn`,
`scatter`, `typeOn` — plays once when the text mounts, with each glyph
on its own beat.

```python
            .textFx(textFx.entrance(textFx.rise(30)))
```

## Step 6 — run it

```sh
Grimoire python_entrances.py                                   # live
Grimoire python_entrances.py --frame arrivals.png              # its own moment, 0.8 s
Grimoire python_entrances.py --frame arrivals.png --at 3.8     # the queue leaving
```

## What to change next

- **Another origin.** `from_=StaggerFrom.Edges` closes the grid in from
  its rim; a number such as `from_=8` opens it from the ninth card.
- **A stated order.** `rankBy=` takes one number per child and deals
  smallest first, so cards can arrive by price, by date or by distance
  from a pointer.
- **An exit that differs from the entrance.** `.exit()` plays the
  entrance backwards and nothing else. For a leave of its own, keep the
  rows in the tree, read live values from `motion.animatable`, and run a
  tween over them on the engine, `ctx.engine.animate(values,
  motion.Tween(to=0, ...))`, when they should go.

## The fine print

- **An entrance is declared, never started.** Nothing in `setup` plays
  the grid; the description says how each card arrives, and the host
  plays it on mount. Rebuilding the same description starts nothing.
- **Siblings are what a stagger counts.** It resolves from a child's
  place in its own parent, so cards split across two parents are two
  runs. Wrap them in one parent, as the grid does, to stagger them as
  one.
- **A leaving row is on its way out.** It answers no hit while it
  leaves, and a description that names it again before it has gone
  turns it round where it stands. A row with no key, or with nothing
  that entered, goes at once.
- **Stills are exact.** The capture steps the session's clock from zero
  in fixed sixtieths, and every entrance, exit and timer above runs on
  that clock, so `--at` shows the page as it stands at that second.
