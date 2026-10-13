# A title sequence on a timeline, in Python

A film's opening card: the title fades up and rises, a rule draws out
beneath it while the title is still settling, a subtitle follows, then
three credits swap in one after another, everything leaves on a stagger,
and the sequence holds a frame halfway through and plays again from the
top when it ends. A strip along the foot shows the playhead crossing the
labels. The whole file is `apps/grimoire/sketches/python_timeline.py`;
open it with `Grimoire python_timeline.py` and edit it live.

## What a timeline is for

An entrance plays when its node mounts and a transition when its value
changes. A sequence is neither: its parts are timed against each other,
not against the tree. A `Timeline` places tweens, runs of tweens and
calls on one clock of its own, and hands back a playback you can pause,
seek and replay as one.

| Position | Starts the item |
| --- | --- |
| `at(t)` | at `t` seconds from the timeline's start |
| `afterEnd(offset)` | after everything already on it has ended — the default |
| `afterPrevious(offset)` | when the item added just before ends |
| `withPrevious(offset)` | when the item added just before starts |
| `atLabel(name, offset)` | at a moment named with `label` |

## Step 1 — the values the sequence writes

Every moving part of the card reads a live value, and the timeline is
the only thing that writes them. The playhead is one more live value,
written by the transport in Step 4.

```python
        self.title = motion.animatable(0.0)
        self.rule = motion.animatable(0.0)
        self.subtitle = motion.animatable(0.0)
        self.playhead = motion.animatable(0.0)
```

The page reads them as properties and through bindings. The title takes
its value as its opacity and, through a binding, as a rise from 28
pixels below:

```python
        title = (
            compose.text("THE NORTHERN LINE", size=64, color=INK)
            .letterSpacing(10)
            .opacity(self.title)
            .translateY(motion.bind(self.title, to=(28, 0)))
        )
```

## Step 2 — labels and positions

The times that matter are named once at the top of the file, so the
timeline and the strip that draws it agree:

```python
OPEN = 0.4  # the title starts
CREDITS = 2.6  # the first credit is swapped in
OUT = 6.8  # everything leaves
LENGTH = OUT + 0.95  # the last item ends
FREEZE = 4.6  # the transport holds this frame
HOLD = 0.8  # for this long, and the finished sequence as long again
```

Then the sequence reads top to bottom the way an editor's notes would:

```python
        self.sequence = (
            ctx.engine.timeline()
            .label("open", at(OPEN))
            .add(
                self.title,
                Tween(from_=0, to=1, duration=1.4, ease=ease.outCubic),
                atLabel("open"),
            )
            .add(
                self.rule,
                Tween(from_=0, to=1, duration=1.0, ease=ease.inOutCubic),
                withPrevious(0.5),
            )
            .add(self.subtitle, Tween(from_=0, to=1, duration=0.7), afterPrevious())
            .label("credits", at(CREDITS))
            .call(lambda: self.show_credit(0), atLabel("credits"))
            .call(lambda: self.show_credit(1), atLabel("credits", 1.4))
            .call(lambda: self.show_credit(2), atLabel("credits", 2.8))
            .add(
                [self.title, self.rule, self.subtitle],
                Tween(to=0, duration=0.65, delay=stagger(0.15)),
                at(OUT),
            )
        )
```

Read it as relations, not as times. The rule starts half a second after
the title starts (`withPrevious(0.5)`), so the two overlap; the subtitle
waits for the rule to finish (`afterPrevious()`). Move `OPEN` and all
three move with it. The credits hang off their own label, so the
opening can be re-timed without touching them.

The last item is a run: a list of values with a `stagger` in the
tween's `delay`, so the title leaves first, the rule 0.15 seconds later
and the subtitle last. Its tween names only `to`, so each value sets out
from wherever it rests.

## Step 3 — a call that swaps a slot

A credit is structure, not a number, so it is a slot. A `call` on the
timeline runs a callback at its position, and the callback re-renders
the slot:

