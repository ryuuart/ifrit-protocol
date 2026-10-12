"""A show's cues from a TouchDesigner patch: FlatBuffers over UDP, read through the sketch's own schema.

Every datagram on UDP port 27025 is one Cue as cue_deck.fbs beside this
file states it: the show's name, a level per channel, the colour the cue
is lit in, the markers on the bar and the beat the show stands on. The
build compiles that schema into cue_deck.bfbs, and the door reads every
arrival through it, so the page reads each cue as the schema's JSON form
and counts an arrival that is no cue rather than drawing it.

    python3 sender.py            # cues to port 27025, ten a second
    python3 sender.py --record   # the recording a capture replays

touchdesigner/scripts/cue_deck.py sends the same bytes from a patch.
A window listens; a capture replays data/cue.feed at the same URI, so a
still is the same picture every run.

EDIT THESE FIRST: PORT, the canvas, METER_HEIGHT, MARQUEE_SPEED, and
cue_deck.fbs for what a cue carries.

TAGS: Data/Schema, Runtime/Live Input, Data/Sources
"""

from math import ceil

from sigil import data
from sigil.compose import Element, Overflow, box, column, row, text
from sigil.compose import document as doc
from sigil.io import ReadyState
from sigil.motion import animatable, bind
from sigil.sketch import Scene2DContext, kit, sketch

NAME = "Cue deck"
CATEGORY = "Data"

PORT = 27025
DOOR = f"udp://:{PORT}"
SCHEMA = "cue_deck.bfbs"
RECORDING = "data/cue.feed"

WIDTH, HEIGHT = 1280, 800
CAPTURE_AT = 3.0  # by now thirty cues have landed and the playhead has moved
CONTENT = 1184  # the page's measure
SIDE = 300  # the door's column
METER_HEIGHT = 250
MARQUEE_HEIGHT = 92
MARQUEE_SPEED = 90.0  # pixels a second the show's name slides left
BAR = 16  # the beats the ruler spans
RULER = CONTENT - 48  # the ruler's track, inside its well's padding
WAITING = "#e2bb74"
LIVE = "#87cfb5"
TROUBLE = "#d09476"


def hex_of(color: data.Json) -> str:
    """A cue's colour as the hex a fill takes; white for a cue with none."""
    if color.null():
        return "#ffffff"
    channels = (color[name].number(1) for name in ("r", "g", "b"))
    return "#" + "".join(
        f"{round(min(1, max(0, value)) * 255):02x}" for value in channels
    )


def show_run(show: str, color: str) -> Element:
    """One pass of the marquee: the name, then the space before the next."""
    return (
        row()
        .gap(48)
        .alignItems("center")
        .paddingRight(48)
        .children(
            text(show, size=54, color=color).letterSpacing(4),
            box().width(10).height(10).borderRadius(5).fill(color),
        )
    )


def meter(name: str, level: float, color: str, look: kit.Theme) -> Element:
    """One channel: its level as a number, a bar filled to it, its name."""
    level = min(1.0, max(0.0, level))
    return (
        column()
        .gap(12)
        .alignItems("center")
        .flexGrow(1)
        .flexBasis(0)
        .children(
            text(f"{level:.0%}").styleClass("readout").fontSize(24),
            box()
            .width(64)
            .height(METER_HEIGHT)
            .borderRadius(8)
            .fill(look.palette.rule)
            .overflow(Overflow.Clip)
            .children(
                box()
                .absolute()
                .left(0)
                .top(METER_HEIGHT * (1 - level))
                .width(64)
                .height(METER_HEIGHT * level)
                .fill(color)
            ),
            doc.label(name),
        )
    )


def ruler(
    markers: list[data.Json], beat: float, color: str, look: kit.Theme
) -> Element:
    """The bar: a tick a beat, every marker flagged, the playhead on the beat."""
    step = RULER / BAR
    ticks = [
        box()
        .absolute()
        .left(index * step)
        .top(30 if index % 4 else 22)
        .width(1)
        .height(18 if index % 4 else 26)
        .fill(look.palette.rule)
        for index in range(BAR + 1)
    ]
    flags = [
        column()
        .absolute()
        .left(marker["beat"].number() % BAR * step)
        .top(0)
        .gap(4)
        .children(
            text(marker["name"].string(), size=11, color=color).letterSpacing(1.5),
            box().width(2).height(44).fill(color),
        )
        for marker in markers
    ]
    playhead = (
        box()
        .absolute()
        .left(beat % BAR * step - 1)
        .top(14)
        .width(3)
        .height(50)
        .borderRadius(1.5)
        .fill(look.palette.figure)
    )
    return box().width(RULER).height(64).children(*ticks, *flags, playhead)


def door_row(label: str, value: str, ink=None) -> Element:
    reading = text(value).styleClass("control")
    if ink is not None:
        reading = reading.ink(ink)
    return (
        row()
        .justifyContent("space_between")
        .width(SIDE - 48)
        .children(doc.label(label), reading)
    )


