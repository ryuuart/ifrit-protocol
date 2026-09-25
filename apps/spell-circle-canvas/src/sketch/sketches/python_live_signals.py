"""A live signal observatory: JSON arrivals, native traces, and a reply to each sender.

Open in Sketchbook, then run with the matching installed package:
  python -m sigil.examples.tools.send_live_signals --export reply.json

Send {"sequence": 1, "pressure": 0.62, "flow": 0.35} to UDP port 27021.
Both channels are normalized from 0 to 1. Edit PORT, colors or describe().
Headless captures replay synthetic arrivals and never open a socket.

TAGS: Data/Visualization, Runtime/Live Input, Drawing/Charts
"""

from collections import deque
from dataclasses import dataclass
from json import dumps
from math import isfinite, sin

from sigil.compose import Element, box, column, graphics, row, text
from sigil.compose import document as doc
from sigil.data import decodeJson
from sigil.draw import CENTER, LEFT, RIGHT, Pen
from sigil.io import Feed, FeedPolicy, ReadyState
from sigil.io.testing import inletOf
from sigil.material import Color
from sigil.sketch import SketchContext, kit, sketch
from sigil.skia import Path, PathBuilder

PORT = 27021
MINT, GOLD = "#87cfb5", "#e2bb74"
TRACE_WIDTH, TRACE_HEIGHT = 606, 198


@dataclass(frozen=True)
class Signal:
    sequence: int
    pressure: float
    flow: float


def reference(sequence: int) -> Signal:
    t = sequence / 30
    return Signal(
        sequence,
        0.55 + 0.22 * sin(t * 2.1) + 0.06 * sin(t * 5.2),
        0.44 + 0.24 * sin(t * 1.4 + 0.8) + 0.08 * sin(t * 3.7),
    )


def encoded(signal: Signal) -> bytes:
    return dumps(
        {
            "sequence": signal.sequence,
            "pressure": round(signal.pressure, 4),
            "flow": round(signal.flow, 4),
        }
    ).encode()


def decoded(payload: bytes) -> Signal:
    document = decodeJson(payload.decode("utf-8"))
    if document is None:
        raise ValueError("The datagram is not valid JSON.")
    value = document.to_python()
    if not isinstance(value, dict):
        raise TypeError("Send a JSON object with sequence, pressure and flow.")
    sequence = value.get("sequence")
    if (
        isinstance(sequence, bool)
        or not isinstance(sequence, (int, float))
        or not isfinite(sequence)
        or sequence < 0
        or not float(sequence).is_integer()
    ):
        raise ValueError("sequence must be a nonnegative integer.")
    channels: list[float] = []
    for name in ("pressure", "flow"):
        number = value.get(name)
        if isinstance(number, bool) or not isinstance(number, (int, float)):
            raise TypeError(f"{name} must be a number from 0 to 1.")
        if not isfinite(number) or not 0 <= number <= 1:
            raise ValueError(f"{name} must be finite and between 0 and 1.")
        channels.append(float(number))
    return Signal(int(sequence), channels[0], channels[1])


def trace(values: list[float]) -> Path:
    path = PathBuilder()
    for index, value in enumerate(values):
        x = 18 + index * (TRACE_WIDTH - 36) / max(1, len(values) - 1)
        y = 22 + (1 - value) * (TRACE_HEIGHT - 58)
        if index == 0:
            path.moveTo(x, y)
        else:
            path.lineTo(x, y)
    return path.detach()


def metric(label: str, value: str, detail: str, accent: str) -> Element:
    return (
        kit.well(padding=21, corners=6)
        .column()
        .gap(10)
        .height(142)
        .flexGrow(1)
        .flexBasis(0)
        .children(
            (
                row()
                .gap(8)
                .alignItems("center")
                .children(
                    (box().width(6).height(6).borderRadius(3).fill(accent)),
                    doc.label(label),
                )
            ),
            text(value).styleClass("readout").fontSize(43),
            doc.caption(detail),
        )
    )


