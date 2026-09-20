---
kind: type
library: SigilIO
name: RecordingWriter
qualified: sigil::io::RecordingWriter
group: The hub
status: stable
---

# RecordingWriter

## Description

THE RECORDING FORMAT: a feed's arrivals as a file, written as they come
and read back as a list. `sigil::io::RecordingWriter` is a recording
being written — the file is emptied on the way in, and every arrival is
appended to it as it comes — and `sigil::io::readRecording` reads one
back.

### The format

A recording opens with the line `sigil-feed-recording 1`. Every frame
after it is the arrival's time as a double, then the message's length as
a 32-bit unsigned, then that many bytes. Both numbers are in the byte
order of the machine that wrote them: a recording is read by that machine
and by its peers rather than carried between architectures, and every
target this tree builds for is little-endian.

A frame reaches the disk as it arrives, so a run that is killed leaves a
file whose last frame may be cut short. Reading drops that frame and
keeps every whole one before it.

### What a recording does not carry

An arrival's generation is not written down. It counts the messages one
feed has taken, which is a property of the feed rather than of the
recording, so a reader numbers the frames 1, 2, 3 as it reads them.

Neither is the address a message came from. A recording is the messages
and not who sent them, so a replayed arrival names no sender.

### Reading and writing

`RecordingWriter::append` appends one frame and puts it on the disk. It
is false when the file cannot take it, which leaves every frame already
written whole. `RecordingWriter::good` says whether the file is open and
everything written so far reached it.

`sigil::io::readRecording` answers every arrival a file holds, in the
order it lists them and numbered from 1; nothing when the file cannot be
read or does not open with the format's header.

## See also

`sigil::io::Feed`, whose `Feed::record` writes one and whose
`Feed::advance` plays one back.
