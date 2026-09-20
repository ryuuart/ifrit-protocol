---
kind: function
library: SigilIO
name: registerWebSocketClient
qualified: sigil::io::registerWebSocketClient
group: Transports
status: stable
---

# registerWebSocketClient

## Description

Installs the WebSocket CLIENT on the hub for the schemes "ws" and "wss",
in front of whatever was registered for them: one scheme, and the shape
of the URI is what says which end of it a feed is. A URI of the form
ws://HOST:PORT/PATH (HOST a name, an IPv4 address, or a bracketed IPv6
address) calls that server, and wss://HOST:PORT/PATH calls it over TLS; a
URI naming no host is a port to hold and goes to the transport this one
was installed over, so `sigil::io::registerWebSocket` comes first and a
scheme with no listener behind it refuses such a URI with the reason.

### One session, one thread

Every message the server sends arrives whole, however many frames it was
split into, naming the server as its sender; `Feed::send` writes one
whole message back to it, a client having the one peer it dialled, which
is also the `Feed::address` it reports. Each feed runs one session on a
thread of its own, and the handshake runs there too: a feed is answered
before its server is reached, `Feed::error` carries the reason when it
cannot be, and a send before then goes nowhere and says so. A server that
ends the session closes the feed.

## See also

`sigil::io::registerWebSocket` for the listening end.
