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
    raise AssertionError(f"No arrival on {feed.uri()}: {feed.state().error}")


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
        feed = io.Feed("fixture://owned", io.ListenOptions(capacity=2))
        inlet = io.testing.inletOf(feed)
        original = bytearray(b"one")
        inlet.deliver(memoryview(original), sender="fixture://sender")
        original[:] = b"two"
        inlet.deliver(b"second")
        inlet.deliver(b"third")
        self.assertEqual(feed.state().revision, 3)
        self.assertEqual(feed.state().dropped, 1)
        self.assertEqual(feed.receive().payload, b"second")
        arrival = feed.receive()
        self.assertEqual(arrival.payload, b"third")
        self.assertIsNone(feed.receive())
        self.assertEqual(feed.latest().payload, b"third")
        self.assertEqual(feed.latest().revision, 3)
        del feed
        gc.collect()
        self.assertTrue(inlet.expired())
        self.assertEqual(arrival.payload, b"third")
        owned = io.Message(payload=original, sender="peer", arrivedAt=0.5)
        original[:] = b"xxx"
        self.assertEqual(owned.payload, b"two")
        self.assertEqual(owned.sender, "peer")
        self.assertEqual(owned.arrivedAt, 0.5)

    def test_zero_capacity_keeps_only_the_newest_snapshot(self):
        feed = io.Feed("fixture://latest", io.ListenOptions(capacity=0))
        io.testing.inletOf(feed).deliver(b"data")
        self.assertEqual(feed.latest().payload, b"data")
        self.assertIsNone(feed.receive())
        self.assertEqual(feed.state().dropped, 1)
        with self.assertRaises((TypeError, OverflowError)):
            io.ListenOptions(capacity=-1)

    def test_buffers_must_be_contiguous_and_byte_data_is_not_text(self):
        feed = io.Feed("fixture://buffers")
        inlet = io.testing.inletOf(feed)
        with self.assertRaises((BufferError, ValueError)):
            inlet.deliver(memoryview(b"abcdef")[::2])
        with self.assertRaises(TypeError):
            inlet.deliver("encode text explicitly")
        self.assertEqual(feed.state().revision, 0)

    def test_replay_preserves_recorded_spacing_and_names_no_sender(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "spacing.feed"
            writer = io.RecordingWriter(path)
            self.assertTrue(writer.append(io.Message(arrivedAt=0, payload=b"first", sender="ignored")))
            self.assertTrue(writer.append(io.Message(arrivedAt=0.25, payload=b"second")))
            del writer
            gc.collect()
            hub = io.Hub()
            feed = hub.replay("fixture://replay", path)
            hub.advance(10)
            arrival = feed.receive()
            self.assertEqual(arrival.payload, b"first")
            self.assertEqual(arrival.sender, "")
            origin = arrival.arrivedAt
            self.assertIsNone(feed.receive())
            self.assertNotEqual(feed.state().readiness, io.ReadyState.Closed)
            hub.advance(10.25)
            next_arrival = feed.receive()
            self.assertEqual(next_arrival.payload, b"second")
            self.assertAlmostEqual(next_arrival.arrivedAt - origin, 0.25)
            self.assertEqual(feed.state().readiness, io.ReadyState.Closed)
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
                        writer.append(io.Message(arrivedAt=invalid))
                    with self.assertRaises(ValueError):
                        inlet.deliver(b"late", arrivedAt=invalid)
                    with self.assertRaises(ValueError):
                        hub.advance(invalid)
        self.assertFalse(feed.state().isOpen())
        self.assertEqual(feed.state().revision, 0)

    def test_recording_file_roundtrip_through_a_replaying_hub(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "events.feed"
            writer = io.RecordingWriter(path)
            self.assertTrue(writer.good())
            self.assertTrue(writer.append(io.Message(arrivedAt=0, payload=b"zero")))
            self.assertTrue(writer.append(io.Message(arrivedAt=0.5, payload=b"half")))
            arrivals = io.readRecording(path)
            self.assertEqual([a.payload for a in arrivals], [b"zero", b"half"])
            self.assertIsNone(io.readRecording(Path(directory) / "absent"))
            hub = io.Hub()
            replaying = hub.replay("recording://scene", path)
            feed = hub.listen("recording://scene")
            self.assertEqual(feed, replaying)
            self.assertEqual(feed.state().error, "")
            hub.advance(100)
            self.assertEqual(feed.receive().payload, b"zero")
            hub.advance(100.5)
            self.assertEqual(feed.receive().payload, b"half")
            self.assertEqual(feed.state().readiness, io.ReadyState.Closed)

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
            self.assertEqual([a.payload for a in io.readRecording(path)], [b"one"])

    def test_udp_request_reply_uses_native_sender_and_owned_payloads(self):
        hub = io.Hub()
        io.registerTransports(hub, ["udp"])
        listener = hub.listen("udp://:0")
        self.assertEqual(listener.state().error, "")
        port = int(listener.state().localAddress.rsplit(":", 1)[1])
        sender = hub.listen(f"udp://127.0.0.1:{port}")
        try:
            self.assertEqual(hub.listen("udp://:0"), listener)
            self.assertIn(listener, hub.feeds())
            payload = bytearray(b"request")
            self.assertTrue(sender.send(payload))
            payload[:] = b"changed"
            arrival = receive(listener)
            self.assertEqual(arrival.payload, b"request")
            self.assertTrue(arrival.sender.startswith("udp://"))
            self.assertTrue(listener.send(b"reply", to=arrival.sender))
            self.assertEqual(receive(sender).payload, b"reply")
            self.assertFalse(listener.send(b"no default peer"))
            listener.close()
            self.assertFalse(listener.send(b"closed", to=arrival.sender))
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
        absent = hub.listen("pigeon://roof")
        self.assertIn("no feed transport", absent.state().error)
        self.assertFalse(absent.state().isOpen())
        # Every linked transport is ready on the first listen, with no
        # registration.
        live = hub.listen("udp://:0")
        self.assertTrue(live.state().isOpen())
        del hub
        gc.collect()
        port = int(live.state().localAddress.rsplit(":", 1)[1])
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sender:
            sender.sendto(b"still alive", ("127.0.0.1", port))
        self.assertEqual(receive(live).payload, b"still alive")
        live.close()


if __name__ == "__main__":
    unittest.main()