@sketch(size=(WIDTH, HEIGHT), capture_at=CAPTURE_AT)
class Deck:
    def setup(self, ctx: Scene2DContext) -> None:
        self.look = kit.feature_theme(kit.Density.Spacious)
        with kit.provide(self.look):
            kit.stage(ctx, size=(WIDTH, HEIGHT), capture_at=CAPTURE_AT)
        hub = ctx.assets.hub()
        # THE SCHEMA THE BUILD WROTE beside this file from cue_deck.fbs.
        binary = hub.read(ctx.local(SCHEMA))
        if binary is None:
            raise RuntimeError(
                f"{SCHEMA} is missing: build the SigilSketchSchemas target"
            )
        # A CAPTURE READS THE RECORDING, a window opens the door: every
        # later ask for the URI answers the replaying feed.
        self.replaying = ctx.deterministic
        if self.replaying:
            hub.replay(DOOR, ctx.local(RECORDING))
        self.door = data.connect(hub, DOOR, schema=data.Schema.fromBinarySchema(binary))
        self.shown = self.door.state()
        # One clock for the marquee: the name slides on a binding of it, so
        # nothing is described again between two cues.
        self.clock = animatable(0)
        ctx.engine.timer(lambda delta, elapsed: self.clock.set(elapsed))
        self.describe(ctx)

    def update(self, elapsed: float, ctx: Scene2DContext) -> None:
        """A door whose vitals moved has a new cue, or a new state to show."""
        state = self.door.state()
        if state != self.shown:
            self.shown = state
            self.describe(ctx)

    def describe(self, ctx: Scene2DContext) -> None:
        # The newest cue whole, kept by the door until the next replaces it.
        cue = self.door.latest().payload
        with kit.provide(self.look):
            ctx.render(
                kit.page(
                    self.content(ctx, cue),
                    title="The cue deck.",
                    subtitle="A show's cues, sent from a TouchDesigner patch as FlatBuffers over UDP",
                    footer=f"UDP {PORT} · cue_deck.fbs · sender.py · touchdesigner/scripts/cue_deck.py",
                )
            )

    def content(self, ctx: Scene2DContext, cue: data.Json) -> Element:
        color = hex_of(cue["color"])
        channels = cue["channels"].array()
        return (
            column()
            .width(CONTENT)
            .gap(18)
            .children(
                self.marquee(ctx, cue["show"].string("WAITING FOR A CUE"), color),
                row()
                .gap(18)
                .width(CONTENT)
                .alignItems("stretch")
                .children(
                    kit.well(padding=24, corners=6)
                    .row()
                    .gap(12)
                    .flexGrow(1)
                    .alignItems("end")
                    .children(
                        *(
                            meter(
                                channel["name"].string(),
                                channel["level"].number(),
                                color,
                                self.look,
                            )
                            for channel in channels
                        )
                        if channels
                        else [doc.caption("The meters stand here once a cue arrives.")]
                    ),
                    self.door_column(color),
                ),
                kit.well(padding=24, corners=6)
                .column()
                .gap(12)
                .children(
                    row()
                    .justifyContent("space_between")
                    .width(RULER)
                    .children(
                        doc.label("THE BAR"),
                        text(f"BEAT {cue['beat'].number():04.1f} / {BAR}").styleClass(
                            "control"
                        ),
                    ),
                    ruler(
                        cue["markers"].array(), cue["beat"].number(), color, self.look
                    ),
                ),
            )
        )

    def marquee(self, ctx: Scene2DContext, show: str, color: str) -> Element:
        """The show's name, sliding left forever. One pass is measured, and
        the row holds enough passes that one always covers the band; the
        clock's binding wraps at one pass, so the slide has no seam."""
        run = ctx.measure(show_run(show, color)).width()
        passes = ceil(CONTENT / run) + 2
        return (
            kit.well(padding=0, corners=6, clip=True)
            .height(MARQUEE_HEIGHT)
            .width(CONTENT)
            .children(
                row()
                .absolute()
                .left(-run)
                .top(0)
                .height(MARQUEE_HEIGHT)
                .alignItems("center")
                .translateX(bind(self.clock, to=(0, -MARQUEE_SPEED), wrap=run))
                .children(*(show_run(show, color) for _ in range(passes)))
            )
        )

    def door_column(self, color: str) -> Element:
        """The door's own account of itself, read from its state."""
        state = self.shown
        status, ink = self.status()
        return (
            kit.well(padding=24, corners=6)
            .column()
            .gap(14)
            .width(SIDE)
            .children(
                doc.label("THE DOOR"),
                door_row("STATE", status, ink),
                door_row("ADDRESS", state.localAddress or DOOR),
                door_row("CUES", f"{state.revision}"),
                door_row("NOT A CUE", f"{state.undecodable}"),
                door_row("DROPPED", f"{state.dropped}"),
                row()
                .gap(10)
                .alignItems("center")
                .children(
                    box().width(18).height(18).borderRadius(4).fill(color),
                    text(color.upper()).styleClass("control"),
                ),
                doc.caption(
                    self.waiting_line()
                    if self.door.latest().empty()
                    else "The newest cue is drawn whole."
                ),
            )
        )

    def status(self) -> tuple[str, str]:
        state = self.shown
        if state.error:
            return "UNAVAILABLE", TROUBLE
        if self.replaying:
            return "REPLAY", LIVE if state.revision else WAITING
        if state.readiness == ReadyState.Closed:
            return "CLOSED", TROUBLE
        if state.readiness == ReadyState.Connecting:
            return "OPENING", WAITING
        return ("LISTENING", LIVE) if state.revision else ("WAITING", WAITING)

    def waiting_line(self) -> str:
        state = self.shown
        if state.error:
            return state.error
        if state.undecodable:
            return f"{state.undecodable} arrivals were no cue"
        if state.readiness == ReadyState.Closed:
            return "The door is closed: nothing else is coming."
        return f"Waiting at {state.localAddress or DOOR}: nothing has arrived."
