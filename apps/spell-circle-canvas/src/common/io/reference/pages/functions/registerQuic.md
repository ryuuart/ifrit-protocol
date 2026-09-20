---
kind: function
library: SigilIO
name: registerQuic
qualified: sigil::io::registerQuic
group: Transports
status: stable
---

# registerQuic

## Description

Installs the QUIC transport on the hub for the scheme "quic". ONE SCHEME,
TWO SHAPES, and the shape of the URI is what says which end a feed is. A
URI of the form quic://:PORT?cert=FILE&key=FILE holds that port, PORT 0
meaning any free port; quic://HOST:PORT (HOST a name, an IPv4 address, or
a bracketed IPv6 address) calls that end.

### A QUIC connection is encrypted or it is not a connection

So there is no unencrypted form of this door. A port therefore NAMES THE
PAIR IT ANSWERS WITH: `cert` and `key` are the certificate and the
private key, each a path or a URI the hub resolves to one — resolved as
the feed opens, so a URI that names no file opens nothing and leaves the
reason on the feed. A call either trusts what it is shown or says plainly
that it will not look: quic://HOST:PORT?insecure=1 checks no certificate,
which is what reaches a SELF-SIGNED certificate — the ordinary case for a
machine on a stage, where nobody signs for anybody. What that gives up is
the certainty that the machine answering is the one the URI named; what
stays is that everything crossing the connection is encrypted all the
same. Both ends name the protocol "sigil-feed/1" in the handshake, so an
end speaking something else over that port is refused there rather than
left to send bytes nobody reads.

### A message is one unidirectional stream

A send opens a stream, writes the bytes and ends it, and the arrival is
delivered when the end of that stream arrives — so a message keeps its
boundary, and no message waits behind another, a stream that lost a
packet holding up itself alone. The price is that two messages sent one
after the other may land in the other order. A stream that grows past 16
MiB is abandoned rather than held, a message being one whole thing.
`&datagrams=1` on either form sends every message as a QUIC DATAGRAM
instead — unreliable, unordered, and no larger than one packet on the
path carries, so a send of more than that is false, as is one made before
the path has said how large that is. Datagrams are always TAKEN, whatever
a door sends, so a peer that writes one reaches a door whose own URI
asked for nothing.

### A port's peer is a connection

Every connection that reaches a listening feed is a peer named
`quic://ADDRESS#NUMBER`, the number counting the connections that feed
has taken; `Feed::sendTo` writes on the connection it names, `Feed::send`
writes on every connection standing and is false where none is, and a
connection that ends ends that peer. The `Feed::address` such a feed
reports is quic://[::]:PORT, every interface of both families being one
dual-stack socket.

### A call holds the one connection it opened

`Feed::send` writes on it, every message the other end writes is an
arrival naming quic://HOST:PORT — which is also the `Feed::address` such
a feed reports, the query being the door's own arrangement and no part of
what anybody reaches — and `Feed::sendTo` is false on it. The other end
ending the connection closes the feed; a connection that fails after it
was reached fails the feed with the sentence saying it ENDED, which a
call that never reached anything does not say, the two being worth
telling apart by reading the reason. A call that has not been answered
within ten seconds fails the feed that way too, while a machine that says
at once that nothing is listening on that port ends the call there and
then — so a host that swallows the connection and one that turns it away
both answer, rather than either leaving a feed waiting.

### No thread is started for a feed

The library underneath runs workers of its own and calls back onto them,
so the packets, the encryption, the streams and the timers are carried
without this library holding a loop, an executor or a socket.
