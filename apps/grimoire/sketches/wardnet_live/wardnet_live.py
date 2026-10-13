"""The WardNet console, driven from outside: JSON over UDP or a WebSocket.

The console from wardnet_console.py, with nothing scripted inside it.
Every row in the scrollback, the alert chip, the marquee, the cards and
the load meter come from small JSON messages arriving through two doors:

    udp://:27030                 one message a datagram
    ws://:27031/wardnet          one message a WebSocket message; the same
                                 port serves pages/index.html, a page of
                                 buttons that sends every kind

Four kinds of message, named by "kind":

    {"kind": "row", "tag": "WARN", "body": "relay/05 jitter above threshold"}
    {"kind": "alert", "text": "BREACH · CHANNEL 9", "level": "breach"}
    {"kind": "meter", "load": 0.7}
    {"kind": "card", "title": "RELAY JITTER", "value": "41 ms", "note": "relay/05", "level": "warn"}

A tag is TRACE, INFO, WARN or BREACH; a level is clear, warn or breach.

    python3 sender.py                  # the scripted show over UDP
    python3 sender.py --door ws        # the same show over the WebSocket
    python3 sender.py --record         # the recording a capture replays

A window listens on both ports. A capture replays data/wardnet.feed at
the UDP door and a recording with nothing in it at the WebSocket door,
so a still opens no socket and is the same picture every run.

EDIT THESE FIRST: the doors, ROWS_SHOWN, CARDS_SHOWN, METER_EASE.

TAGS: Data/Sources, Runtime/Live Input, Compose/Motion, Typography/Console
"""

from math import ceil

from sigil import compose, data, material, motion
from sigil.material import pattern
from sigil.motion import animatable, bind, ease
from sigil.sketch import Scene2DContext, sketch

NAME = "WardNet, live"
CATEGORY = "Data"

UDP_PORT = 27030
WEBSOCKET_PORT = 27031
UDP_DOOR = f"udp://:{UDP_PORT}"
WEBSOCKET_PATH = "/wardnet"  # where the page's socket connects
PAGES = "pages"  # the directory the WebSocket port serves
RECORDING = "data/wardnet.feed"  # the show, replayed at the UDP door
SILENCE = "data/silence.feed"  # a recording with nothing in it

WIDTH, HEIGHT = 1280, 720
CAPTURE_AT = 4.0  # the breach is in and the last row is still arriving
ROWS_SHOWN = 18  # the scrollback keeps the newest this many
CARDS_SHOWN = 3  # the side column keeps the newest this many titles
METER_WIDTH = 300  # the side column's measure, and the meter's track
METER_EASE = 0.8  # seconds the meter takes to reach a new load
MARQUEE_SPEED = 70.0  # pixels a second the newest alert slides left

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

TAG_INK = {"TRACE": DIM, "INFO": ACCENT, "WARN": EMBER, "BREACH": ALERT}
LEVEL_INK = {"clear": ACCENT, "warn": EMBER, "breach": ALERT}
# How an alert breathes at each level: the fill it falls back to, and
# the seconds one breath out takes. A breach breathes fastest.
LEVEL_PULSE = {
    "clear": ("#123540", 1.4),
    "warn": ("#4a3312", 0.7),
    "breach": ("#5a1a22", 0.35),
}


def websocket_door(ctx: Scene2DContext) -> str:
    """The WebSocket door: the port and path a page's socket reaches, and
    the pages the same port serves, which stand beside this file."""
    return f"ws://:{WEBSOCKET_PORT}{WEBSOCKET_PATH}?pages={ctx.local(PAGES)}"


def label(content: str, size: float = 12, color: str = CHROME) -> compose.Element:
    return (
        compose.text(content, size=size, color=color)
        .fontFamily(MONO)
        .letterSpacing(1.5)
    )


def level_of(message: data.Message) -> str:
    """A message's level, read as clear where it names none or one unknown."""
    level = message["level"].string("clear").lower()
    return level if level in LEVEL_INK else "clear"


