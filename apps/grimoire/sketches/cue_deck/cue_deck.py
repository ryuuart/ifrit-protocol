"""A show's cues from a TouchDesigner patch: JSON, OSC or a FlatBuffer over UDP.

The same cue can arrive through three doors, and each reads it into the
same data.Json: the show's name, a level per channel, the colour the cue
is lit in, the markers on the bar and the beat the show stands on.

    udp://:27025   JSON text, one datagram a cue, read as it arrives
    osc://:27026   OSC messages, one value each, gathered into a cue
    udp://:27027   a FlatBuffer, read through cue_deck.fbs, the schema
                   the build compiles into cue_deck.bfbs beside this file

The page draws the newest cue, whichever door it came through, and says
which one that was.

    python3 sender.py                       # JSON cues, ten a second
    python3 sender.py --wire osc            # the same cues over OSC
    python3 sender.py --wire flatbuffers    # the same cues as FlatBuffers
    python3 sender.py --record              # the recording a capture replays

touchdesigner/scripts/cue_deck.py sends the same cues from a patch. A
window listens; a capture replays data/cue.feed at the JSON door and a
recording with nothing in it at the other two, so a still opens no socket
and is the same picture every run.

EDIT THESE FIRST: the doors, the canvas, METER_HEIGHT, MARQUEE_SPEED.

TAGS: Data/Schema, Runtime/Live Input, Data/Sources
"""

from math import ceil

from sigil import data
from sigil.compose import Element, Overflow, box, column, row, text, var
from sigil.compose import document
from sigil.compose import kit as neutral
from sigil.io import ReadyState
from sigil.motion import animatable, bind
from sigil.sketch import Scene2DContext, kit, sketch

NAME = "Cue deck"
CATEGORY = "Data"

JSON_DOOR = "udp://:27025"  # a cue as JSON text
OSC_DOOR = "osc://:27026"  # a cue as OSC messages, one value each
SCHEMA_DOOR = "udp://:27027"  # a cue as a FlatBuffer
SCHEMA = "cue_deck.bfbs"
RECORDING = "data/cue.feed"  # JSON cues, replayed at the JSON door
SILENCE = "data/silence.feed"  # a recording with nothing in it

WIDTH, HEIGHT = 1280, 800
CAPTURE_AT = 3.0  # by now thirty cues have landed and the playhead has moved
CONTENT = 1184  # the page's measure
SIDE = 300  # the doors' column
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


def meter(name: str, level: float, color: str) -> Element:
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
            .fill(var("palette-rule"))
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
            document.label(name),
        )
    )


def ruler(
    markers: list[data.Json], beat: float, color: str
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
        .fill(var("palette-rule"))
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
        .fill(var("palette-figure"))
    )
    return box().width(RULER).height(64).children(*ticks, *flags, playhead)


