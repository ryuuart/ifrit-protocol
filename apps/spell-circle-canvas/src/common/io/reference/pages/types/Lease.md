---
kind: type
library: SigilIO
name: Lease
qualified: sigil::io::Lease
group: The hub
status: stable
---

# Lease

## Description

A movable lease that keeps a callback on the hub's advance.

Something that reads feeds on the frame — a reader draining one, a
decoder over what arrived — has to be driven, and the call a host already
makes once a frame is `sigil::io::advance`. This is how that driving is
registered without the host naming the reader: the lease holds the
callback, and releasing it or destroying it takes the callback off the
hub. A lease may outlive its `sigil::io::Hub`, having then nothing left
to unregister from.

`Lease::Callback` is what an advance hands a callback: the
time it was given — an absolute time on the caller's clock, as a
`std::chrono::duration<double>` — which is the time every replayed
recording was just advanced to.

`Lease::release` takes the callback off the hub, which destroying
the lease does anyway. An advance that is already running its callbacks
runs this one out: it holds what it is running.
`Lease::registered` says whether a callback still stands on the
hub through this lease.

## See also

`sigil::io::Hub`, whose `sigil::io::onAdvance` makes one.
