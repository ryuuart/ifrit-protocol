"""Market tape: a live page fed by two ordinary Python libraries, with
nothing between them and the page.

`websockets` subscribes to Binance's public trade stream for BTC/USDT and
`httpx` polls Open-Meteo for the weather in Chicago. Both run as asyncio
tasks on `self.loop`, the event loop the sketch package turns once a frame
on the sketch thread, so each arrival is a Python dict a task writes
straight onto the sketch, and the next description reads it: no thread,
no queue, and nothing decoded but the JSON the servers send.

A capture never reaches the network. It replays data/tape.jsonl and
data/weather.jsonl, which record.py wrote from the same two libraries, one
arrival a line, at the seconds each arrival landed, so a still is the same
picture every run.

    uv run --project .. record.py            # both sources, printed live
    uv run --project .. record.py --record   # the recordings under data/

EDIT THESE FIRST: STREAM, FORECAST and PLACE, POLL_SECONDS, ROWS, HISTORY.

TAGS: Python, Data/Sources, Data/Charts, Runtime/Live Input
"""

import asyncio
import json
from collections import deque
from datetime import UTC, datetime

from sigil import data
from sigil.compose import Element, Text, column, row, text
from sigil.sketch import Scene2DContext, kit, sketch

NAME = "Market tape"
CATEGORY = "Data"

# Binance's market-data stream: public, keyless, one JSON object a trade.
STREAM = "wss://data-stream.binance.vision/ws/btcusdt@trade"
# Open-Meteo's forecast: public, keyless, one JSON object a request.
FORECAST = "https://api.open-meteo.com/v1/forecast"
PLACE = {"latitude": 41.88, "longitude": -87.63, "current_weather": "true"}
POLL_SECONDS = 60.0

ROWS = 16  # trades listed on the tape
HISTORY = 240  # trades the sparkline spans
RATE_WINDOW = 5.0  # seconds of trades the rate meter counts

WIDTH, HEIGHT = 1280, 780
MONOSPACE = "Menlo, monospace"
INK, QUIET, RULE = "#e9edf2", "#8c97a6", "#2a313b"
BUY, SELL, ACCENT = "#5fbf8f", "#e0705a", "#e8b45a"
SIDES = {"BUY": BUY, "SELL": SELL}


class Source:
    """One source's account of itself, which the status readout shows."""

    def __init__(self, library: str, where: str) -> None:
        self.library = library
        self.where = where
        self.state = "waiting"
        self.count = 0


def recorded(ctx: Scene2DContext, path: str) -> list[dict]:
    """A recording beside the sketch, one arrival a line, loaded through the
    session's hub, which reads a `.jsonl` file as the list of its lines."""
    document = ctx.assets.hub().load(data.Json, ctx.local(path))
    arrivals = document.toPython() if document is not None else None
    if not isinstance(arrivals, list):
        raise FileNotFoundError(f"{path} holds no recording; record.py --record writes it")
    return arrivals


class Recording:
    """A recording replayed at the seconds its arrivals landed: each one is
    handed to the same method a live task hands it to."""

    def __init__(self, arrivals: list[dict], arrived) -> None:
        self.arrivals = arrivals
        self.arrived = arrived
        self.played = 0

    def play(self, seconds: float) -> None:
        while (
            self.played < len(self.arrivals)
            and self.arrivals[self.played]["at"] <= seconds
        ):
            self.arrived(self.arrivals[self.played]["message"])
            self.played += 1


