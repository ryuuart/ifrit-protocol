"""One send is one datagram, independent of a scene renderer."""

import socket
import unittest

from SpellCircle import SceneSender, send_once


class Network(unittest.TestCase):
    def test_context_sender_preserves_datagrams_and_closes(self):
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as receiver:
            receiver.bind(("127.0.0.1", 0))
            receiver.settimeout(2)
            with SceneSender(*receiver.getsockname()) as sender:
                sender.send(memoryview(b"one\x00"))
                sender.send(bytearray(b"two"))
                self.assertEqual(receiver.recv(65536), b"one\x00")
                self.assertEqual(receiver.recv(65536), b"two")
            sender.close()
            with self.assertRaises(OSError):
                sender.send(b"closed")

    def test_ipv6_destination_uses_an_ipv6_socket(self):
        if not socket.has_ipv6:
            self.skipTest("IPv6 is unavailable")
        try:
            receiver = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
        except OSError as error:
            self.skipTest(f"IPv6 loopback is unavailable: {error}")
        with receiver:
            try:
                receiver.bind(("::1", 0))
            except OSError as error:
                self.skipTest(f"IPv6 loopback is unavailable: {error}")
            receiver.settimeout(2)
            send_once(b"ipv6", "::1", receiver.getsockname()[1])
            self.assertEqual(receiver.recv(65536), b"ipv6")

    def test_invalid_destination_port_is_rejected_before_socket_creation(self):
        for port in (0, -1, 65536, True):
            with self.subTest(port=port), self.assertRaises(ValueError):
                SceneSender(port=port)


if __name__ == "__main__":
    unittest.main()
