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
            before = hub.fetch("out://nested/value.bin")
            self.assertEqual(before, b"first\x00value")
            self.assertTrue(hub.write("out://nested/value.bin", b"second"))
            self.assertEqual(before, b"first\x00value")
            self.assertEqual(hub.fetch("out://nested/value.bin"), b"second")
            self.assertEqual(hub.text("out://nested/value.bin"), "second")
            self.assertEqual(hub.probe(io.ResourceInfo, "out://nested/value.bin").byteSize, 6)
            self.assertEqual(
                hub.resolve("out://nested/value.bin"),
                Path(directory) / "nested/value.bin",
            )
            self.assertIsNone(hub.fetch("out://missing"))
            self.assertFalse(hub.write("https://example.invalid/output", b"x"))

    def test_arrivals_copy_buffers_and_survive_feed_destruction(self):
        feed = io.Feed("fixture://owned", io.FeedPolicy(capacity=2))
        inlet = io.testing.inletOf(feed)
        original = bytearray(b"one")
        inlet.deliver(memoryview(original), sender="fixture://sender")
        original[:] = b"two"
        inlet.deliver(b"second")
        inlet.deliver(b"third")
        self.assertEqual(feed.generation(), 3)
        self.assertEqual(feed.dropped(), 1)
        self.assertEqual(feed.receive().bytes, b"second")
        arrival = feed.receive()
        self.assertEqual(arrival.bytes, b"third")
        self.assertIsNone(feed.receive())
        self.assertEqual(feed.latest().bytes, b"third")
        self.assertEqual(feed.latest().generation, 3)
        del feed
        gc.collect()
        self.assertTrue(inlet.expired())
        self.assertEqual(arrival.bytes, b"third")
        owned = io.Arrival(bytes=original, from_="peer", at=0.5)
        original[:] = b"xxx"
        self.assertEqual(owned.bytes, b"two")
        self.assertEqual(owned.from_, "peer")
        self.assertEqual(owned.at, 0.5)

    def test_zero_capacity_keeps_only_the_newest_snapshot(self):
        feed = io.Feed("fixture://latest", io.FeedPolicy(capacity=0))
        io.testing.inletOf(feed).deliver(b"data")
        self.assertEqual(feed.latest().bytes, b"data")
        self.assertIsNone(feed.receive())
        self.assertEqual(feed.dropped(), 1)
        with self.assertRaises((TypeError, OverflowError)):
            io.FeedPolicy(capacity=-1)

    def test_buffers_must_be_contiguous_and_byte_data_is_not_text(self):
        feed = io.Feed("fixture://buffers")
        inlet = io.testing.inletOf(feed)
        with self.assertRaises((BufferError, ValueError)):
            inlet.deliver(memoryview(b"abcdef")[::2])
        with self.assertRaises(TypeError):
            inlet.deliver("encode text explicitly")
        self.assertEqual(feed.generation(), 0)

    def test_replay_preserves_recorded_spacing_and_names_no_sender(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "spacing.feed"
            writer = io.RecordingWriter(path)
            self.assertTrue(writer.append(io.Arrival(at=0, bytes=b"first", from_="ignored")))
            self.assertTrue(writer.append(io.Arrival(at=0.25, bytes=b"second")))
            del writer
            gc.collect()
            hub = io.Hub()
            feed = hub.replay("fixture://replay", path)
            hub.dispatch(10)
            arrival = feed.receive()
            self.assertEqual(arrival.bytes, b"first")
            self.assertEqual(arrival.from_, "")
            origin = arrival.at
            self.assertIsNone(feed.receive())
            self.assertFalse(feed.closed())
            hub.dispatch(10.25)
            next_arrival = feed.receive()
            self.assertEqual(next_arrival.bytes, b"second")
            self.assertAlmostEqual(next_arrival.at - origin, 0.25)
            self.assertTrue(feed.closed())
            self.assertFalse(feed.send(b"no transport"))

    def test_invalid_times_are_refused_before_they_reach_a_feed(self):
        feed = io.Feed("fixture://invalid")
        inlet = io.testing.inletOf(feed)
        hub = io.Hub()
        with tempfile.TemporaryDirectory() as directory:
            writer = io.RecordingWriter(Path(directory) / "invalid.feed")
            for invalid in (-1, math.inf, math.nan):
                with self.subTest(invalid=invalid):
                    with self.assertRaises(ValueError):
                        writer.append(io.Arrival(at=invalid))
                    with self.assertRaises(ValueError):
                        inlet.deliver(b"late", arrivedAt=invalid)
                    with self.assertRaises(ValueError):
                        hub.dispatch(invalid)
        self.assertFalse(feed.opened())
        self.assertEqual(feed.generation(), 0)

    def test_recording_file_roundtrip_through_a_replaying_hub(self):
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
            replaying = hub.replay("recording://scene", path)
            feed = hub.feed("recording://scene")
            self.assertEqual(feed, replaying)
            self.assertEqual(feed.error(), "")
            hub.dispatch(100)
            self.assertEqual(feed.receive().bytes, b"zero")
            hub.dispatch(100.5)
            self.assertEqual(feed.receive().bytes, b"half")
            self.assertTrue(feed.closed())

    def test_a_recording_lasts_as_long_as_its_handle(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "live.feed"
            feed = io.Feed("fixture://recorded")
            inlet = io.testing.inletOf(feed)
            with feed.record(path) as recording:
                self.assertFalse(recording.stopped())
                inlet.deliver(b"one", arrivedAt=0.0)
            self.assertTrue(recording.stopped())
            inlet.deliver(b"two", arrivedAt=1.0)
            self.assertEqual([a.bytes for a in io.readRecording(path)], [b"one"])

    def test_udp_request_reply_uses_native_sender_and_owned_payloads(self):
        hub = io.Hub()
        io.registerTransports(hub, ["udp"])
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
        io.registerTransports(hub, ["udp"])
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
