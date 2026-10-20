#!/usr/bin/env python3
"""What python_market_tape's two libraries deliver: shown, or recorded.

    uv run --project .. record.py
        prints every trade websockets receives and every reading httpx
        fetches, as they arrive, until interrupted: the same two sources
        the sketch reads, with nothing drawn.

    uv run --project .. record.py --record
        listens for RECORD_SECONDS and writes what arrived into
        data/tape.jsonl and data/weather.jsonl, which is what a capture
        replays.

A recording is JSON lines, one arrival a line: the dict the library
delivered and the seconds after the start at which it landed. The sketch
replays it at those seconds, so a still is the same picture every run.

Run it with the interpreter of the catalogue's project, `uv run --project
..` from this directory, which holds httpx and websockets; nothing else
is needed.
"""

import argparse
import asyncio
import json
import time
from pathlib import Path

import httpx
import websockets

# The same two sources the sketch reads.
STREAM = "wss://data-stream.binance.vision/ws/btcusdt@trade"
FORECAST = "https://api.open-meteo.com/v1/forecast"
PLACE = {"latitude": 41.88, "longitude": -87.63, "current_weather": "true"}
POLL_SECONDS = 60.0

RECORD_SECONDS = 16.0
HERE = Path(__file__).resolve().parent


async def listen(arrived):
    """Every trade the stream pushes, handed to arrived as a dict."""
    async for connection in websockets.connect(STREAM):
        try:
            async for message in connection:
                arrived(json.loads(message))
        except websockets.ConnectionClosed:
            continue


async def poll(arrived):
    """The current weather, every POLL_SECONDS, handed to arrived."""
    async with httpx.AsyncClient(timeout=10) as client:
        while True:
            response = await client.get(FORECAST, params=PLACE)
            response.raise_for_status()
            arrived(response.json())
            await asyncio.sleep(POLL_SECONDS)


def show(name):
    def arrived(message):
        print(name, json.dumps(message, separators=(",", ":")), flush=True)

    return arrived


def collect(arrivals, start):
    def arrived(message):
        arrivals.append({"at": round(time.monotonic() - start, 3), "message": message})

    return arrived


def write(name, arrivals):
    """JSON lines, one arrival a line, so a new recording diffs."""
    path = HERE / "data" / name
    path.write_text("".join(json.dumps(arrival, separators=(",", ":")) + "\n" for arrival in arrivals))
    print(f"wrote {len(arrivals)} arrivals into {path.relative_to(HERE)}")


async def record():
    start = time.monotonic()
    trades, readings = [], []
    tasks = [
        asyncio.create_task(listen(collect(trades, start))),
        asyncio.create_task(poll(collect(readings, start))),
    ]
    await asyncio.sleep(RECORD_SECONDS)
    for task in tasks:
        task.cancel()
    await asyncio.gather(*tasks, return_exceptions=True)
    write("tape.jsonl", trades)
    write("weather.jsonl", readings)


async def watch():
    await asyncio.gather(listen(show("trade")), poll(show("weather")))


def main():
    parser = argparse.ArgumentParser(
        description="What python_market_tape's two libraries deliver: shown, or recorded."
    )
    parser.add_argument(
        "--record",
        action="store_true",
        help=f"write {RECORD_SECONDS:g} seconds of both sources under data/",
    )
    arguments = parser.parse_args()
    try:
        asyncio.run(record() if arguments.record else watch())
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
