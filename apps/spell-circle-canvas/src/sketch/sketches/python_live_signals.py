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
from sigil.data import decodeJson
from sigil.draw import CENTER, LEFT, RIGHT, Pen
from sigil.io import Arrival, Feed, FeedPolicy
from sigil.sketch import SketchContext, sketch
from sigil.skia import Path, PathBuilder

PORT = 27021
PAPER, INK, MUTED = "#edf0e7", "#263f41", "#6b7e79"
PANEL, RULE = "#143238", "#d0d9cd"
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
        column()
        .gap(10)
        .padding(21)
        .height(142)
        .grow(1)
        .basis(0)
        .fill("#f8f9f2")
        .corners(12)
        .ink(MUTED)
        .children(
            (
                row()
                .gap(8)
                .alignItems("center")
                .children(
                    (box().width(6).height(6).corners(3).fill(accent)),
                    text(label, size=11),
                )
            ),
            text(value, size=43, color=INK),
            text(detail, size=11, color=MUTED),
        )
    )


@sketch(size=(1100, 720), background=PAPER, capture_at=4.0)
class LiveSignals:
    def setup(self, ctx: SketchContext) -> None:
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
            self.feed = Feed("replay://live-signals", policy)
            self.feed.replay(
                [
                    Arrival(at=index / 30, bytes=encoded(reference(index)))
                    for index in range(241)
                ]
            )
        else:
            self.feed = ctx.assets.hub().feed(f"udp://:{PORT}", policy)
        self.rebuild_traces()
        ctx.render(self.describe(0))

    def update(self, elapsed: float, ctx: SketchContext) -> None:
        if self.replay:
            self.feed.advance(elapsed)
        changed = False
        for _ in range(64):
            arrival = self.feed.receive()
            if arrival is None:
                break
            try:
                signal = decoded(arrival.bytes)
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
            self.peer = arrival.from_ or "Synthetic recording"
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
                if self.feed.sendTo(arrival.from_, reply):
                    self.replies += 1
                else:
                    self.problem = (
                        "The arrival was accepted, but its reply could not be sent."
                    )
        if changed:
            self.rebuild_traces()
        if elapsed - self.last_description >= 0.1:
            self.last_description = elapsed
            ctx.render(self.describe(elapsed))

    def rebuild_traces(self) -> None:
        self.pressure_path = trace([sample.pressure for sample in self.samples])
        self.flow_path = trace([sample.flow for sample in self.samples])

    def state(self, elapsed: float) -> tuple[str, str, str]:
        if self.feed.error():
            return "UNAVAILABLE", "#ad624e", self.feed.error()
        if self.problem:
            return "CHECK INPUT", "#ad624e", self.problem
        if self.replay:
            return (
                "REPLAY",
                "#537d83",
                "Synthetic recording. No socket is open; no reply is sent.",
            )
        if self.feed.closed():
            return (
                "CLOSED",
                MUTED,
                "The feed is closed. Save the sketch to open a fresh session.",
            )
        if not self.feed.opened():
            return "OPENING", MUTED, "The native transport is opening the listener."
        if not self.received:
            return (
                "WAITING",
                "#ad8545",
                f"Listening on UDP {PORT}. Reference traces are shown until data arrives.",
            )
        if elapsed - self.last_arrival > 2:
            return (
                "QUIET",
                "#ad8545",
                "No recent arrivals. The last received samples remain visible.",
            )
        return (
            "LIVE",
            "#448875",
            f"Receiving from {self.peer}. Every valid arrival gets a JSON acknowledgment.",
        )

    def describe(self, elapsed: float) -> Element:
        status, accent, detail = self.state(elapsed)
        latest = self.samples[-1]
        return (
            column()
            .width(1004)
            .gap(16)
            .absolute()
            .left(48)
            .top(34)
            .children(
                (
                    row()
                    .width(1004)
                    .justify("space_between")
                    .children(
                        text("FIELD INSTRUMENTS / 03", size=11, color=MUTED),
                        text("JSON IN · JSON OUT", size=11, color=MUTED),
                    )
                ),
                (
                    row()
                    .width(1004)
                    .justify("space_between")
                    .alignItems("center")
                    .children(
                        text("Signals, received.", size=48, color=INK),
                        (
                            row()
                            .gap(9)
                            .padding(10, 15)
                            .fill("#f8f9f2")
                            .corners(18)
                            .children(
                                (box().width(7).height(7).corners(4).fill(accent)),
                                text(status, size=12, color=accent),
                            )
                        ),
                    )
                ),
                (
                    text(
                        "Two channels, one small message, and a way back to the sender.",
                        size=16,
                        color=MUTED,
                    )
                ),
                (
                    row()
                    .gap(16)
                    .width(1004)
                    .children(
                        metric(
                            "PRESSURE",
                            f"{latest.pressure:.0%}" if self.received else "—",
                            "Normalized input / 0–1",
                            "#448875",
                        ),
                        metric(
                            "FLOW",
                            f"{latest.flow:.0%}" if self.received else "—",
                            "Normalized input / 0–1",
                            "#ad8545",
                        ),
                        metric(
                            "ARRIVALS / REPLIES",
                            f"{self.received:03} / {self.replies:03}",
                            f"{self.rejected} invalid · {self.feed.dropped()} queue drops",
                            "#537d83",
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
                            column()
                            .gap(14)
                            .padding(24)
                            .fill(PANEL)
                            .corners(14)
                            .width(654)
                            .children(
                                (
                                    row()
                                    .width(606)
                                    .justify("space_between")
                                    .children(
                                        text(
                                            "RECENT ARRIVALS",
                                            size=11,
                                            color="#d3e6dd",
                                        ),
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
                                                    color="#9bbab4",
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
                                        color="#9bbab4",
                                    )
                                ),
                            )
                        ),
                        (
                            column()
                            .gap(16)
                            .padding(24)
                            .fill("#e2e8dc")
                            .corners(14)
                            .width(334)
                            .children(
                                text("A small contract.", size=24, color=INK),
                                (
                                    text(
                                        '{\n  "sequence": 1,\n  "pressure": 0.62,\n  "flow": 0.35\n}',
                                        size=16,
                                        color="#527a72",
                                    )
                                ),
                                (box().width(286).height(1).fill(RULE)),
                                (
                                    text(
                                        "The reply echoes sequence, counts accepted samples and returns mean pressure.",
                                        size=13,
                                        color=MUTED,
                                    )
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
                        (box().width(4).height(34).fill(accent).corners(2)),
                        (
                            column()
                            .gap(7)
                            .children(
                                text(detail, size=13, color=INK),
                                (
                                    text(
                                        f"udp://:{PORT}  ·  sender: sigil.examples.tools.send_live_signals  ·  --export reply.json",
                                        size=10,
                                        color=MUTED,
                                    )
                                ),
                            )
                        ),
                    )
                ),
            )
        )

    def plot(self, pen: Pen) -> None:
        pen.background(PANEL)
        pen.stroke("#29464a")
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
        pen.fill("#9bbab4")
        pen.textSize(9)
        pen.textAlign(LEFT, CENTER)
        pen.text("OLDER", 18, TRACE_HEIGHT - 9)
        pen.textAlign(RIGHT, CENTER)
        pen.text("LATEST", TRACE_WIDTH - 18, TRACE_HEIGHT - 9)
