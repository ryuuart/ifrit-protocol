# SigilIO reference

The prose the headers' briefs stand over. A header says what a thing is,
what it accepts, what holds when it is not called, and the one trap a
caller cannot read off the signature; everything longer than that — a URI
grammar, a wire format, a rule that takes a paragraph to derive — is on a
page here.

| Page | What it holds |
| --- | --- |
| `pages/types/Hub.md` | the mounted-URI walkthrough, the one-entry-per-URI rule, the network cache and the policies, feeds and dispatch, selection, both probes |
| `pages/types/Feed.md` | the door and its readers, the capacity and what dropping means, sending back, failing without closing, recording and replay |
| `pages/types/ResourceLease.md` | what a retention lease promises and when a selector is re-run |
| `pages/types/DispatchLease.md` | the callback the frame's own call drives |
| `pages/types/RecordingWriter.md` | the recording format, byte for byte, and what it deliberately does not carry |
| `pages/types/ByteSource.md` | the byte vocabulary in both directions: the four concepts, the probe seam, the type-erased source, the two filesystem ends |
| `pages/types/ArchiveSource.md` | a zip held whole in memory, and the entry ceiling that reads a claim before it allocates for one |
| `pages/types/SharedMemoryWriter.md` | the writing end of a shm:// region |
| `pages/functions/register*.md` | one page per door: the URI grammar it answers to, what a message is on it, which way it sends, and what thread it runs on |

## The doors

`sigil::io::registerTransports` installs every transport this feature
carries: UDP, the OSC name over it, WebSocket at both ends — the listener
first, and the client in front of it — the shared memory reader, MIDI,
the serial port, gRPC at both ends, QUIC at both ends, and WebRTC over a
signalling door of the same hub.

Three of them carry no socket at all: a shm:// feed reads a region of
memory another process on this machine has mapped, a midi:// feed is a
port on the controller standing beside the screen, and a serial:// feed
is the device file a board on a cable is plugged into — the same door
with the network taken out from under it.

## The network cache

`sigil::io::probeNetworkCache` checks metadata, not file contents, and
creates no directories or files. `sigil::io::seedNetworkCache` publishes
a complete resource on a successful write; a failure leaves any previous
resource intact, and empty bytes are valid. Later disk-cache reads answer
seeded bytes until another seed or fetch replaces them, while a
`sigil::io::Hub`'s already-loaded views keep their values. An empty cache
directory selects the same platform cache directory as a hub given no
override.

## A catalogue of authored text

`sigil::io::TextCatalog` is one directory of text resources mounted at
one URI prefix — the shape a directory of AUTHORED shaders takes: a
consumer keeps its `.sksl` or `.slang` files wherever it likes, asks for
each by name, and may warm the whole directory before the first ask. A
stock value over the hub, so a catalogue is a declaration rather than a
hub, a mount and a lookup written out again. A shader a library SHIPS is
not this: it is compiled into that library's archive, and reading one
costs no hub.

`TextCatalog::text` is the file beneath the directory, through the hub's
cache, so a file edited on disk reaches the next ask after `Hub::poll`.
`TextCatalog::preload` fetches every file beneath the directory
concurrently. The hub is reachable through `TextCatalog::hub` for
anything else — a typed decode, a poll, a lease.

## Where the platform answers

`sigil::io::scratchDirectory` creates nothing: the caller decides whether
a stale directory from a run that died is emptied or swept, which is not
one answer.
