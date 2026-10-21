# An animated console in Python: typed rows, a marquee, a pulsing alert

A security console that behaves like a web page: a header with a ticker
that never stops, a scrollback whose rows type themselves on behind a
blinking cursor, status cards, a load meter that glides, an alert chip
that breathes, and scanlines over everything. It is one Python file of
ordinary Compose elements, and every moving part uses one of four
mechanisms. The whole file is `apps/grimoire/sketches/wardnet_live/wardnet_live.py`;
open it with `Grimoire wardnet_live.py` and edit it live.

Nothing on this console is scripted inside it: the rows, the alert, the
cards and the load arrive as JSON messages from outside, and a still
replays a recording of them. [A console driven from outside](guide:console-driven-from-outside-python)
covers the doors and the messages; this page covers how the page moves.

## The four mechanisms

Every animation on this page is one of these, and knowing which is which
is most of the craft.

| Mechanism | Who drives it | Use it for |
| --- | --- | --- |
| A tween on a lane, looping | the runtime | anything periodic: a marquee, a blink, a breathing chip |
| An entrance | the runtime | anything that happens once when a node mounts: a row sliding in, its text typing on |
| A slot described again | you, when something changed | anything whose structure changes: rows arriving, the alert's text |
| A live value followed through a binding | the engine eases it; the property reads it | a value that moves without the tree changing: the meter's bar |

The page is described once in `setup`. The runtime owns the loops and
the entrances; a message describes again only the slot it changed;
nothing rebuilds the whole tree.

## Step 1 — the palette and a label

```python
WIDTH, HEIGHT = 1280, 720

VOID = "#04060b"
PANEL = "#0c121c"
RULE = "#22344a"
ACCENT = "#59cbe3"
BONE = "#e8eff6"
CHROME = "#8296ae"
DIM = "#49596d"
ALERT = "#ff4d5a"
EMBER = "#f2a33c"
MONO = "Menlo, SF Mono, monospace"
```

```python
def label(content: str, size: float = 12, color: str = CHROME) -> compose.Text:
    return (
        compose.text(content, size=size, color=color)
        .fontFamily(MONO)
        .letterSpacing(1.5)
    )
```

A console is a typographic object: one monospace family, a small set of
inks that mean something (dim for traces, accent for information, ember
for warnings, alert for breaches), and tracking on everything.

## Step 2 — a marquee: a loop on a lane

A ticker is one pass of the message measured, enough passes laid in a
row to cover the band, and the row slid left by exactly one pass, over
and over. `ctx.measure` lays a tree out and answers its size without
drawing it; `translateX` takes a tween; `loop=-1` makes it endless; the
parent clips what slides out of its box. Because the slide is one pass
long and every pass is the same, the loop has no seam.

```python
def marquee_pass(message: str, ink: str) -> compose.Element:
    """One pass of the marquee: the alert, then the space before the next."""
    return (
        compose.box(label(message, 13, ink), label("···", 13, DIM))
        .row()
        .gap(40)
        .paddingRight(40)
    )
```

```python
    def ticker(self, ctx: Scene2DContext) -> compose.Element:
        """The newest alert, sliding left forever. One pass is measured,
        enough passes are laid in a row to cover the band, and the row
        slides by one pass on a loop, so the slide has no seam."""
        text, level = self.alert
        run = ctx.measure(marquee_pass(text, LEVEL_INK[level])).width()
        passes = ceil((WIDTH - 56) / run) + 1
        slide = motion.animate(from_=0, to=-run, duration=run / MARQUEE_SPEED, loop=-1)
        return (
            compose.box(
                compose.box(
                    *(marquee_pass(text, LEVEL_INK[level]) for _ in range(passes))
                )
                .row()
                .translateX(slide)
            )
            .height(28)
            .alignItems("center")
            .overflow(compose.Overflow.Clip)
        )
```

Nothing is stepped by hand. The tween lives on the node's lane, so the
row is described once per alert and the runtime slides it every frame.

## Step 3 — a pulsing alert and a blinking cursor: loops that alternate

The same mechanism on a colour. A fill takes a colour tween;
`loop=-1, alternate=True` sends it back and forth; a glow filter spreads
the chip's light beyond its box. The level picks how fast it breathes.

```python
LEVEL_INK = {"clear": ACCENT, "warn": EMBER, "breach": ALERT}
# How an alert breathes at each level: the fill it falls back to, and
# the seconds one breath out takes. A breach breathes fastest.
LEVEL_PULSE = {
    "clear": ("#123540", 1.4),
    "warn": ("#4a3312", 0.7),
    "breach": ("#5a1a22", 0.35),
}
```

