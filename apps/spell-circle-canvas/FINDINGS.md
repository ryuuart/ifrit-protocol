# Findings

A work queue: each entry states what the code does, what it was evidently
intended to do, and what a test should assert once intent is restored.
Entries are deleted as they are fixed, and the file is deleted when it is
empty.

## The MIDI suite aborts when CoreMIDI refuses a client

**What the code does.** Once, across many runs of the whole `io_test`
binary, the first `IOMidi` case ended the process with an uncaught
`RtMidiError` reading `MidiInCore::initialize: error creating OS-X MIDI
client object (-304)`. Every construction of an `RtMidiIn` or `RtMidiOut`
in `src/common/io/transport/Midi.cpp` stands inside a `try` that turns an
`RtMidiError` into a refused feed, and the suite skips when a port made
rather than found is refused, so the throw that escaped came from a path
neither guards; it did not recur in three further whole-binary runs and
in twelve repeats of the `IOSerial` and `IOMidi` pair.

**What it was intended to do.** A machine, or a moment, in which
CoreMIDI will not hand out a client is a machine with no MIDI, and the
suite's own rule for that is to skip with the driver's sentence, never to
end the binary.

**What a test should assert.** With RtMidi's client creation made to
fail, every `IOMidi` case skips and the binary goes on to the next suite;
and the transport answers a feed whose `error()` carries the driver's
sentence rather than throwing out of `registerMidi`'s opener or out of a
door's destructor.
