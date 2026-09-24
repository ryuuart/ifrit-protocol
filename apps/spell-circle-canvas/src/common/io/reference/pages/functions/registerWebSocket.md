---
kind: function
library: SigilIO
name: registerWebSocket
qualified: sigil::io::registerWebSocket
group: Transports
status: stable
---

# registerWebSocket

## Description

Installs the WebSocket transport on the hub for the scheme "ws". A URI of
the form ws://:PORT/PATH listens on every interface for peers reaching
that path, PORT 0 meaning any free port and an omitted PATH meaning "/".
Every text or binary message from any peer arrives in the feed naming the
peer it came from, and `Feed::send` goes out to every peer on that path
at once, while one named peer is answered alone through `Feed::sendTo`.
Each listening feed runs on a thread of its own that stands until the
feed is closed.

### A URI's query may name where its pages stand

ws://:PORT/PATH?pages=URI — and the same port then answers HTTP GET out
of that directory, so what a peer loads and the socket it opens back are
one address. "/" and "/index.html" are the directory's index.html; any
other path is the file of that name beneath the directory, typed by its
extension; a path naming no file there, one climbing out through "..",
and every request to a listener whose URI named no pages at all, are
answered 404. The URI is resolved through the hub's mount table as the
feed opens — a URI that resolves to no directory opens nothing and leaves
the reason on the feed — and what the listener keeps afterwards is the
directory itself, read again per request, so a page edited on disk is the
page the next reload is served. The path peers reach is the PATH alone: a
query is the listener's own arrangement and stands in neither the address
the feed reports nor the one an arrival names. A pages value that is an
absolute path is that directory itself, with no mount in between.

### Where it stands and whom it admits

`?bind=ADDRESS` holds that one interface instead of every one, and the
address the feed reports is that interface's, so a listener bound to
127.0.0.1 cannot be reached from another machine. `?admit=ADDRESS`,
written once per peer, with `admit=loopback` for every loopback address,
lets in those peers alone: any other is answered 403 with a sentence
naming it, before the upgrade and before a page, and never becomes a
peer. A listener that names nobody admits everybody; an admission or a
bind that is no IP address opens nothing and leaves the reason on the
feed. `Feed::peers` names the peers attached now, so a door learns that
one has left when its name is gone.

### What it sends

A send goes out as a binary frame, since a feed carries bytes;
`?frames=text` sends text frames instead, for peers that read every
message as a string, and what is sent must then be UTF-8. Any value but
`text` or `binary` opens nothing and leaves the reason on the feed.

### This registration listens

The sockets underneath carry no client and are built without TLS, so a
URI naming a host to call opens nothing here and leaves the reason on the
feed, and there is no "wss" it could hold a port for.
`sigil::io::registerWebSocketClient` is what takes those URIs, and it
stands in front of this.

## See also

`sigil::io::registerWebSocketClient` for the calling end, and
`sigil::io::registerTransports`, which installs the listener first and the
client in front of it.
