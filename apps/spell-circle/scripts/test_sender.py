#!/usr/bin/env python3
"""
UDP test sender for NetworkManager.

Sends JSON-encoded packets to the NetworkManager UDP port.
No external dependencies — uses only Python stdlib.
"""

import argparse
import json
import random
import socket
import time

HOST = "127.0.0.1"
PORT = 27015

SOURCES = ["sensor-alpha", "sensor-beta", "detector-1", "detector-2", "relay-north"]
EVENT_TYPES = ["ALERT", "INFO", "WARNING", "DATA", "STATUS", "HEARTBEAT"]


def make_packet(seq: int) -> bytes:
    payload = {
        "seq": seq,
        "type": random.choice(EVENT_TYPES),
        "src": random.choice(SOURCES),
        "value": round(random.uniform(0.0, 100.0), 3),
        "ts": time.time(),
    }
    return json.dumps(payload).encode()


def main():
    parser = argparse.ArgumentParser(description="NetworkManager UDP test sender")
    parser.add_argument("--host", default=HOST, help="Target host (default: 127.0.0.1)")
    parser.add_argument(
        "--port", default=PORT, type=int, help="UDP port (default: 27015)"
    )
    parser.add_argument(
        "--count", default=20, type=int, help="Number of packets to send (default: 20)"
    )
    parser.add_argument(
        "--interval",
        default=0.15,
        type=float,
        help="Seconds between packets (default: 0.15)",
    )
    parser.add_argument(
        "--burst",
        default=False,
        action="store_true",
        help="Send all packets immediately without delay",
    )
    args = parser.parse_args()

    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        print(f"Sending {args.count} packets to {args.host}:{args.port}")
        for i in range(args.count):
            pkt = make_packet(i)
            sock.sendto(pkt, (args.host, args.port))
            print(f"  [{i:03d}] {pkt.decode()}")
            if not args.burst:
                time.sleep(args.interval)

    print("Done.")


if __name__ == "__main__":
    main()
