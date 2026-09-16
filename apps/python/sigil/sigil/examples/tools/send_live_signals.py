"""Send JSON through native SigilIO and print the observatory's acknowledgments.

Open python_live_signals.py in Sketchbook, then use an installed sigil package:
  python -m sigil.examples.tools.send_live_signals --export reply.json

Only --export writes a file: the last reply's exact bytes, through the native hub.
"""

import argparse
import math
import time
from json import dumps
from pathlib import Path

from sigil.io import Feed, Hub, registerUdp


class Options(argparse.Namespace):
    host: str = "127.0.0.1"
    port: int = 27021
    duration: float = 10
    rate: float = 20
    export: Path | None = None


def receive(feed: Feed, latest: bytes | None) -> bytes | None:
    for _ in range(256):
        arrival = feed.receive()
        if arrival is None:
            break
        latest = arrival.bytes
        print(
            f"reply {arrival.from_}: {latest.decode('utf-8', errors='replace')}",
            flush=True,
        )
    return latest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--host", default="127.0.0.1", help="Destination host (default: loopback)"
    )
    parser.add_argument("--port", type=int, default=27021)
    parser.add_argument("--duration", type=float, default=10, help="Seconds to send")
    parser.add_argument("--rate", type=float, default=20, help="Messages per second")
    parser.add_argument(
        "--export", type=Path, help="Write the last acknowledgment's exact bytes"
    )
    options = parser.parse_args(namespace=Options())
    if not 1 <= options.port <= 65535:
        parser.error("--port must be between 1 and 65535")
    if not math.isfinite(options.duration) or options.duration <= 0:
        parser.error("--duration must be finite and positive")
    if not math.isfinite(options.rate) or not 0 < options.rate <= 120:
        parser.error("--rate must be between 0 and 120")

    hub = Hub()
    registerUdp(hub)
    feed = hub.feed(f"udp://{options.host}:{options.port}")
    latest: bytes | None = None
    try:
        if feed.error():
            raise RuntimeError(feed.error())
        print(
            f"Sending to {feed.uri()}; replies arrive at {feed.address()}", flush=True
        )
        started = time.monotonic()
        sequence = 0
        while time.monotonic() - started < options.duration:
            elapsed = time.monotonic() - started
            message = dumps(
                {
                    "sequence": sequence,
                    "pressure": round(0.55 + 0.22 * math.sin(elapsed * 2.1), 4),
                    "flow": round(0.44 + 0.24 * math.sin(elapsed * 1.4 + 0.8), 4),
                }
            ).encode()
            if not feed.send(message):
                raise RuntimeError(
                    feed.error() or "The native feed refused the message."
                )
            latest = receive(feed, latest)
            sequence += 1
            time.sleep(max(0, started + sequence / options.rate - time.monotonic()))
        deadline = time.monotonic() + 0.25
        while time.monotonic() < deadline:
            latest = receive(feed, latest)
            time.sleep(0.01)
    except KeyboardInterrupt:
        print("\nStopped.")
    except RuntimeError as error:
        print(f"Signal sender: {error}")
        return 1
    finally:
        feed.close()

    if latest is None:
        print(
            "No acknowledgment received. Open the live sketch and check its listener status."
        )
        return 1
    if options.export is not None:
        output = options.export.expanduser().absolute()
        if not hub.write(str(output), latest):
            print(f"Could not write acknowledgment to {output}")
            return 1
        print(f"Wrote {len(latest)} bytes to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
