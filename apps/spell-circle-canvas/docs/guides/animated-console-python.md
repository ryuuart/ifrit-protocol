# An animated console in Python: typed lines, a marquee, a pulsing alert

A security console that behaves like a web page: a header with a ticker
that never stops, a scrollback that types its lines on with a blinking
cursor, status cards, a load meter that moves, an alert chip that
breathes, and scanlines over everything. It is one Python file of
ordinary Compose elements, and every moving part uses one of three
mechanisms. The whole file is `apps/grimoire/sketches/wardnet_console.py`;
open it with `Grimoire wardnet_console.py` and edit it live.

## The three mechanisms

Every animation on this page is one of these, and knowing which is which
is most of the craft.

| Mechanism | Who drives it | Use it for |
| --- | --- | --- |
| A tween on a lane, looping | the runtime | anything periodic: a marquee, a blink, a breathing chip |
| A slot re-rendered from a timer | you | anything whose structure changes: a line typing on, rows arriving, a meter |
| An entrance tween | the runtime | anything that happens once when a node mounts |

The page is described once in `setup`. The runtime owns the loops; a
timer owns the parts that change shape; nothing rebuilds the whole tree.

## Step 1 — the palette and a label

```python
import math

from sigil import compose, material, motion
from sigil.material import pattern
from sigil.sketch import Scene2DContext, sketch

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


def label(content: str, size: float = 12, color: str = CHROME) -> compose.Element:
    return compose.text(content, size=size, color=color).fontFamily(MONO).letterSpacing(1.5)
```

A console is a typographic object: one monospace family, a small set of
inks that mean something (dim for traces, accent for information, ember
for warnings, alert for breaches), and tracking on everything.

## Step 2 — a marquee: a loop on a lane

A ticker is the message twice in a row, slid left by its own width over
and over. `translateX` takes a tween; `loop=-1` makes it endless; the
parent clips what slides out of its box.

```python
def marquee(message: str) -> compose.Element:
    width = 1600
    run = compose.box(label(message, 13, ACCENT), label(message, 13, ACCENT)).row().gap(80)
    return (
        compose.box(run.translateX(motion.animate(from_=0, to=-width / 2 - 40, duration=14, loop=-1)))
        .height(28)
        .alignItems("center")
        .overflow(compose.Overflow.Clip)
    )
```

Nothing is stepped by hand. The tween lives on the node's lane, so the
tree is described once and the runtime slides it every frame.

## Step 3 — a pulsing alert: a loop that alternates

The same mechanism on a colour. A fill takes a colour tween; `loop=-1,
alternate=True` sends it back and forth; a glow filter spreads the
chip's light beyond its box.

```python
def alert_chip(content: str) -> compose.Element:
    pulse = motion.animate(from_="#5a1a22", to=ALERT, duration=0.7, loop=-1, alternate=True)
    return (
        compose.box(label(content, 12, BONE))
        .padding(6, 12)
        .borderRadius(3)
        .fill(pulse)
        .filter(material.Filter.glow(ALERT, 10))
    )
```

A blinking cursor is the same tween on `opacity`:

```python
compose.box()
.width(8)
.height(14)
.fill(ACCENT)
.opacity(motion.animate(from_=1.0, to=0.0, duration=0.45, loop=-1, alternate=True))
```

## Step 4 — typed lines: a slot re-rendered from a timer

Text typing on changes the tree's content, so a loop on a lane cannot do
it. The page holds a `slot("log")` where the scrollback goes; a timer
re-renders that slot a few times a second with the lines so far and the
current line cut to the characters typed. Rows are keyed, so a row
already on screen keeps its picture when a new one arrives.

```python
LINES = [
    ("TRACE", "handshake node/07 ← relay/02 · 14 ms"),
    ("INFO", "ward perimeter sweep complete · 0 breaches"),
    ("INFO", "cipher rotation scheduled · key 0x3f9a"),
    ("WARN", "relay/05 jitter above threshold · 41 ms"),
    ("INFO", "operator session resumed · console 3"),
    ("BREACH", "unsigned packet on channel 9 · quarantined"),
    ("INFO", "quarantine held · forensic capture armed"),
    ("TRACE", "heartbeat · all wards answering"),
]
TAG_INK = {"TRACE": DIM, "INFO": ACCENT, "WARN": EMBER, "BREACH": ALERT}


def log_row(index: int, tag: str, body: str, cursor: bool) -> compose.Element:
    ink = TAG_INK[tag]
    parts = [
        label(f"{index:03d}", 12, DIM),
        label(f"[{tag}]", 12, ink),
        label(body, 12, BONE if tag != "TRACE" else CHROME),
    ]
    if cursor:
        parts.append(
            compose.box()
            .width(8)
            .height(14)
            .fill(ACCENT)
            .opacity(motion.animate(from_=1.0, to=0.0, duration=0.45, loop=-1, alternate=True))
        )
    row = compose.box(*parts).row().gap(12).alignItems("center").key(f"row{index}")
    if tag == "BREACH":
        row = row.fill("#2a0d12").padding(2, 6).borderRadius(2)
    return row


@sketch(size=(WIDTH, HEIGHT), background=VOID, captureAt=4.0)
class WardNet:
    def setup(self, ctx: Scene2DContext) -> None:
        self.composer = ctx.composer
        self.rows: list[tuple[str, str]] = []     # finished lines
        self.typing = 0                           # characters of the current line shown
        self.line = 0                             # which line is being typed
        self.clock = 0.0
        ctx.engine.timer(self.tick, stepRate=30.0)
        ctx.render(self.page())

    def tick(self, delta: float, elapsed: float) -> None:
        """Type the current line on, two characters a step; start the next when done."""
        if self.line >= len(LINES):
            return
        tag, body = LINES[self.line]
        self.typing = min(len(body), self.typing + 2)
        if self.typing == len(body):
            self.rows.append((tag, body))
            self.line += 1
            self.typing = 0
        self.composer.renderSlot("log", self.log())
        self.composer.renderSlot("meter", self.meter(elapsed))

    def log(self) -> compose.Element:
        rows = [log_row(i, tag, body, False) for i, (tag, body) in enumerate(self.rows)]
        if self.line < len(LINES):
            tag, body = LINES[self.line]
            rows.append(log_row(len(self.rows), tag, body[: self.typing], True))
        return compose.box(*rows[-9:]).column().gap(8)
```