@sketch(
    size=(WIDTH, HEIGHT),
    captureAt=CAPTURE_AT,
    styleSheet=kit.featureStyleSheet(kit.Density.Spacious),
)
class Deck:
    def setup(self, ctx: Scene2DContext) -> None:
        hub = ctx.assets.hub()
        # A CAPTURE OPENS NO SOCKET: every later ask for a door's URI
        # answers the recording replayed there instead.
        self.replaying = ctx.deterministic
        if self.replaying:
            hub.replay(JSON_DOOR, ctx.local(RECORDING))
            hub.replay(OSC_DOOR, ctx.local(SILENCE))
            hub.replay(SCHEMA_DOOR, ctx.local(SILENCE))

        # JSON TEXT: every datagram is read as one data.Json.
        self.json_door = data.connect(hub, JSON_DOOR)

        # OSC: one message a value, gathered into the cue they add up to.
        self.osc_door = data.connect(hub, OSC_DOOR)
        self.gathered = {"channels": {}, "color": {}, "markers": {}}
        self.osc_door.on("/cue/show", self.take_show)
        self.osc_door.on("/cue/level/*", self.take_level)
        self.osc_door.on("/cue/color/{r,g,b}", self.take_color)
        self.osc_door.on("/cue/marker", self.take_marker)
        self.osc_door.on("/cue/beat", self.take_beat)

        # A FLATBUFFER, read through the schema the build compiled from
        # cue_deck.fbs, into the same JSON form the JSON door reads.
        binary = hub.read(ctx.local(SCHEMA))
        if binary is None:
            raise RuntimeError(
                f"{SCHEMA} is missing: build the SigilSketchSchemas target"
            )
        schema = data.Schema.fromBinarySchema(binary)
        self.schema_door = data.connect(hub, SCHEMA_DOOR, schema=schema)

        self.doors = {
            "JSON": self.json_door,
            "OSC": self.osc_door,
            "FLATBUFFER": self.schema_door,
        }
        self.shown = self.states()
        # One clock for the marquee: the name slides on a binding of it, so
        # nothing is described again between two cues.
        self.clock = animatable(0)
        ctx.engine.timer(lambda delta, elapsed: self.clock.set(elapsed))
        self.redraw(ctx)

    # ── the OSC door's handlers, run as each message is read ──────────

    def take_show(self, message: data.Message) -> None:
        self.gathered["show"] = message.string()

    def take_level(self, message: data.Message) -> None:
        name = message.address().rsplit("/", 1)[1]
        self.gathered["channels"][name] = message.number()

    def take_color(self, message: data.Message) -> None:
        name = message.address().rsplit("/", 1)[1]
        self.gathered["color"][name] = message.number()

    def take_marker(self, message: data.Message) -> None:
        self.gathered["markers"][message[0].string()] = message[1].number()

    def take_beat(self, message: data.Message) -> None:
        self.gathered["beat"] = message.number()

    def osc_cue(self) -> data.Json:
        """What the OSC messages add up to, in the shape the other two
        doors' cues arrive in."""
        gathered = self.gathered
        cue = {
            "channels": [
                {"name": name, "level": level}
                for name, level in gathered["channels"].items()
            ],
            "markers": [
                {"name": name, "beat": beat}
                for name, beat in gathered["markers"].items()
            ],
        }
        if gathered["color"]:
            cue["color"] = gathered["color"]
        for field in ("show", "beat"):
            if field in gathered:
                cue[field] = gathered[field]
        return data.Json(cue)

    # ── the page ──────────────────────────────────────────────────────

    def states(self) -> dict:
        return {name: door.state() for name, door in self.doors.items()}

    def update(self, elapsed: float, ctx: Scene2DContext) -> None:
        """A door whose vitals moved has a new cue, or a new state to show."""
        states = self.states()
        if states != self.shown:
            self.shown = states
            self.redraw(ctx)

    def newest(self) -> tuple[str, data.Json]:
        """The door the newest message came through, and the cue it holds."""
        arrived = [
            (door.latest().receivedAt(), name)
            for name, door in self.doors.items()
            if not door.latest().empty()
        ]
        if not arrived:
            return "", data.Json()
        name = max(arrived)[1]
        if name == "OSC":
            return name, self.osc_cue()
        return name, self.doors[name].latest().payload

    def redraw(self, ctx: Scene2DContext) -> None:
        door, cue = self.newest()
        ctx.render(
            kit.page(
                self.content(ctx, cue, door),
                title="The cue deck.",
                subtitle="A show's cues from a TouchDesigner patch, as JSON, OSC or a FlatBuffer over UDP",
                footer="JSON udp 27025 · OSC 27026 · FlatBuffer udp 27027 · sender.py · touchdesigner/scripts/cue_deck.py",
            )
        )

    def content(self, ctx: Scene2DContext, cue: data.Json, door: str) -> Element:
        color = hex_of(cue["color"])
        channels = cue["channels"].array()
        return (
            column()
            .width(CONTENT)
            .gap(18)
            .children(
                self.marquee(
                    ctx,
                    cue["show"].string(
                        "WAITING FOR A CUE" if cue.null() else "A SHOW WITH NO NAME"
                    ),
                    color,
                ),
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
                            )
                            for channel in channels
                        )
                        if channels
                        else [
                            document.caption(
                                "The meters stand here once a cue arrives."
                            )
                        ]
                    ),
                    self.door_column(color, door),
                ),
                kit.well(padding=24, corners=6)
                .column()
                .gap(12)
                .children(
                    row()
                    .justifyContent("space-between")
                    .width(RULER)
                    .children(
                        document.label("THE BAR"),
                        text(f"BEAT {cue['beat'].number():04.1f} / {BAR}").styleClass(
                            "control"
                        ),
                    ),
                    ruler(cue["markers"].array(), cue["beat"].number(), color),
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

    def door_column(self, color: str, door: str) -> Element:
        """The account the drawn cue's door gives of itself, through the
        kit's readout, the other doors' counts after it, and the colour the
        cue is lit in."""
        status, ink = self.status()
        shown = door or "JSON"
        others = [
            neutral.Reading(name=name.lower(), value=f"{state.revision}", ink=self.ink(state))
            for name, state in self.shown.items()
            if name != shown
        ]
        return (
            kit.well(padding=24, corners=6)
            .column()
            .gap(14)
            .width(SIDE)
            .children(
                document.label("THE DOORS"),
                kit.connectionReadout(
                    self.doors[shown],
                    door=shown,
                    rows=neutral.Rows(measure=SIDE - 48, gap=8),
                    readings=[
                        *others,
                        neutral.Reading(name="status", value=status, ink=ink),
                    ],
                ),
                row()
                .gap(10)
                .alignItems("center")
                .children(
                    box().width(18).height(18).borderRadius(4).fill(color),
                    text(color.upper()).styleClass("control"),
                ),
                document.caption(
                    f"The newest cue came through the {door} door, "
                    f"{self.doors[door].uri()}."
                    if door
                    else self.waiting_line()
                ),
            )
        )

    @staticmethod
    def ink(state: data.ConnectionState) -> str:
        if state.error:
            return TROUBLE
        return LIVE if state.revision else WAITING

    def status(self) -> tuple[str, str]:
        states = self.shown.values()
        if any(state.error for state in states):
            return "UNAVAILABLE", TROUBLE
        arrived = any(state.revision for state in states)
        if self.replaying:
            return "REPLAY", LIVE if arrived else WAITING
        if all(state.readiness == ReadyState.Closed for state in states):
            return "CLOSED", TROUBLE
        return ("LISTENING", LIVE) if arrived else ("WAITING", WAITING)

    def waiting_line(self) -> str:
        for state in self.shown.values():
            if state.error:
                return state.error
        undecodable = sum(state.undecodable for state in self.shown.values())
        if undecodable:
            return f"{undecodable} arrivals were no cue."
        return "Waiting on all three doors: nothing has arrived."
