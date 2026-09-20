---
kind: function
library: SigilIO
name: registerSharedMemory
qualified: sigil::io::registerSharedMemory
group: Transports
status: stable
---

# registerSharedMemory

## Description

Installs the SHARED MEMORY reader on the hub for the scheme "shm". A URI
of the form shm://NAME maps the shared memory object of that name and
delivers what the one writer of that region puts there, so a message
crosses from another process on this machine through memory both of them
have mapped, with no socket beneath it and no kernel on the way.

### The reader never writes and never waits

The region is mapped read-only, and it carries a number that counts the
messages written into it: odd while one is being written and even once it
is whole. So a read that brackets its copy between two equal even numbers
took a message nobody was writing over, and one that does not is dropped
and read again at the next look.

### Nothing pushes, so the feed looks

shm://NAME?rate=HERTZ reads the region a whole number of times a second,
120 times where the URI names no rate, and every message the writer
leaves standing between two looks is one arrival. Every arrival names the
region as its sender, spelled shm://NAME as the `Feed::address` the feed
reports is — the rate being the reader's own arrangement and no part of
what the region is called. A reader has no way back to a writer through
the region, so `Feed::send` and `Feed::sendTo` are false on such a feed: a
scene that must answer holds another door for that. The looks of every
feed opened through one registration run on one thread, made when the
first of them opens.

### What a look looks at is the name

So THE TWO ENDS MAY START IN EITHER ORDER. A feed opened on a name nobody
has made a region under is a door onto nothing rather than one that
failed — it reports that region as its `Feed::address`, nothing as its
`Feed::error`, and delivers the moment a writer makes one. A door that
has a mapping maps whatever stands under the name afresh about once a
second — a region made again under a name is another object wearing that
word, which is what a writer started again leaves behind it — and lets
its mapping go where the name is gone. A shared memory object carries no
identity a reader could ask for, so what a door compares is the message:
one that is not the message it last delivered is one to deliver, whatever
count it stands under, which is what the first message of a region made
again is. A region that stands but is not one to read, one whose first
bytes are not this layout's among them, is left where it is with the
reason on the feed and the door still standing. Only a URI naming no
region at all opens nothing.

## See also

`sigil::io::SharedMemoryWriter`, the other end of such a region.