```python
    def show_credit(self, index: int) -> None:
        self.composer.renderSlot("credit", credit(index))
```

Each credit is keyed by its index and carries its own entrance, so when
the slot swaps to the next one the new node mounts and plays it:

```python
def credit(index: int) -> compose.Element:
    role, name = CREDIT_LINES[index]
    return (
        compose.box(label(role, 13, EMBER), compose.text(name, size=30, color=INK))
        .column()
        .gap(8)
        .alignItems("center")
        .key(f"credit{index}")
        .opacity(motion.animate(from_=0, to=1, duration=0.6))
        .translateY(motion.animate(from_=14, to=0, duration=0.6, ease=ease.outCubic))
    )
```

The timeline owns WHEN; the slot owns WHAT; the node owns HOW it
arrives. None of the three knows about the others.

## Step 4 — a transport on a timer

The timeline is a playback, and a timer is a good place to drive one.
This one runs every frame: it writes the playhead from the sequence's
progress, pauses on the frame at `FREEZE`, resumes after `HOLD`, and
when the sequence has completed it waits as long again, clears the
credit and plays from the top.

```python
    def transport(self, delta: float, elapsed: float) -> None:
        self.playhead.set(self.sequence.progress())
        if self.sequence.isPaused() or self.sequence.isCompleted():
            self.waited += delta
            if self.waited < HOLD:
                return
            self.waited = 0.0
            if self.sequence.isCompleted():
                self.frozen = False
                self.composer.renderSlot("credit", compose.box())
                self.sequence.seek(0).play()
            else:
                self.sequence.resume()
        elif not self.frozen and self.sequence.currentTime() >= FREEZE:
            self.frozen = True
            self.sequence.pause()
```

`seek(0)` puts every value back where the timeline says it stands at
zero, and the calls fire again as the playhead crosses them. The other
verbs answer the same way: `restart`, `reverse`, `alternate`,
`complete`, `cancel`, and `revert`, which puts every value back where it
stood before the timeline began.

The strip reads the playhead the way the card reads the title: a bar
scaled by it, and a dot carried across by a binding.

```python
            .translateX(motion.bind(self.playhead, to=(0, TRACK)))
```

## Step 5 — run it

```sh
Grimoire python_timeline.py                                  # live, looping
Grimoire python_timeline.py --frame northern.png             # its own moment, 1.4 s
Grimoire python_timeline.py --frame northern.png --at 3.4    # the first credit in
Grimoire python_timeline.py --frame northern.png --at 5.0    # held on the freeze frame
```

## Why a still at `--at` is exact

A capture opens the session under a clock that only a stated frame
moves, and steps it from zero to the moment asked for in fixed
sixtieths of a second. The timeline, its calls, the transport timer and
every entrance a credit plays all run on that one clock. So `--at 5.0`
is the sequence after exactly 300 steps: the freeze already taken at
`FREEZE`, the pause counted in those same steps, the second credit in.
Nothing reads the wall, so nothing a machine does changes the picture.

## What to change next

- **Re-time by label.** Move `CREDITS` later and every credit moves;
  add `.label("score", afterPrevious())` and hang a music cue off it.
- **A scrubber.** Write the playhead from a pointer instead of from the
  progress, and call `self.sequence.seek(position * LENGTH)` with it.
- **Overlap the exit.** Give the closing run `delay=stagger(0.15,
  from_=StaggerFrom.Last)` so the subtitle leaves first.

## The fine print

- **One writer per value.** The timeline writes the title; nothing else
  should. A second animation on the same value takes it over.
- **Calls are for structure.** A `call` that re-renders a slot is the
  bridge between time and the tree; a number that moves belongs in a
  tween, where it is interpolated rather than jumped.
- **Keep the composer, not the context.** `ctx` is valid only inside
  `setup`; the timeline's calls and the timer run later, so they use
  `self.composer`.
