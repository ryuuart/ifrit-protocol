# Market tape

A live ticker page fed by two ordinary Python libraries with nothing
between them and the page. `websockets` subscribes to Binance's public
trade stream for BTC/USDT and `httpx` polls Open-Meteo for the current
weather in Chicago; neither needs a key. Both run as asyncio tasks on
`self.loop`, the event loop the sketch package gives every scene2d
sketch and turns once a frame on the sketch thread, so each arrival is a
Python dict a task writes straight onto the sketch and the next
description reads it: no thread, no queue, and nothing decoded but the
JSON the servers send.

The page shows a strip of the newest trades running past, the price as a
sparkline over the last 240 trades, the tape of the newest sixteen, a
weather card, and each source's own account of itself: its state, how
many arrivals it has brought and, for the stream, the trades a second.
The canvas is 1280 × 780.

[The guide](../../../spell-circle-canvas/docs/guides/data-from-any-python-library.md)
walks through every part of it.

## The sources

| Library | Source | What arrives |
| --- | --- | --- |
| `websockets` | `wss://data-stream.binance.vision/ws/btcusdt@trade` | one JSON object a trade, pushed |
| `httpx` | `https://api.open-meteo.com/v1/forecast?latitude=41.88&longitude=-87.63&current_weather=true` | one JSON object a request, every minute |

The stream is Binance's market-data address. Its exchange address,
`wss://stream.binance.com:9443`, refuses connections from some regions
with HTTP 451; the market-data one carries the same public streams.

## The files

| File | What it is |
| --- | --- |
| `python_market_tape.py` | The sketch: two tasks on `self.loop`, one method per kind of arrival, and the page. |
| `record.py` | Both sources printed as they arrive, or `--record` to write the recordings under `data/`. |
| `data/tape.json` | Sixteen seconds of trades as `websockets` delivered them, each with the second it landed at. |
| `data/weather.json` | The reading `httpx` brought back in the same sixteen seconds. |

## The libraries

The two libraries are dependencies of the catalogue's own Python
project, `../pyproject.toml`, pinned in `../uv.lock`. Grimoire prepares
that project's environment, `../.venv`, before it opens a Python sketch
from the catalogue, so nothing needs installing by hand. To add them to
a project of your own:

```sh
uv add httpx websockets
```

## Run it

From this directory:

```sh
../../../spell-circle-canvas/build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire python_market_tape.py
```

A still replays the recordings instead of opening a socket, so it is the
same picture every run:

```sh
../../../spell-circle-canvas/build/bin/Release/Grimoire.app/Contents/MacOS/Grimoire python_market_tape.py --frame market_tape.png
```

`--no-deterministic` takes the still under the wall's clock instead, with
both libraries live, which shows what has really arrived by the time the
still is taken.

To watch the two sources in a terminal, or to record them again:

```sh
uv run --project .. record.py
uv run --project .. record.py --record
```

## What it puts under load

Third-party asyncio libraries running on a sketch's own event loop on
the sketch thread; a push stream and a polled endpoint written into one
model by plain method calls; reconnection with a backoff from the
library's own iterator; the tasks cancelled when the session ends; and a
recording replayed through the same methods at the seconds its arrivals
landed, so a capture never reaches the network.
