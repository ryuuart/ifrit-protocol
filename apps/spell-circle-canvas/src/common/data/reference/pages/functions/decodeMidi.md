---
kind: function
library: SigilData
name: decodeMidi
qualified: sigil::data::decodeMidi
group: Wires
status: stable
---

# decodeMidi

## Description

A MIDI MESSAGE AS THE ONE DYNAMIC VALUE, and back out again.

MIDI is what the instruments in a room say to one another: a pad was
struck this hard, a knob stands here now, a wheel was pushed that far
from centre. Here one message is read into a `sigil::data::Json` — the
same nested, ordered, comparable value a document is read into — so a
sketch reading a controller and a sketch reading a file work one value
rather than two.

A MESSAGE IS ITS KIND, the channel it was played on, and the fields that
kind carries:

```json
{"kind": "NoteOn", "channel": 1, "note": 60, "velocity": 100,
 "status": 144, "data": [60, 100]}
```

`kind` is one of `NoteOn`, `NoteOff`, `PolyAftertouch`, `ControlChange`,
`ProgramChange`, `Aftertouch`, `PitchBend`, `SystemExclusive`, `Clock`,
`Start`, `Continue`, `Stop`, `ActiveSensing`, `Reset`, and `System` for
a system message this codec has no name of its own for. `channel` is 1
to 16 and stands on the channel messages alone — the six kinds through
`PitchBend` — a system message being addressed to the room rather than
to a channel in it. The named fields are the kind's own:

- `NoteOn`, `NoteOff` — `note` and `velocity`
- `PolyAftertouch` — `note` and `pressure`, one key leaned on
- `ControlChange` — `controller` and `value`, which is a knob
- `ProgramChange` — `program`
- `Aftertouch` — `pressure`, the whole keyboard leaned on
- `PitchBend` — `bend`, -8192 at the bottom of the wheel's travel, 0 at
  its centre and 8191 at the top; the wire's two halves are put back
  together here, so nothing reading this value has to know that it
  arrived in sevenths
- `SystemExclusive` and `System` — `bytes`, every byte of the message
  including the one it opens with, which is what writes it back out
  verbatim

AND TWO FIELDS EVERY MESSAGE CARRIES: `status`, the byte the message
opens with, and `data`, the bytes after it that are not status bytes
themselves — empty on a message that carries none, and on a system
exclusive message the payload without the byte that ends it. So a reader
that knows the wire reads the wire, and a reader that does not reads the
names.

### A note on at velocity zero is a note off

And is read as one, with a velocity of 0: it is how a keyboard says a
key was released, and a scene that acted on the kind alone would hold
every note it was ever played. It is the one form that does not go back
out as the bytes it came in as — the reading names a `NoteOff`, and a
`NoteOff` writes the status byte a note off carries.

### What goes back out

`sigil::data::encodeMidi` writes the same forms: the kind and the
channel say which status byte, the named fields say the data bytes, a
channel left out is channel 1, and a value outside what its place on the
wire holds is written at the nearer end of it, since wrapping it would
put a note nobody played on the cable. A record carrying `bytes` is
written exactly as those bytes stand, whatever else it says, which is
how a message this codec has no name for goes back out the way it
arrived. A value with neither a kind this knows nor bytes of its own has
no spelling on the wire and writes as nothing at all, an empty message
being no shorter message.

`sigil::data::decodeMidi` answers nothing when the bytes are no MIDI
message: one that opens with a byte the wire cannot open a message with,
one that carries fewer data bytes than its kind takes or more, and one
whose data bytes are not data bytes. A message is read whole or not at
all, so nothing here reads past what arrived and nothing answers half of
what it was given.

This codec is its own: it reads and writes the wire directly, so nothing
here or behind it names a dependency and a consumer compiles against the
standard library and this library's own value.