```python
def alert_chip(content: str, level: str) -> compose.Element:
    """A chip whose fill breathes between two colours, faster the worse
    the level."""
    rest, breath = LEVEL_PULSE[level]
    ink = LEVEL_INK[level]
    pulse = motion.ColorTween(rest, ink, duration=breath, loop=-1, alternate=True)
    return (
        compose.box(label(content, 12, BONE))
        .padding(6, 12)
        .borderRadius(3)
        .fill(pulse)
        .filter(material.Filter.glow(ink, 10))
        .key("alert")
    )
```

A blinking cursor is the same tween on `opacity`:

```python
def cursor() -> compose.Element:
    """A block cursor blinking on a loop on its opacity lane."""
    return (
        compose.box()
        .width(8)
        .height(14)
        .fill(ACCENT)
        .opacity(motion.animate(from_=1.0, to=0.0, duration=0.45, loop=-1, alternate=True))
    )
```

## Step 4 — typed rows: an entrance on each

A row is keyed by its sequence number, so when the scrollback is
described again a row already on screen keeps its picture and only a new
one mounts. A mounting row plays its entrances once: the row fades and
slides in, and its body types itself on — `textFx.entrance(textFx.typeOn())`
shows each glyph whole at its turn. The newest row carries the cursor.

```python
def log_row(sequence: int, tag: str, body: str, newest: bool = False) -> compose.Element:
    """One scrollback row, keyed by its sequence number. The body types
    itself on, a glyph at its turn, when the row mounts, and the newest row
    carries the cursor; a row already on screen keeps its picture when the
    slot is described again."""
    parts: list[compose.Element | compose.Text] = [
        label(f"{sequence:03d}", 12, DIM),
        label(f"[{tag}]", 12, TAG_INK.get(tag, CHROME)),
        label(body, 12, CHROME if tag == "TRACE" else BONE).textFx(
            textFx.entrance(textFx.typeOn())
        ),
    ]
    if newest:
        parts.append(cursor())
    row = (
        compose.box(*parts)
        .row()
        .gap(12)
        .alignItems("center")
        .key(f"row{sequence}")
        .opacity(motion.animate(from_=0, to=1, duration=0.35))
        .translateX(motion.animate(from_=-16, to=0, duration=0.3, ease=ease.outCubic))
    )
    if tag == "BREACH":
        row = row.fill("#2a0d12").padding(2, 6).borderRadius(2)
    return row
```

```python
    def scrollback(self, ctx: Scene2DContext) -> compose.Element | compose.Text:
        if not self.rows:
            return label("nothing has arrived on either door", 12, DIM)
        newest = self.rows[-1][0]
        return (
            compose.box(*(log_row(*row, newest=row[0] == newest) for row in self.rows))
            .column()
            .gap(8)
        )
```

`ROWS_SHOWN` keeps the newest rows, so the oldest row's node is dropped
as a new one mounts.

## Step 5 — the slots, described again only when something changed

The page holds a `compose.slot` for each part that changes shape. A
handler writes the model and names the slots it touched; once a frame,
`update` describes again only those, so a frame with no message
describes nothing.

```python
    def update(self, elapsed: float, ctx: Scene2DContext) -> None:
        states = self.states()
        if states != self.shown:
            self.shown = states
            self.changed.add("doors")
        drawn = {
            "log": self.scrollback,
            "alert": self.chip,
            "marquee": self.ticker,
            "cards": self.card_column,
            "meter": self.load_meter,
            "doors": self.door_readout,
        }
        for name in sorted(self.changed):
            self.composer.renderSlot(name, drawn[name](ctx))
        self.changed.clear()
```

`self.composer` is kept in `setup` because the context is valid only
during its callback; the composer lives as long as the session.

## Step 6 — a meter: a live value through a binding

A bar that follows a number does not need its slot described again on
every frame. The load is a live value; a meter message asks the engine
to ease it to the new load on the session's clock, and the bar's
`translateX` reads it through `bind`, so the bar glides with nothing
described.

```python
    def take_meter(self, message: data.Message) -> None:
        self.target = min(1.0, max(0.0, message["load"].number(self.target)))
        self.engine.animate(
            self.load,
            motion.Tween(to=self.target, duration=METER_EASE, ease=ease.inOutCubic),
        )
        self.changed.add("meter")
```

