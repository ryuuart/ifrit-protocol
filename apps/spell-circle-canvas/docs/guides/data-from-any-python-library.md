# Data from any Python library: websockets and httpx on the sketch's own loop

A live ticker page fed by two ordinary Python libraries, the way a web
page is fed by `fetch` and a `WebSocket`: `websockets` subscribes to
Binance's public trade stream for BTC/USDT, `httpx` polls Open-Meteo for
the weather in Chicago, and the page shows a strip of trades running
past, a sparkline of the price, the tape of the newest trades, a weather
card and each source's own status. Neither library knows it is feeding
a sketch. Each runs as an asyncio task on `self.loop`, an event loop the
sketch package turns once a frame on the sketch thread, so what a task
receives is a Python dict it writes straight onto the sketch, and the
next description reads it. There is no thread to hand data across, no
queue, and nothing encoded or decoded inside the process.

The sketch is the directory `apps/grimoire/sketches/python_market_tape/`;
open it with `Grimoire python_market_tape.py` and edit it live. Every
code block below is copied from `python_market_tape.py` or `record.py`
in that directory.

## Who does what

| Piece | What it owns here |
| --- | --- |
| `self.loop` | An asyncio event loop of the sketch's own, turned once a frame on the sketch thread and closed with the session |
| `websockets` | The push stream: connecting, reconnecting with a backoff, and one text message a trade |
| `httpx` | The polled endpoint: one request a minute and its JSON body |
| The sketch, `python_market_tape.py` | Two tasks, one method per kind of arrival, and the page those methods' writes are drawn into |
| `record.py` | The same two libraries outside the sketch: the sources printed live, or recorded for a capture |
| [SigilIO](doxygen:SigilIO) and [SigilData](doxygen:SigilData) | Only the recordings: the session's hub loads each one as JSON for a capture to replay |

## Step 1 — the loop every sketch has

Every scene2d sketch has `self.loop`, made the first time it is asked
for. Once a frame, before `update` and `describe`, the sketch package
gives it one turn: the loop polls its sockets without waiting and runs
every callback that is ready, which is where a task resumes. A frame
never waits on the network; a task waiting on a socket resumes on the
first turn after its bytes arrive. Declaring the attribute in the class
body tells a type checker what `@sketch` supplies:

```python
@sketch(size=(WIDTH, HEIGHT), captureAt=12.0, theme=kit.featureTheme(kit.Density.Compact))
class MarketTape:
    loop: asyncio.AbstractEventLoop  # supplied by @sketch, turned once a frame
```

`setup` starts the two tasks with `create_task`, exactly as an asyncio
program would, and keeps them, because a sketch's `setup` runs again
when text evaluated in the running sketch asks for a new declaration:

```python
        for task in getattr(self, "tasks", []):
            task.cancel()
        self.tasks = []
```

```python
        self.tasks = [
            self.loop.create_task(self.listen()),
            self.loop.create_task(self.poll()),
        ]
```

When the session ends, whether the window moves to another sketch, the
file is saved and reloaded, or a still has been written, every task
still running is cancelled and the loop is closed. A task's `finally`
and `async with` blocks get a quarter of a second to run as they would
in any asyncio program, which is time for a client to close its
connections.

## Step 2 — a push stream with websockets

A task is an ordinary `async def` method. Iterating `websockets.connect`
yields a connection and, when the stream drops, waits with a backoff
and yields the next one; iterating a connection yields each message as
text. The task decodes the server's JSON and hands the dict to a method
of the sketch:

```python
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
```

`STREAM` is Binance's market-data address,
`wss://data-stream.binance.vision/ws/btcusdt@trade`, public and keyless.
Its exchange address, `wss://stream.binance.com:9443`, refuses
connections from some regions with HTTP 451; the market-data one
carries the same public streams.

## Step 3 — a polled endpoint with httpx

The weather is a request on a timer. `asyncio.sleep` is the timer: the
loop wakes the task on the first turn after the minute is up, and the
frames between cost the task nothing.

```python
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
```

A failed request is caught where it happens and becomes the source's
state on the page. An exception a task does not catch ends that task
alone; the sketch and the other task carry on.

## Step 4 — writing the model directly

Because the tasks run on the sketch thread between frames, the model is
plain attributes and the methods that write them are plain methods. One
method per kind of arrival:

```python
    def trade(self, message: dict) -> None:
        self.trades.append(message)
        self.stream.count += 1

    def reading(self, message: dict) -> None:
        self.weather = message["current_weather"]
        self.forecast.count += 1
        self.forecast.state = "ok"
```

`self.trades` is a `deque` bounded at the trades the sparkline spans,
so the newest trade pushes the oldest out. `describe` reads the
attributes as they stand at that frame, the way a component reads its
state:

```python
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
```

