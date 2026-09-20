---
kind: type
library: SigilIO
name: SharedMemoryWriter
qualified: sigil::io::SharedMemoryWriter
group: Transports
status: stable
---

# SharedMemoryWriter

## Description

THE OTHER END OF A shm:// REGION: the one process that puts the messages
in it.

Construction makes the shared memory object named `name`, replacing
whatever stood under that name, large enough to hold `capacity` bytes of
payload under the fixed-size header the layout opens with. ONE WRITER PER
REGION: a region carries one count of its messages and not one per
writer, so two writers on a name would write over each other. Destruction
unmaps the region and takes the name back, after which a reader of that
name has nothing to read until another region is made under it.

### Either end may start first

A reader holds the name rather than the memory: a region made after a
feed was opened on its name is one that feed reads, and so is one made
again under a name this writer took back — a reader started once follows
a writer started again.

A writer in another language needs nothing of this class: the layout and
the count are the whole of what the two ends share.

### What a write does

`SharedMemoryWriter::write` puts the bytes in the region as the newest
message: the count goes odd, the size and the bytes are written, and the
count goes even again, so a reader either takes the whole message or sees
that one was being written and looks again. It is false when the region
does not stand and when the bytes are more than the capacity the writer
was made with — a message is written whole or not at all. The same bytes
written twice are two messages: what makes a message new is the count and
not what it says.

`SharedMemoryWriter::open` says whether the region stands. It is false
when the object could not be made or mapped, and every write is false
from then on.

## See also

`sigil::io::registerSharedMemory`, the reading end.
