"""Native IO ownership, mounted resources, replay and transport contracts."""

import gc
import math
import socket
import tempfile
import time
import unittest
from pathlib import Path

from sigil import io


def receive(feed):
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        arrival = feed.receive()
        if arrival is not None:
            return arrival
        time.sleep(0.001)
    raise AssertionError(f"No arrival on {feed.uri()}: {feed.error()}")


class IO(unittest.TestCase):
    def test_mounted_write_invalidates_cached_owned_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            hub = io.Hub()
            hub.mount("out://", Path(directory))
            payload = bytearray(b"first\x00value")
            self.assertTrue(hub.write("out://nested/value.bin", payload))
            payload[:] = b"xxxxx\x00xxxxx"
            before = hub.blob("out://nested/value.bin")
            self.assertEqual(before, b"first\x00value")
            self.assertTrue(hub.write("out://nested/value.bin", b"second"))
            self.assertEqual(before, b"first\x00value")
            self.assertEqual(hub.fetch("out://nested/value.bin"), b"second")
            self.assertEqual(hub.text("out://nested/value.bin"), "second")
            self.assertEqual(hub.probe("out://nested/value.bin").byteSize, 6)
            self.assertEqual(
                hub.resolve("out://nested/value.bin"),
                Path(directory) / "nested/value.bin",
            )
            self.assertIsNone(hub.blob("out://missing"))
            self.assertFalse(hub.write("https://example.invalid/output", b"x"))

    def test_arrivals_copy_buffers_and_survive_feed_destruction(self):
        feed = io.Feed("fixture://owned", io.FeedPolicy(capacity=2))
        original = bytearray(b"one")
        feed.deliver(memoryview(original), from_="fixture://sender")
        original[:] = b"two"
        feed.deliver(b"second")
        feed.deliver(b"third")
        self.assertEqual(feed.generation(), 3)
        self.assertEqual(feed.dropped(), 1)
        self.assertEqual(feed.receive().bytes, b"second")
        arrival = feed.receive()
        self.assertEqual(arrival.bytes, b"third")
        self.assertIsNone(feed.receive())
        self.assertEqual(feed.newest().bytes, b"third")
        self.assertEqual(feed.latest(), b"third")
        del feed
        gc.collect()
        self.assertEqual(arrival.bytes, b"third")
        owned = io.Arrival(bytes=original, from_="peer", at=0.5)
        original[:] = b"xxx"
        self.assertEqual(owned.bytes, b"two")
        self.assertEqual(owned.from_, "peer")
        self.assertEqual(owned.at, 0.5)

    def test_zero_capacity_keeps_only_the_newest_snapshot(self):
        feed = io.Feed("fixture://latest", io.FeedPolicy(capacity=0))
        feed.deliver(b"data")
        self.assertEqual(feed.latest(), b"data")
        self.assertIsNone(feed.receive())
        self.assertEqual(feed.dropped(), 1)
        with self.assertRaises((TypeError, OverflowError)):
            io.FeedPolicy(capacity=-1)

    def test_buffers_must_be_contiguous_and_byte_data_is_not_text(self):
        feed = io.Feed("fixture://buffers")
        with self.assertRaises((BufferError, ValueError)):
            feed.deliver(memoryview(b"abcdef")[::2])
        with self.assertRaises(TypeError):
            feed.deliver("encode text explicitly")
        self.assertEqual(feed.generation(), 0)

    def test_replay_copies_arrivals_and_preserves_recorded_spacing(self):
        first = io.Arrival(at=0, bytes=b"first", from_="ignored")
        second = io.Arrival(at=0.25, bytes=b"second")
        feed = io.Feed("fixture://replay")
        feed.replay([first, second])
        first.bytes = b"edited"
        second.at = 0
        feed.advance(10)
        arrival = feed.receive()
        self.assertEqual(arrival.bytes, b"first")
        self.assertEqual(arrival.from_, "")
        origin = arrival.at
        self.assertIsNone(feed.receive())
        self.assertFalse(feed.closed())
        feed.advance(10.25)
        next_arrival = feed.receive()
        self.assertEqual(next_arrival.bytes, b"second")
        self.assertAlmostEqual(next_arrival.at - origin, 0.25)
        self.assertTrue(feed.closed())
        self.assertFalse(feed.send(b"no transport"))

    def test_replay_rejects_invalid_order_and_times_before_mutation(self):
        feed = io.Feed("fixture://invalid")
        for invalid in (-1, math.inf, math.nan):
            with self.subTest(invalid=invalid):
                with self.assertRaises(ValueError):
                    feed.replay([io.Arrival(at=invalid)])
                with self.assertRaises(ValueError):
                    feed.advance(invalid)
        with self.assertRaises(ValueError):
            feed.replay([io.Arrival(at=1), io.Arrival(at=0)])
        self.assertFalse(feed.opened())
        self.assertEqual(feed.generation(), 0)

    def test_recording_file_roundtrip_through_a_mounted_hub(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events.feed"
            writer = io.RecordingWriter(path)
            self.assertTrue(writer.good())
            self.assertTrue(writer.append(io.Arrival(at=0, bytes=b"zero")))
            self.assertTrue(writer.append(io.Arrival(at=0.5, bytes=b"half")))
            arrivals = io.readRecording(path)
            self.assertEqual([a.bytes for a in arrivals], [b"zero", b"half"])
            self.assertIsNone(io.readRecording(Path(directory) / "absent"))
            hub = io.Hub()
            hub.mount("recording://scene", path)
            feed = hub.feed("recording://scene")
            self.assertEqual(feed.error(), "")
            hub.dispatch(100)
            self.assertEqual(feed.receive().bytes, b"zero")
            hub.dispatch(100.5)
            self.assertEqual(feed.receive().bytes, b"half")
            self.assertTrue(feed.closed())

    def test_udp_request_reply_uses_native_sender_and_owned_payloads(self):
        hub = io.Hub()
        io.registerUdp(hub)
        listener = hub.feed("udp://:0")
        self.assertEqual(listener.error(), "")
        port = int(listener.address().rsplit(":", 1)[1])
        sender = hub.feed(f"udp://127.0.0.1:{port}")
        try:
            self.assertEqual(hub.feed("udp://:0"), listener)
            self.assertIn(listener, hub.feeds())
            payload = bytearray(b"request")
            self.assertTrue(sender.send(payload))
            payload[:] = b"changed"
            arrival = receive(listener)
            self.assertEqual(arrival.bytes, b"request")
            self.assertTrue(arrival.from_.startswith("udp://"))
            self.assertTrue(listener.sendTo(arrival.from_, b"reply"))
            self.assertEqual(receive(sender).bytes, b"reply")
            self.assertFalse(listener.send(b"no default peer"))
            listener.close()
            self.assertFalse(listener.sendTo(arrival.from_, b"closed"))
            with socket.socket(socket.AF_INET6, socket.SOCK_DGRAM) as replacement:
                replacement.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
                replacement.bind(("::", port))
        finally:
            listener.close()
            sender.close()

    def test_standalone_feed_keeps_its_hub_alive_and_missing_transport_is_explicit(
        self,
    ):
        hub = io.Hub()
        absent = hub.feed("udp://:0")
        self.assertIn("no feed transport", absent.error())
        self.assertFalse(absent.opened())
        io.registerUdp(hub)
        live = hub.feed("udp://:0")
        self.assertEqual(live, absent)
        self.assertTrue(live.opened())
        del hub
        gc.collect()
        port = int(live.address().rsplit(":", 1)[1])
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
            sender.sendto(b"still alive", ("127.0.0.1", port))
        self.assertEqual(receive(live).bytes, b"still alive")
        live.close()


if __name__ == "__main__":
    unittest.main()
