"""A cyberpunk console: typed log lines, a marquee, a pulsing alert, phosphor glow."""

# TAGS: Compose/Motion, Typography/Console, Python

import math

from sigil import compose, material, motion
from sigil.material import pattern
from sigil.motion import ease
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


def label(content: str, size: float = 12, color: str = CHROME) -> compose.Element:
    return compose.text(content, size=size, color=color).fontFamily(MONO).letterSpacing(1.5)


def marquee(message: str) -> compose.Element:
    """A ticker: the message twice in a row, slid left by its own width, forever."""
    width = 1600
    run = compose.box(label(message, 13, ACCENT), label(message, 13, ACCENT)).row().gap(80)
    return (
        compose.box(run.translateX(motion.animate(from_=0, to=-width / 2 - 40, duration=14, loop=-1)))
        .height(28)
        .alignItems("center")
        .overflow(compose.Overflow.Clip)
    )


def alert_chip(content: str) -> compose.Element:
    """A chip whose fill breathes between two colours while it is live."""
    pulse = motion.animate(from_="#5a1a22", to=ALERT, duration=0.7, loop=-1, alternate=True)
    return (
        compose.box(label(content, 12, BONE))
        .padding(6, 12)
        .borderRadius(3)
        .fill(pulse)
        .filter(material.Filter.glow(ALERT, 10))
    )


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

    def meter(self, elapsed: float) -> compose.Element:
        load = 0.55 + 0.35 * math.sin(elapsed * 1.3) * math.sin(elapsed * 0.37)
        return (
            compose.box(compose.box().width(f"{load * 100:.0f}%").height(6).borderRadius(3).fill(ACCENT))
            .width("100%")
            .height(6)
            .borderRadius(3)
            .fill(RULE)
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