`ctx.engine.timer(callback, stepRate=30.0)` runs the callback thirty
times a second on the session's clock, handing it the step and its
elapsed seconds. `self.composer` is kept in `setup` because the context
is valid only during its callback; the composer lives as long as the
session. A still at `--at 4` shows exactly the lines typed by then,
because the timer steps on the same clock the capture advances.

## Step 5 — a meter: the same slot idiom for a value

A bar whose width follows a value is a structure change too, so it is a
slot: a box whose child's width is a percentage recomputed each tick.

```python
    def meter(self, elapsed: float) -> compose.Element:
        load = 0.55 + 0.35 * math.sin(elapsed * 1.3) * math.sin(elapsed * 0.37)
        return (
            compose.box(compose.box().width(f"{load * 100:.0f}%").height(6).borderRadius(3).fill(ACCENT))
            .width("100%")
            .height(6)
            .borderRadius(3)
            .fill(RULE)
        )
```

## Step 6 — the page: cards, panels, glow and scanlines

The layout is a web page's: a header column, a body row with a growing
log panel and a fixed side column, panels with a fill, a stroke and a
radius. The phosphor bloom on the log panel lifts its bright text the
way a tube does; an absolute, inset box of scanlines lies over the lot.

```python
def card(title: str, value: str, note: str, accent: str = ACCENT) -> compose.Element:
    return (
        compose.box(
            label(title, 11, DIM),
            compose.text(value, size=34, color=accent).fontFamily(MONO),
            label(note, 11, CHROME),
        )
        .column()
        .gap(6)
        .padding(16)
        .fill(PANEL)
        .stroke(material.from_(RULE), 1)
        .borderRadius(4)
    )


    def page(self) -> compose.Element:
        header = (
            compose.box(
                compose.text("WARDNET", size=22, color=BONE).fontFamily(MONO).letterSpacing(6)
                .filter(material.Filter.glow(ACCENT, 8)),
                label("NODE / 07 · PERIMETER WATCH", 11, DIM),
                compose.box().width("100%").height(1).fill(RULE),
                marquee("UNSIGNED PACKET QUARANTINED ON CHANNEL 9 ··· OPERATOR ACKNOWLEDGEMENT REQUIRED ··· "),
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
            .filter(material.Filter.phosphorBloom(radius=7, threshold=0.55, intensity=0.5))
        )
        side = (
            compose.box(
                card("WARDS ANSWERING", "12 / 12", "heartbeat 1.0 s"),
                card("RELAY JITTER", "41 ms", "relay/05 above threshold", EMBER),
                compose.box(label("LOAD", 11, DIM), compose.slot("meter")).column().gap(8),
                alert_chip("BREACH · CHANNEL 9"),
            )
            .column()
            .gap(14)
            .width(300)
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

## Step 7 — run it

```sh
Grimoire wardnet_console.py                               # live, swapped on every save
Grimoire wardnet_console.py --frame console.png --at 1    # two lines typed, marquee just moving
Grimoire wardnet_console.py --frame console.png --at 4    # the breach line in, the chip mid-pulse
```

No `--gpu`: this is a flat page, drawn by Compose on whatever backend
the host has.

## What to change next

- **Rows from a feed.** Replace `LINES` with rows arriving on a
  connection and the typing timer with an append per arrival; keyed rows
  mean an append mounts one row and leaves the rest cached.
- **Entrances per severity.** Give a breach row `.opacity(motion.animate(from_=0, to=1, duration=0.3))`
  and a warning `.translateX(motion.animate(from_=-12, to=0, duration=0.25))`;
  an entrance plays once when the row mounts.
- **A CRT.** Raise the scanline alpha, add a vignette as a radial
  gradient fill on another inset box, and put the whole page's filter to
  `phosphorBloom` with a larger radius.
- **A title that glitches.** A keyframed `translateX` on the title with
  three short steps and `loop=-1, loopDelay=3.0` jumps it every few
  seconds and settles.

## The fine print

- **Loops belong on lanes, structure belongs in slots.** A looping tween
  never rebuilds the tree; a slot re-render replaces one subtree and its
  keyed children keep their pictures. Re-rendering the whole page from a
  timer works but throws every cache away each tick.
- **Keep the composer, not the context.** `ctx` is valid only inside the
  callback it was handed to; `ctx.composer` and `ctx.engine` outlive it.
- **A still is exact.** The timer steps on the session's clock, so
  `--at` shows the console as it stands at that second, every time.
- **Enumerations are enums here.** `overflow` takes
  `compose.Overflow.Clip` or `Visible`; the flex and alignment verbs take
  strings.
