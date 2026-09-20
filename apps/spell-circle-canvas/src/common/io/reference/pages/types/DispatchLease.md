---
kind: type
library: SigilIO
name: DispatchLease
qualified: sigil::io::DispatchLease
group: The hub
status: stable
---

# DispatchLease

## Description

A movable lease that keeps a callback on the hub's dispatch.

Something that reads feeds on the frame — a reader draining one, a
decoder over what arrived — has to be driven, and the call a host already
makes once a frame is `Hub::dispatch`. This is how that driving is
registered without the host naming the reader: the lease holds the
callback, and releasing it or destroying it takes the callback off the
hub. A lease may outlive its `sigil::io::Hub`, having then nothing left
to unregister from.

`DispatchLease::Callback` is what a dispatch hands a callback: the
seconds it was given, which are the seconds every replayed recording was
just advanced to.

`DispatchLease::release` takes the callback off the hub, which destroying
the lease does anyway. A dispatch that is already running its callbacks
runs this one out: it holds what it is running.
`DispatchLease::registered` says whether a callback still stands on the
hub through this lease.

## See also

`sigil::io::Hub`, whose `Hub::onDispatch` makes one.