```python
    def load_meter(self, ctx: Scene2DContext) -> compose.Element:
        """The load, as a number and a bar. The bar is a full-width box
        slid out of a clipping track by the eased load, so a new reading
        glides there rather than jumps."""
        ink = ALERT if self.target >= 0.85 else ACCENT
        bar = (
            compose.box()
            .width(METER_WIDTH)
            .height(6)
            .fill(ink)
            .translateX(bind(self.load, to=(-METER_WIDTH, 0)))
            .key("meter-bar")
        )
        track = (
            compose.box(bar)
            .width(METER_WIDTH)
            .height(6)
            .borderRadius(3)
            .fill(RULE)
            .overflow(compose.Overflow.Clip)
        )
        heading = (
            compose.box(label("LOAD", 11, DIM), label(f"{self.target:.0%}", 11, ink))
            .row()
            .justifyContent("space_between")
        )
        return compose.box(heading, track).column().gap(8)
```

## Step 7 — the page: cards, panels, glow and scanlines

The layout is a web page's: a header column, a body row with a growing
log panel and a fixed side column, panels with a fill, a stroke and a
radius. The phosphor bloom on the log panel lifts its bright text the
way a tube does; an absolute, inset box of scanlines lies over the lot.

```python
def card(title: str, value: str, note: str, level: str) -> compose.Element:
    return (
        compose.box(
            label(title, 11, DIM),
            compose.text(value, size=30, color=LEVEL_INK[level]).fontFamily(MONO),
            label(note, 11, CHROME),
        )
        .column()
        .gap(6)
        .padding(14)
        .fill(PANEL)
        .stroke(material.from_(RULE), 1)
        .borderRadius(4)
        .key(f"card:{title}")
        .opacity(motion.animate(from_=0, to=1, duration=0.3))
    )
```

```python
    def page(self) -> compose.Element:
        header = (
            compose.box(
                compose.text("WARDNET", size=22, color=BONE)
                .fontFamily(MONO)
                .letterSpacing(6)
                .filter(material.Filter.glow(ACCENT, 8)),
                label("NODE / 07 · DRIVEN FROM OUTSIDE", 11, DIM),
                compose.box().width("100%").height(1).fill(RULE),
                compose.slot("marquee"),
            )
            .column()
            .gap(10)
        )
        log_panel = (
            compose.box(label("SCROLLBACK", 11, DIM), compose.slot("log"))
            .column()
            .gap(12)
            .padding(18)
            .fill(PANEL)
            .stroke(material.from_(RULE), 1)
            .borderRadius(4)
            .flexGrow(1)
            .overflow(compose.Overflow.Clip)
            .filter(
                material.Filter.phosphorBloom(radius=7, threshold=0.55, intensity=0.5)
            )
        )
        side = (
            compose.box(
                compose.slot("cards"),
                compose.slot("meter"),
                compose.slot("alert"),
                compose.slot("doors"),
            )
            .column()
            .gap(14)
            .width(METER_WIDTH)
        )
        body = compose.box(log_panel, side).row().gap(18).flexGrow(1)
        scanlines = (
            compose.box()
            .absolute()
            .inset(0)
            .fill(pattern.scanlines((0, 0, 0, 0.18), period=4.0, on=2.0))
        )
        return (
            compose.box(header, body, scanlines)
            .cover()
            .padding(28)
            .column()
            .gap(18)
            .fill(VOID)
        )
```

## Step 8 — run it

```sh
Grimoire wardnet_live.py                                    # live, listening on both doors
Grimoire wardnet_live.py --frame console.png --at 1         # two rows in, the marquee just moving
Grimoire wardnet_live.py --frame console.png --at 4         # the breach row typing, the chip mid-pulse
python3 sender.py                                           # from the sketch's directory: the show, live
```

No `--gpu`: this is a flat page, drawn by Compose on whatever backend
the host has.

## What to change next

- **Entrances per severity.** Give a breach row a shake, or a warning a
  longer slide; an entrance plays once when the row mounts.
- **A CRT.** Raise the scanline alpha, add a vignette as a radial
  gradient fill on another inset box, and give the log panel a
  `phosphorBloom` with a larger radius.
- **A title that glitches.** A keyframed `translateX` on the title with
  three short steps and `loop=-1, loopDelay=3.0` jumps it every few
  seconds and settles.

## The fine print

- **Loops belong on lanes, structure belongs in slots.** A looping tween
  never rebuilds the tree; a slot described again replaces one subtree
  and its keyed children keep their pictures. Describing the whole page
  on every message works but throws every cache away each time.
- **Keep the composer, not the context.** `ctx` is valid only inside the
  callback it was handed to; `ctx.composer` and `ctx.engine` outlive it.
- **A still is exact.** A capture replays the recording at its seconds
  on the session's clock, so `--at` shows the console as it stands at
  that second, every time.
- **Enumerations are enums here.** `overflow` takes
  `compose.Overflow.Clip` or `Visible`; the flex and alignment verbs take
  strings.