@sketch(size=(1100, 720), capture_at=4.0)
class LiveSignals:
    def setup(self, ctx: SketchContext) -> None:
        self.look = kit.feature_theme(kit.Density.Spacious)
        with kit.provide(self.look):
            kit.stage(ctx, size=(1100, 720), capture_at=4.0)
        self.replay = ctx.deterministic
        self.samples: deque[Signal] = deque(
            (reference(index) for index in range(180)), maxlen=180
        )
        self.received = 0
        self.replies = 0
        self.rejected = 0
        self.last_arrival = -1.0
        self.last_description = -1.0
        self.problem = ""
        self.peer = "No sender yet"
        policy = FeedPolicy(capacity=256)
        if self.replay:
            # A capture plays a scripted signal: the frames are delivered
            # through the feed's inlet as their moments pass, the way a
            # transport would deliver them.
            self.feed = Feed("replay://live-signals", policy)
            self.inlet = inletOf(self.feed)
            self.pending: deque[tuple[float, bytes]] = deque(
                (index / 30, encoded(reference(index))) for index in range(241)
            )
        else:
            self.feed = ctx.assets.hub().feed(f"udp://:{PORT}", policy)
        self.rebuild_traces()
        with kit.provide(self.look):
            ctx.render(self.describe(0))

    def update(self, elapsed: float, ctx: SketchContext) -> None:
        if self.replay:
            while self.pending and self.pending[0][0] <= elapsed:
                arrivedAt, payload = self.pending.popleft()
                self.inlet.deliver(payload, arrivedAt=arrivedAt)
        changed = False
        for _ in range(64):
            arrival = self.feed.receive()
            if arrival is None:
                break
            try:
                signal = decoded(arrival.payload)
            except (TypeError, ValueError, RuntimeError) as error:
                self.rejected += 1
                self.problem = (
                    str(error).partition("\n")[0][:90]
                    or "Could not decode incoming JSON."
                )
                continue
            if self.received == 0:
                self.samples.clear()
            self.samples.append(signal)
            self.received += 1
            self.last_arrival = elapsed
            self.peer = arrival.sender or "Synthetic recording"
            self.problem = ""
            changed = True
            if not self.replay:
                reply = dumps(
                    {
                        "sequence": signal.sequence,
                        "accepted": True,
                        "samples": self.received,
                        "mean_pressure": round(
                            sum(sample.pressure for sample in self.samples)
                            / len(self.samples),
                            3,
                        ),
                    }
                ).encode()
                if self.feed.send(reply, to=arrival.sender):
                    self.replies += 1
                else:
                    self.problem = (
                        "The arrival was accepted, but its reply could not be sent."
                    )
        if changed:
            self.rebuild_traces()
        if elapsed - self.last_description >= 0.1:
            self.last_description = elapsed
            with kit.provide(self.look):
                ctx.render(self.describe(elapsed))

    def rebuild_traces(self) -> None:
        self.pressure_path = trace([sample.pressure for sample in self.samples])
        self.flow_path = trace([sample.flow for sample in self.samples])

    def state(self, elapsed: float) -> tuple[str, str | Color, str]:
        state = self.feed.state()
        if state.error:
            return "UNAVAILABLE", "#d09476", state.error
        if self.problem:
            return "CHECK INPUT", "#d09476", self.problem
        if self.replay:
            return (
                "REPLAY",
                "#92b4e4",
                "Synthetic recording. No socket is open; no reply is sent.",
            )
        if state.readiness == ReadyState.Closed:
            return (
                "CLOSED",
                self.look.palette.ash,
                "The feed is closed. Save the sketch to open a fresh session.",
            )
        if not state.isOpen():
            return (
                "OPENING",
                self.look.palette.ash,
                "The native transport is opening the listener.",
            )
        if not self.received:
            return (
                "WAITING",
                GOLD,
                f"Listening on UDP {PORT}. Reference traces are shown until data arrives.",
            )
        if elapsed - self.last_arrival > 2:
            return (
                "QUIET",
                GOLD,
                "No recent arrivals. The last received samples remain visible.",
            )
        return (
            "LIVE",
            MINT,
            f"Receiving from {self.peer}. Every valid arrival gets a JSON acknowledgment.",
        )

    def describe(self, elapsed: float) -> Element:
        return kit.page(
            self.content(elapsed),
            title="Signals, received.",
            subtitle="Two channels, one JSON message, and a reply to every sender",
            footer=f"UDP {PORT} · sigil.examples.tools.send_live_signals · --export reply.json",
        )

    def content(self, elapsed: float) -> Element:
        status, accent, detail = self.state(elapsed)
        latest = self.samples[-1]
        return (
            column()
            .width(1004)
            .gap(16)
            .children(
                row(
                    doc.label("JSON IN · JSON OUT"),
                    doc.label(status).ink(accent),
                ).justifyContent("space_between"),
                (
                    row()
                    .gap(16)
                    .width(1004)
                    .children(
                        metric(
                            "PRESSURE",
                            f"{latest.pressure:.0%}" if self.received else "—",
                            "Normalized input / 0–1",
                            MINT,
                        ),
                        metric(
                            "FLOW",
                            f"{latest.flow:.0%}" if self.received else "—",
                            "Normalized input / 0–1",
                            GOLD,
                        ),
                        metric(
                            "ARRIVALS / REPLIES",
                            f"{self.received:03} / {self.replies:03}",
                            f"{self.rejected} invalid · {self.feed.dropped()} queue drops",
                            "#92b4e4",
                        ),
                    )
                ),
                (
                    row()
                    .gap(16)
                    .width(1004)
                    .alignItems("stretch")
                    .children(
                        (
                            kit.well(padding=24, corners=6)
                            .column()
                            .gap(14)
                            .width(654)
                            .children(
                                (
                                    row()
                                    .width(606)
                                    .justifyContent("space_between")
                                    .children(
                                        doc.label("RECENT ARRIVALS"),
                                        (
                                            row()
                                            .gap(8)
                                            .children(
                                                text(
                                                    "PRESSURE",
                                                    size=10,
                                                    color=MINT,
                                                ),
                                                text(
                                                    "/",
                                                    size=10,
                                                    color=self.look.palette.ash,
                                                ),
                                                text(
                                                    "FLOW",
                                                    size=10,
                                                    color=GOLD,
                                                ),
                                            )
                                        ),
                                    )
                                ),
                                (
                                    graphics("live-signals-traces", self.plot)
                                    .width(TRACE_WIDTH)
                                    .height(TRACE_HEIGHT)
                                ),
                                (
                                    text(
                                        "Last 180 samples"
                                        if self.received
                                        else "REFERENCE SIGNAL · awaiting your data",
                                        size=10,
                                        color=self.look.palette.ash,
                                    )
                                ),
                            )
                        ),
                        (
                            kit.well(padding=24, corners=6)
                            .column()
                            .gap(16)
                            .width(334)
                            .children(
                                doc.h2("A small contract."),
                                (
                                    doc.code(
                                        '{\n  "sequence": 1,\n  "pressure": 0.62,\n  "flow": 0.35\n}'
                                    )
                                    .fontSize(16)
                                    .ink(self.look.palette.figure)
                                ),
                                (
                                    box()
                                    .width(286)
                                    .height(1)
                                    .fill(self.look.palette.rule)
                                ),
                                (
                                    doc.paragraph(
                                        "The reply echoes sequence, counts accepted samples and returns mean pressure."
                                    )
                                    .fontSize(13)
                                    .ink(self.look.palette.ash)
                                ),
                            )
                        ),
                    )
                ),
                (
                    row()
                    .gap(12)
                    .alignItems("center")
                    .children(
                        (box().width(4).height(34).fill(accent).borderRadius(2)),
                        (
                            column()
                            .gap(7)
                            .children(
                                doc.caption(detail)
                                .fontSize(13)
                                .ink(self.look.palette.ink),
                            )
                        ),
                    )
                ),
            )
        )

    def plot(self, pen: Pen) -> None:
        pen.background(self.look.palette.cellGround)
        pen.stroke(self.look.palette.rule)
        pen.strokeWeight(1)
        for value in (0, 0.25, 0.5, 0.75, 1):
            y = 22 + (1 - value) * (TRACE_HEIGHT - 58)
            pen.line(18, y, TRACE_WIDTH - 18, y)
        for index in range(7):
            x = 18 + index * (TRACE_WIDTH - 36) / 6
            pen.line(x, 22, x, TRACE_HEIGHT - 36)
        pen.noFill()
        for path, accent in ((self.pressure_path, MINT), (self.flow_path, GOLD)):
            pen.stroke(accent)
            pen.strokeWeight(2.5)
            pen.shape(path)
        pen.noStroke()
        pen.fill(self.look.palette.ash)
        pen.textFont(self.look.font(self.look.type.control))
        pen.textSize(9)
        pen.textAlign(LEFT, CENTER)
        pen.text("OLDER", 18, TRACE_HEIGHT - 9)
        pen.textAlign(RIGHT, CENTER)
        pen.text("LATEST", TRACE_WIDTH - 18, TRACE_HEIGHT - 9)
