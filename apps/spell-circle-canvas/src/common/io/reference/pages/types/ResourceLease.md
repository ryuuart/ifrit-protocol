---
kind: type
library: SigilIO
name: ResourceLease
qualified: sigil::io::ResourceLease
group: The hub
status: stable
---

# ResourceLease

## Description

A movable lease that keeps an inspectable set of resource URIs resident.

Selectors are snapshots. `ResourceLease::include` adds a selector and
immediately refreshes the union; `ResourceLease::refresh` reruns every
selector so newly created files join and vanished files leave. Multiple
leases may retain the same URI independently. Destroying a lease releases
only its own claim. The `sigil::io::Hub` must outlive calls on its
leases, but a lease may be destroyed safely after its hub.

`ResourceLease::preload` loads the retained resources into their hub's
byte cache concurrently, and `ResourceLease::uris` answers the sorted,
duplicate-free URI snapshot the lease currently retains.

## See also

`sigil::io::Hub`, whose `Hub::retain` makes one, and
`sigil::io::DispatchLease`.