def log_row(sequence: int, tag: str, body: str) -> compose.Element:
    """One scrollback row, keyed by its sequence number. The entrance
    plays once, when the row mounts; a row already on screen keeps its
    picture when the slot is described again."""
    row = (
        compose.box(
            label(f"{sequence:03d}", 12, DIM),
            label(f"[{tag}]", 12, TAG_INK.get(tag, CHROME)),
            label(body, 12, CHROME if tag == "TRACE" else BONE),
        )
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


def alert_chip(content: str, level: str) -> compose.Element:
    """A chip whose fill breathes between two colours, faster the worse
    the level."""
    rest, breath = LEVEL_PULSE[level]
    ink = LEVEL_INK[level]
    pulse = motion.animate(from_=rest, to=ink, duration=breath, loop=-1, alternate=True)
    return (
        compose.box(label(content, 12, BONE))
        .padding(6, 12)
        .borderRadius(3)
        .fill(pulse)
        .filter(material.Filter.glow(ink, 10))
        .key("alert")
    )


def marquee_pass(message: str, ink: str) -> compose.Element:
    """One pass of the marquee: the alert, then the space before the next."""
    return (
        compose.box(label(message, 13, ink), label("···", 13, DIM))
        .row()
        .gap(40)
        .paddingRight(40)
    )


@sketch(size=(WIDTH, HEIGHT), background=VOID, capture_at=CAPTURE_AT)
class WardNetLive:
    def setup(self, ctx: Scene2DContext) -> None:
        self.composer = ctx.composer
        self.engine = ctx.engine
        hub = ctx.assets.hub()
        # THE WEBSOCKET DOOR serves the page beside this file on the same
        # port it listens on, so a browser opens http://HOST:27031/ and the
        # page's socket connects straight back.
        websocket = websocket_door(ctx)
        # A CAPTURE OPENS NO SOCKET: every later ask for a door's URI
        # answers the recording replayed there instead.
        self.replaying = ctx.deterministic
        if self.replaying:
            hub.replay(UDP_DOOR, ctx.local(RECORDING))
            hub.replay(websocket, ctx.local(SILENCE))

        # ONE SET OF HANDLERS, TWO DOORS. Both doors read JSON text, and a
        # handler is named by the message's "kind".
        self.doors = {
            "UDP": data.connect(hub, UDP_DOOR),
            "WEBSOCKET": data.connect(hub, websocket),
        }
        for door in self.doors.values():
            door.on("row", self.take_row)
            door.on("alert", self.take_alert)
            door.on("meter", self.take_meter)
            door.on("card", self.take_card)
            door.otherwise(self.take_stranger)

        # THE MODEL the messages write, and the slots each part is drawn in.
        self.sequence = 0
        self.rows: list[tuple[int, str, str]] = []
        self.alert = ("AWAITING TRAFFIC", "clear")
        self.cards: dict[str, tuple[str, str, str]] = {}
        self.target = 0.0
        self.strangers = 0
        # The live value the meter's bar follows. A meter message eases it
        # to the new load on the session's clock; the bar reads it through
        # a binding, so the bar moves without the page being described.
        self.load = animatable(0.0)
        self.changed = {"log", "alert", "marquee", "cards", "meter", "doors"}
        self.shown = self.states()
        ctx.render(self.page())
        self.update(0.0, ctx)

    # ── the handlers, run as each message is read ─────────────────────

    def take_row(self, message: data.Message) -> None:
        self.sequence += 1
        tag = message["tag"].string("INFO").upper()
        self.rows.append((self.sequence, tag, message["body"].string()))
        del self.rows[:-ROWS_SHOWN]
        self.changed.add("log")

    def take_alert(self, message: data.Message) -> None:
        self.alert = (message["text"].string("ALERT"), level_of(message))
        self.changed.update(("alert", "marquee"))

    def take_meter(self, message: data.Message) -> None:
        self.target = min(1.0, max(0.0, message["load"].number(self.target)))
        self.engine.animate(
            self.load,
            motion.Tween(to=self.target, duration=METER_EASE, ease=ease.inOutCubic),
        )
        self.changed.add("meter")

    def take_card(self, message: data.Message) -> None:
        title = message["title"].string("UNTITLED")
        self.cards.pop(title, None)  # a title sent again moves to the end
        self.cards[title] = (
            message["value"].string(),
            message["note"].string(),
            level_of(message),
        )
        while len(self.cards) > CARDS_SHOWN:
            self.cards.pop(next(iter(self.cards)))
        self.changed.add("cards")

    def take_stranger(self, message: data.Message) -> None:
        self.strangers += 1
        self.changed.add("doors")

    # ── the frame: describe again only the slots a message changed ────

    def states(self) -> dict:
        return {name: door.state() for name, door in self.doors.items()}

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

    def scrollback(self, ctx: Scene2DContext) -> compose.Element:
        if not self.rows:
            return label("nothing has arrived on either door", 12, DIM)
        return compose.box(*(log_row(*row) for row in self.rows)).column().gap(8)

    def chip(self, ctx: Scene2DContext) -> compose.Element:
        return alert_chip(*self.alert)

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

    def card_column(self, ctx: Scene2DContext) -> compose.Element:
        if not self.cards:
            return label("cards stand here once one arrives", 11, DIM)
        return (
            compose.box(*(card(title, *values) for title, values in self.cards.items()))
            .column()
            .gap(12)
        )

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

    def door_readout(self, ctx: Scene2DContext) -> compose.Element:
        """Each door's count of arrivals, and where a browser finds the page."""
        lines = [
            compose.box(
                label(f"{name} {self.doors[name].uri().split('?')[0]}", 10, DIM),
                label(
                    f"{state.revision}",
                    10,
                    ALERT if state.error else ACCENT if state.revision else DIM,
                ),
            )
            .row()
            .justifyContent("space_between")
            for name, state in self.shown.items()
        ]
        status = (
            "REPLAY" if self.replaying else f"OPEN http://localhost:{WEBSOCKET_PORT}/"
        )
        if self.strangers:
            status += f" · {self.strangers} NOT UNDERSTOOD"
        return compose.box(*lines, label(status, 10, CHROME)).column().gap(4)

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