@sketch(size=(WIDTH, HEIGHT), captureAt=12.0, styleSheet=kit.featureStyleSheet(kit.Density.Compact))
class MarketTape:
    loop: asyncio.AbstractEventLoop  # supplied by @sketch, turned once a frame

    def setup(self, ctx: Scene2DContext) -> None:
        self.trades: deque[dict] = deque(maxlen=HISTORY)
        self.weather: dict | None = None
        self.stream = Source("websockets", STREAM)
        self.forecast = Source("httpx", FORECAST)
        self.recordings: list[Recording] = []
        # setup runs again when text evaluated in the running sketch asks for
        # a new declaration; the tasks it started before stop first.
        for task in getattr(self, "tasks", []):
            task.cancel()
        self.tasks = []
        if ctx.deterministic:
            self.recordings = [
                Recording(recorded(ctx, "data/tape.jsonl"), self.trade),
                Recording(recorded(ctx, "data/weather.jsonl"), self.reading),
            ]
            self.stream.state = self.forecast.state = "replaying"
            return
        self.tasks = [
            self.loop.create_task(self.listen()),
            self.loop.create_task(self.poll()),
        ]

    # ── the two libraries ────────────────────────────────────────────────

    async def listen(self) -> None:
        """Every trade the stream pushes, written onto the tape as it lands.
        Iterating `connect` reconnects with a backoff whenever the stream
        drops."""
        # Imported where it is used: a capture never runs a task, so a still
        # needs neither library installed.
        import websockets

        self.stream.state = "connecting"
        async for connection in websockets.connect(STREAM):
            self.stream.state = "open"
            try:
                async for message in connection:
                    self.trade(json.loads(message))
            except websockets.ConnectionClosed:
                self.stream.state = "reconnecting"

    async def poll(self) -> None:
        """The current weather, asked for every POLL_SECONDS."""
        import httpx

        async with httpx.AsyncClient(timeout=10) as client:
            while True:
                self.forecast.state = "asking"
                try:
                    response = await client.get(FORECAST, params=PLACE)
                    response.raise_for_status()
                    self.reading(response.json())
                except httpx.HTTPError as error:
                    self.forecast.state = f"failed: {type(error).__name__}"
                await asyncio.sleep(POLL_SECONDS)

    # ── the model: one method per kind of arrival ───────────────────────

    def trade(self, message: dict) -> None:
        self.trades.append(message)
        self.stream.count += 1

    def reading(self, message: dict) -> None:
        self.weather = message["current_weather"]
        self.forecast.count += 1
        self.forecast.state = "ok"

    # ── the page ─────────────────────────────────────────────────────────

    def update(self, elapsed: float) -> None:
        for recording in self.recordings:
            recording.play(elapsed)

    def describe(self, seconds: float) -> Element:
        trades = list(self.trades)
        return kit.page(
            column().gap(18).children(
                strip(trades, seconds),
                row().gap(24).children(
                    column().gap(18).width(780).children(chart(trades), tape(trades)),
                    column().gap(18).flexGrow(1).children(
                        weather_card(self.weather),
                        status(self.stream, "PUSHED", rate(trades)),
                        status(self.forecast, "POLLED", None),
                    ),
                ),
            ),
            title="Market tape.",
            subtitle="BTC/USDT through websockets, Chicago's weather through httpx, "
            "one event loop on the sketch thread",
            footer="self.loop · websockets.connect · httpx.AsyncClient · "
            "a capture replays data/tape.jsonl and data/weather.jsonl",
        )


# ── reading a trade ─────────────────────────────────────────────────────


def price(trade: dict) -> float:
    return float(trade["p"])


def side(trade: dict) -> str:
    """Who crossed the spread: a buyer who made the market was hit by a
    seller, and the other way round."""
    return "SELL" if trade["m"] else "BUY"


def clock(milliseconds: float) -> str:
    """A trade's own time, to the millisecond, in UTC."""
    moment = datetime.fromtimestamp(milliseconds / 1000, tz=UTC)
    return f"{moment:%H:%M:%S.%f}"[:-3]


def rate(trades: list[dict]) -> float:
    """Trades a second over the newest RATE_WINDOW seconds of the tape,
    read from the trades' own clocks."""
    if not trades:
        return 0.0
    newest = trades[-1]["T"]
    recent = [trade for trade in trades if newest - trade["T"] <= RATE_WINDOW * 1000]
    return len(recent) / RATE_WINDOW


# ── the pieces of the page ──────────────────────────────────────────────


def words(content: str, size: float = 13, color: str = QUIET) -> Text:
    return text(content, size=size, color=color).fontFamily(MONOSPACE)


def strip(trades: list[dict], seconds: float) -> Element:
    """The newest trades running past along the top."""
    pieces = [
        words(f"{side(trade)} {price(trade):,.2f} × {float(trade['q']):.5f}", 13, SIDES[side(trade)])
        for trade in trades[-12:]
    ] or [words("waiting for the first trade")]
    return kit.ticker(
        row().gap(28).children(*pieces),
        contentWidth=2600,
        phase=-seconds * 0.04,
        width=WIDTH - 96,
        height=22,
    )


