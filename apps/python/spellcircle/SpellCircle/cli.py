"""Inspect saved scenes or deliver their exact bytes to a protocol receiver."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from .codec import decode_scene
from .network import DEFAULT_HOST, DEFAULT_PORT, send_once


def _port(value: str) -> int:
    number = int(value)
    if not 1 <= number <= 65535:
        raise argparse.ArgumentTypeError("port must be between 1 and 65535")
    return number


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    inspect = commands.add_parser(
        "inspect", help="Print a saved scene's metadata as JSON"
    )
    inspect.add_argument("file", type=Path)
    send = commands.add_parser("send", help="Send a saved scene over UDP")
    send.add_argument("file", type=Path)
    send.add_argument("--host", default=DEFAULT_HOST)
    send.add_argument("--port", type=_port, default=DEFAULT_PORT)
    arguments = parser.parse_args(argv)
    try:
        payload = arguments.file.read_bytes()
        scene = decode_scene(payload)
        if arguments.command == "send":
            send_once(payload, arguments.host, arguments.port)
            print(f"Sent {len(payload)} bytes to {arguments.host}:{arguments.port}")
        else:
            print(
                json.dumps(
                    {
                        "width": scene.width,
                        "height": scene.height,
                        "circles": len(scene.circles),
                        "edges": len(scene.edges),
                        "boxes": len(scene.boxes),
                        "bytes": len(payload),
                    },
                    indent=2,
                )
            )
    except (OSError, ValueError) as error:
        print(f"spellcircle: {error}", file=sys.stderr)
        return 1
    return 0
