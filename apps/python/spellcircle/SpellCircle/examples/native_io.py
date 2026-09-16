"""Send a typed scene over native UDP, then save its exact bytes through a mount.

Run ``python -m SpellCircle.examples.native_io --output scene.bin`` in an
environment with the optional, interpreter-compatible ``sigil-sketch`` package.
The ephemeral listener uses the native wildcard binding; its peer connects over
loopback. No graphical application is created.
"""

from __future__ import annotations

import argparse
import time
from contextlib import ExitStack
from pathlib import Path

from SpellCircle import (
    BoxDefinition,
    CircleDefinition,
    EdgeDefinition,
    PointReference,
    SceneDefinition,
    decode_scene,
    encode_scene,
)


def example_scene() -> SceneDefinition:
    """A small scene whose labels and references survive the wire roundtrip."""
    source = CircleDefinition("Source", 180, 180, 96, active=1)
    destination = CircleDefinition("Destination", 460, 180, 72)
    first = PointReference(source, 0.25, "out")
    second = PointReference(destination, 0.75, "in")
    return SceneDefinition(
        circles=(source, destination),
        edges=(EdgeDefinition(first, second),),
        boxes=(BoxDefinition("Native IO", first, active=0.5),),
        width=640,
        height=360,
    )


def roundtrip(scene: SceneDefinition, output: Path) -> SceneDefinition:
    """Return the scene received over UDP and reloaded from the mounted file.

    Feeds close before this function returns, including when sending, decoding
    or writing fails. Wire bytes are compared without re-encoding the received
    scene, so the file is an exact copy of the datagram.
    """
    from sigil import io

    payload = encode_scene(scene)
    output = Path(output).expanduser().absolute()
    hub = io.Hub()
    io.registerUdp(hub)
    with ExitStack() as feeds:
        listener = hub.feed("udp://:0")
        feeds.callback(listener.close)
        if not listener.opened() or listener.error():
            raise RuntimeError(f"Could not listen: {listener.error()}")
        port = int(listener.address().rsplit(":", 1)[1])
        peer = hub.feed(f"udp://127.0.0.1:{port}")
        feeds.callback(peer.close)
        if not peer.opened() or peer.error():
            raise RuntimeError(f"Could not connect: {peer.error()}")
        if not peer.send(payload):
            raise RuntimeError(f"Could not send scene: {peer.error()}")

        deadline = time.monotonic() + 5
        while (arrival := listener.receive()) is None:
            if listener.error():
                raise RuntimeError(f"Could not receive: {listener.error()}")
            if time.monotonic() >= deadline:
                raise TimeoutError("No scene arrived on the native UDP feed")
            time.sleep(0.001)
        received = arrival.bytes
        if received != payload:
            raise RuntimeError("The received datagram differs from the sent scene")
        decoded = decode_scene(received)

        hub.mount("scene://", output.parent)
        uri = f"scene://{output.name}"
        if not hub.write(uri, received):
            raise OSError(f"Could not write {output}")
        stored = hub.blob(uri)
        if stored != received:
            raise OSError(f"Mounted file bytes differ from the datagram: {output}")
        if decode_scene(stored) != decoded:
            raise RuntimeError("The stored scene differs from the received scene")
        return decoded


def main(argv: list[str] | None = None) -> int:
    """Run the exchange and print the counts and exact wire-file location."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=Path("scene.bin"))
    arguments = parser.parse_args(argv)
    output = arguments.output.expanduser().absolute()
    try:
        scene = roundtrip(example_scene(), output)
    except ModuleNotFoundError as error:
        if error.name not in {"sigil", "_sigil"}:
            raise
        parser.exit(
            1,
            "This example requires sigil-sketch installed for the current "
            "interpreter.\n",
        )
    except (OSError, RuntimeError, ValueError) as error:
        parser.exit(1, f"{error}\n")
    print(
        f"Received circles={len(scene.circles)}, edges={len(scene.edges)}, "
        f"boxes={len(scene.boxes)}; saved {output.stat().st_size} bytes to {output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
