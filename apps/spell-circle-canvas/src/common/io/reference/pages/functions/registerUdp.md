---
kind: function
library: SigilIO
name: registerUdp
qualified: sigil::io::registerUdp
group: Transports
status: stable
---

# registerUdp

## Description

Installs the UDP transport on the hub for the schemes "udp", "osc" and
"artnet". A URI of the form udp://:PORT listens on every interface, IPv4
and IPv6 alike, PORT 0 meaning any free port; udp://HOST:PORT (HOST a
name, an IPv4 address, or a bracketed IPv6 address) opens a socket that
sends to that peer and receives whatever comes back to it. Every socket
the transport opens runs on one thread of its own that lives as long as
the hub holds the transport.

A listening socket holds no one peer, so nothing goes out of it by
itself; what it can do is answer ONE sender, by the address that
sender's datagram arrived from — which is `Feed::sendTo`.

### osc:// and artnet:// are the same socket

osc:// is that same socket under another name: an osc://:9000 feed is a
UDP socket whose messages are OSC packets. artnet:// is that same socket
again, for the datagrams a lighting desk sends: an artnet://:6454 feed
listens for them and an artnet://HOST:6454 feed is a desk to send them
to. A feed keeps the scheme it was opened with — in its `Feed::uri`, in
the local `Feed::address` it reports and in the sender every arrival
names — so a reader picks the decoding off the URI rather than out of
the bytes.

## See also

`sigil::io::registerTransports` installs this door with every other one.
