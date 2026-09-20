---
kind: function
library: SigilIO
name: registerGrpc
qualified: sigil::io::registerGrpc
group: Transports
status: stable
---

# registerGrpc

## Description

Installs the gRPC transport on the hub for the scheme "grpc". ONE SCHEME,
TWO SHAPES, and the shape of the URI is what says which end a feed is. A
URI of the form grpc://:PORT/Service/Method holds that port and serves
that one method, PORT 0 meaning any free port;
grpc://HOST:PORT/Service/Method (HOST a name, an IPv4 address, or a
bracketed IPv6 address) calls that method there. BOTH ENDS NAME THE
METHOD, so a URI carrying a service and nothing after it opens nothing
and leaves the reason on the feed.

### The method is schema-agnostic

It carries bytes in both directions and nothing here parses one, so a
feed's buffers, its JSON and whatever else a sender writes cross it with
no generated stub in the transport. What a message MEANS is the business
of the library that owns the format, exactly as on every other door.

### A server's peer is a call and not a caller

Every call that arrives is a stream of its own, one caller may hold
several at once, and each message written on one is an arrival naming
that call — `grpc://ADDRESS#NUMBER`, the number counting the calls that
feed has taken. `Feed::sendTo` writes on the call it names, `Feed::send`
writes on every call standing and is false where none is, and a caller
that ends its half of the stream ends that peer. The `Feed::address` such
a feed reports is grpc://[::]:PORT/Service/Method, every interface of
both families being one dual-stack listener.

### A client holds the one call it opened

`Feed::send` writes one message on it, every message the server writes is
an arrival naming the URI that was called — which is also the
`Feed::address` such a feed reports — and `Feed::sendTo` is false on it,
a client having the one peer it called. The server ending the call closes
the feed, and a server that goes away mid-conversation fails it with the
sentence saying the call ENDED — which a call that never reached a server
at all does not say, the two being worth telling apart by reading the
reason. A channel that has not connected within ten seconds fails the
feed that way too, and a refused connection says it at once instead of
waiting the bound out.

### No TLS at either end

In this cut, so both a server and a call stand on a machine or a network
somebody already trusts; a "grpcs" scheme carrying credentials is the
step after this one and is not registered.

### No thread is started for a feed

gRPC runs threads of its own and calls back onto them, so a server's
callers and a client's stream are carried without this library holding a
loop, an executor or a completion queue of its own.