def chart(trades: list[dict]) -> Element:
    """The last HISTORY prices as a sparkline, with the first as a rule and
    the prices it ranges over numbered up its side."""
    prices = [price(trade) for trade in trades]
    if len(prices) < 2:
        return column().height(220).children(words("the sparkline draws from the second trade"))
    low, high = min(prices), max(prices)
    margin = max((high - low) * 0.15, 0.5)
    frame = kit.Plot(pad=10)
    frame.x.domain = data.Interval(0, 1)
    frame.y.domain = data.Interval(low - margin, high + margin)
    change = prices[-1] - prices[0]
    headline = row().gap(16).alignItems("end").children(
        words(f"{prices[-1]:,.2f}", 34, INK),
        words(f"{change:+,.2f} over {len(prices)} trades", 14, BUY if change >= 0 else SELL),
    )
    return column().gap(8).children(
        headline,
        kit.plot(
            "price",
            frame,
            [
                kit.rules(y=[prices[0]], pen=1),
                kit.trace(prices, pen=2),
                kit.axis(of=kit.Axis.Y, count=4, line=False, inside=True),
            ],
        ).width(780).height(170),
    )


def tape(trades: list[dict]) -> Element:
    """The newest ROWS trades, newest first."""
    lines = [
        row().gap(28).children(
            words(clock(trade["T"]), 13, QUIET).width(120),
            words(side(trade), 13, SIDES[side(trade)]).width(48),
            words(f"{price(trade):>12,.2f}", 13, INK).width(120),
            words(f"{float(trade['q']):>10.5f} BTC", 13, QUIET),
        )
        for trade in reversed(trades[-ROWS:])
    ]
    return column().gap(4).children(
        words("TIME (UTC)     SIDE         PRICE          SIZE", 11, QUIET), *lines
    )


WEATHER_WORDS = {0: "clear", 1: "mainly clear", 2: "partly cloudy", 3: "overcast"}
COMPASS = "N NNE NE ENE E ESE SE SSE S SSW SW WSW W WNW NW NNW".split()


def sky(code: int) -> str:
    if code in WEATHER_WORDS:
        return WEATHER_WORDS[code]
    for first, last, name in ((45, 48, "fog"), (51, 67, "rain"), (71, 77, "snow"),
                              (80, 86, "showers"), (95, 99, "thunderstorm")):
        if first <= code <= last:
            return name
    return f"code {code}"


def weather_card(weather: dict | None) -> Element:
    """The newest reading httpx brought back."""
    if weather is None:
        return kit.panel(words("waiting for the first reading"), eyebrow="CHICAGO", title="Weather")
    temperature = weather["temperature"]
    wind = weather["windspeed"]
    heading = COMPASS[round(weather["winddirection"] / 22.5) % 16]
    return kit.panel(
        column().gap(12).children(
            row().gap(14).alignItems("end").children(
                words(f"{temperature:.1f}°C", 40, INK),
                words(sky(weather["weathercode"]), 16, ACCENT),
            ),
            kit.meter(fraction=(temperature + 20) / 60, label="−20 to 40 °C",
                      reading=f"{temperature:.1f}", width=360),
            kit.meter(fraction=min(wind / 60, 1), label=f"wind from {heading}",
                      reading=f"{wind:.0f} km/h", width=360, bar=ACCENT),
            words(f"observed {weather['time']} UTC · {'day' if weather['is_day'] else 'night'}"),
        ),
        eyebrow="CHICAGO · OPEN-METEO",
        title="Weather",
    )


def status(source: Source, how: str, trades_a_second: float | None) -> Element:
    """Where a source is, what state it is in and what it has brought."""
    colour = BUY if source.state in ("open", "ok", "replaying") else ACCENT
    lines = [
        row().gap(12).alignItems("center").children(
            kit.chip(label=source.state.upper(), ground=RULE, ink=colour),
            words(f"{source.count} arrival{'' if source.count == 1 else 's'}", 13, INK),
        ),
        words(source.where, 11),
    ]
    if trades_a_second is not None:
        lines.append(
            kit.meter(fraction=min(trades_a_second / 60, 1), label="trades a second",
                      reading=f"{trades_a_second:.1f}", width=360)
        )
    return kit.panel(column().gap(8).children(*lines), eyebrow=how, title=source.library)