The sparkline is the kit's plot with a trace over the measured prices,
and the weather card and the status panels are the kit's panels and
meters; nothing on the page knows where its numbers came from.

## Step 5 — a recording for captures

A still and a plate must be the same picture every run, so a capture
never awaits the network. `record.py` runs the same two libraries
outside the sketch and keeps every arrival with the second it landed at:

```python
def collect(arrivals, start):
    def arrived(message):
        arrivals.append({"at": round(time.monotonic() - start, 3), "message": message})

    return arrived
```

In a deterministic session, `setup` loads the two recordings instead of
starting the tasks:

```python
        if ctx.deterministic:
            self.recordings = [
                Recording(recorded(ctx, "data/tape.json"), self.trade),
                Recording(recorded(ctx, "data/weather.json"), self.reading),
            ]
            self.stream.state = self.forecast.state = "replaying"
            return
```

Each is one JSON document beside the sketch, loaded through the
session's hub, which reads it the same way in a window, a still and a
test:

```python
def recorded(ctx: Scene2DContext, path: str) -> dict:
    """A recording beside the sketch, loaded through the session's hub."""
    document = ctx.assets.hub().load(data.Json, ctx.local(path))
    recording = document.toPython() if document is not None else None
    if not isinstance(recording, dict):
        raise FileNotFoundError(f"{path} holds no recording; record.py --record writes it")
    return recording
```

and `update` plays each one up to the scene time, handing every arrival
to the same method a live task hands it to:

```python
    def update(self, elapsed: float) -> None:
        for recording in self.recordings:
            recording.play(elapsed)
```

```python
    def play(self, seconds: float) -> None:
        while (
            self.played < len(self.arrivals)
            and self.arrivals[self.played]["at"] <= seconds
        ):
            self.arrived(self.arrivals[self.played]["message"])
            self.played += 1
```

The still at twelve seconds is therefore every trade that had landed
twelve seconds into the recording, whichever machine takes it.

## Step 6 — run it

The two libraries are dependencies of the catalogue's own Python
project, `apps/grimoire/sketches/pyproject.toml`, pinned in its
`uv.lock`; Grimoire prepares that environment before it opens a Python
sketch from the catalogue. In a project of your own, `uv add httpx
websockets` does the same. From the sketch's directory:

```sh
Grimoire python_market_tape.py                                    # live, in a window
Grimoire python_market_tape.py --frame market_tape.png            # the recording, replayed
Grimoire python_market_tape.py --frame live.png --no-deterministic --at 120
uv run --project .. record.py                                     # both sources, printed
uv run --project .. record.py --record                            # the recordings again
```

The third still is taken under the wall's clock with both libraries
live: the sketch steps to its moment as fast as it can draw, and the
page shows what had really arrived by then.

## What to change next

- **MQTT.** `aiomqtt` is an asyncio client over `paho-mqtt`: open
  `aiomqtt.Client(host)` with `async with` in a task, `subscribe` to a
  topic, and iterate `client.messages`, handing each payload to a method
  the way `listen` hands each trade to `trade`.
- **A server pushing into the same loop.** An ASGI application such as
  FastAPI, served by `uvicorn.Server(...).serve()` started as a task on
  `self.loop`, handles each request on the sketch thread, so a `POST`
  handler can write onto the sketch as directly as a task does.
- **A serial device.** `pyserial-asyncio`'s `open_serial_connection`
  gives a reader whose `readline` a task awaits in a loop, one line a
  reading.
- **A capture of your own source.** Copy `record.py`'s `collect` around
  any callback and write the arrivals with `write`; the sketch's
  `Recording` replays any file in that shape.

## Fine print

- **Threads.** The loop and every task on it run on the sketch thread.
  A library that insists on a thread of its own, such as `paho-mqtt`
  with `loop_start`, hands each result to the sketch with
  `self.loop.call_soon_threadsafe(self.reading, message)`: the object
  crosses as it is, and the method runs on the sketch thread at the
  next turn. A blocking call inside a task stops the frame until it
  returns; give it to `asyncio.to_thread`, whose result comes back to
  the task on the loop.
- **One turn a frame.** A task resumes at most once a turn for each
  thing it awaits, so a chain of awaits, such as a connection's TLS
  handshake, takes a few frames. The turn is a timer on the session's
  engine, and the operating system keeps what arrives between turns.
- **The end of a session** waits a quarter of a second for cancelled
  tasks to finish what they do on the way out, then closes the loop over
  whatever is still pending.
- **A scene3d sketch** has no `self.loop`: its frame is a function of
  the scene time alone.

## Where to go next

- **[Write a sketch in Python](guide:a-sketch-in-python)** — install,
  render a still, open it live.
- **[A console driven from outside, in Python](guide:console-driven-from-outside-python)**
  — the other way in: JSON messages arriving at SigilIO's UDP and
  WebSocket doors from another program.
