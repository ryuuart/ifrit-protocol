# Seer — the wires, the readings and the way out

The chapter on what Seer's wire library is made of: the list of feeds a
reader watches, the seven ways one message is shown, the peer a message
goes to, and the file a wire is written down to and played back from.
`README.md` beside the library is the front page.

## Every wire at once

A wire is one URI a message arrives on or leaves by. Opening one adds it
to a list that keeps the order it was opened in, so a reader looks down
the same column from one frame to the next. A URI nothing can open is
still a wire: the feed exists and carries the sentence that says why,
which is what a reader has to see to correct it.

Nothing in the library has a thread or a clock of its own.
`Wires::dispatch` moves every replayed recording to the caller's seconds
and `Wires::tick` reads the wires and writes down what they are doing,
both of them driven by whatever loop the host runs.

`Vitals` is WHAT ONE WIRE IS DOING, as of the tick that read it. It is a
plain value: a reader compares this frame's against the last one's rather
than asking the feed a second time, and the feed is free to take another
message while the reader is looking.

`Vitals::arrivalsPerSecond` is messages a second over the last second of
ticks, and zero until a second of ticks has gone by.
`Vitals::closed` is whether the feed takes anything more — a recording
that has played out closes itself, and so does a socket the system
ended.

`Vitals::lastFrom` is the address the newest message on this wire came
from, spelled the way a URI of its scheme is. It is read off the arrival
itself rather than off whoever drained it, so every wire names its sender
and not only the one being read. It is empty before the first message, on
a recording, and on a transport that cannot tell who sent what it
delivered.

## Wires: the list, and the one hub under it

Every transport this build carries is registered on that hub when the
list is made, so a `udp://` URI opens a socket without the caller naming
a transport. A URI that resolves onto a file is played back from it
instead, which is what `Wires::mountRecording` arranges.

One schema stands over all of them, or none does. It is the wires' and
not one wire's because a reader who was handed a schema was handed it for
the messages, and the same messages cross whichever wire the sender
happened to open.

`Wires::open` opens a URI and keeps the feed, so the wire stays open
until it is closed rather than for as long as the caller holds the
answer. A URI already open answers the feed that is already there, and
its policy is the one it was opened with. A URI nothing can open answers
a feed whose error says so.

`Wires::close` drops the wire from the list, and answers false when no
wire is open on that URI. A feed that closes itself — a recording played
out, a socket the system ended — stays in the list and says it is closed,
because it is still a wire somebody opened.

`Wires::mountRecording` resolves a URI onto a recording on disk, so the
next open on that URI plays the file back instead of opening a socket. A
wire already open on the URI is unaffected: it holds the door it was
given, so it is closed first by whoever wants the file.

`Wires::dispatch` moves every replayed recording to a time on the
caller's clock, delivering each arrival the file stamped at or before it.
A live wire is unaffected — its transport delivers on its own.

`Wires::readThrough` reads every wire through a schema from then on: the
token a message is shown as its own form through, which a reader holds
once they have been handed the schema the sender's build wrote. A schema
that is none takes that reading away again.

`Wires::tick` reads every wire and writes down what it is doing at a time
on the caller's clock. The rate is worked out from the generations
earlier ticks read, so it is a property of how often it is called and
needs no thread behind it.

## Writing a wire down, and playing it back

Recording appends every arrival from the moment it starts to a file; what
came before is not in the file, because the feed handed those messages
out already.

Replaying is the same URI opened onto that file instead of onto a socket,
so everything downstream — the list, the log, the readings — is looking
at the same kind of wire it was looking at live, and the messages arrive
as time is moved forward rather than all at once.

## A message made readable

The same bytes are shown seven ways, each of which answers an empty
string when the bytes are not that — and two more things a URI says about
the wire itself: the word its scheme speaks in, and the short spelling of
an address.

Nothing there decides what a message IS. A reader looking at an unknown
wire wants all of them at once — the bytes as they stand, the text if
they happen to be text, the document, the packet, the message off an
instrument or the universe off a lighting desk if they happen to be one —
and the emptiness of a reading is the answer that they are not.

Six of the seven need nothing but the bytes. The seventh needs a schema,
because a buffer read in place says nothing about itself: the names of
its fields are in the schema and nowhere in the message, so a reader who
has not been handed one sees bytes.

`hexadecimal` is the first bytes as lower-case hexadecimal pairs
separated by single spaces, with an ellipsis after the last pair when the
message is longer than the limit.

`printableText` is the bytes as text, when every one of them is part of
well-formed UTF-8 and none is a control character other than a tab, a
newline or a carriage return. A message that is half text is not text: a
reader who was shown the readable half would take the whole message for a
broken string rather than for bytes.

`indentedJson` is the bytes as an indented document, when they parse as
one. Members keep the order the document wrote them, and a member that is
a list or a record of its own is indented under its key.

`oscReading` is the bytes as an OSC packet — an address with its
arguments under it, or a bundle with the packets it carries and the time
they are for — written out indented, the same shape a document is written
in. Two readings of one message in one shape is how a reader comparing
them sees what differs rather than how each was printed.

`midiReading` is the bytes as one MIDI message: its kind, the channel it
was played on, the numbers that kind carries and the bytes they went out
as. A message's own bytes are a run along a cable rather than a list of
separate values, so they stand across one line instead of one to a line:
what a reader counts through is the run.

`dmxReading` is the bytes as one Art-Net packet — a universe of dimmers
with the address it is for, a poll, or whatever else arrived under a name
of its own. A universe is up to 512 dimmers, and a column of 512 numbers
is a column nobody reads: a rig that fits one line stands whole beside
its key, and a longer one is rows of sixteen, which is how a desk counts
its own channels across.

`schemaReading` is the bytes read through a schema. A buffer that
verifies as the schema's root is converted to the schema's own form;
bytes that are that form already are converted to a buffer and back, so
what a reader is shown is what a door reading this wire through the same
schema would hold, defaults and all. It is empty when the message is
neither and when there is no schema, with the optional reason carrying
the sentence that says which.

`dialect` is THE WORD THE SCHEME OF A URI SPEAKS IN: "osc" for a wire of
packets, "midi" for what an instrument says, "dmx" for a universe of
dimmers, "lines" for a cable that ends a message at a newline, "json" for
a socket that carries documents, and "bytes" where the scheme carries
whatever a sender puts on it. It is empty for a scheme there is no word
for, which is a wire that opens and carries messages all the same — a
word put on a scheme nobody taught this would be a claim about a wire
that does not exist.

It is read off the URI and not off a message, because a reader picks the
wire to watch before anything has arrived on any of them. It is what the
wire SPEAKS and not what one message turned out to be: a socket named for
documents still carries a message that is none, and the readings are
where that shows.

`hostAndPort` is the end of a URI without the scheme in front of it: what
follows the `://`, which for an address a transport spells is the host
and the port. A sender is shown beside the wire it arrived on and that
wire's scheme is spelled there already, so repeating it on every line
would crowd out the part that differs. A URI with no scheme in it is
itself.

## Sending down a wire

The peer a message goes to, one message or the same message again and
again driven by the caller's own clock, and the messages a reader spells
rather than types out byte by byte.

A wire that speaks a format is answered in what it speaks. The
hexadecimal a reader would otherwise type is a header, a set of type
tags, a status byte or a run of padding all at once — which is a message
nobody spells twice without a mistake in it — so the three wires that
name their format carry the message a reader means instead: an address
with its arguments, a note played on a channel, a universe of dimmers.

The peer is a wire like any other — it is opened through the list, it
stands in the same list, and what the peer sends back arrives on it — so
a reader watches the answer to what was just sent without opening
anything else. The repeat has no thread: `Sender::tick` is what sends, so
a host that stops calling it stops sending, and nothing is in flight once
it returns.

`oscMessage` is THE BYTES OF ONE OSC MESSAGE: an address, and the
arguments a JSON document spells — a list being the arguments in order,
any other value the one argument it is, and nothing at all a message
carrying none. It is empty when the address is empty, when the arguments
are not a document, and when the message will not fit one packet, so half
a message never goes out. An address is where on the instrument at the
other end the message lands, and it is the one part of a packet that is
not an argument.

`midiMessage` is THE BYTES OF ONE MIDI MESSAGE: a kind — "NoteOn",
"NoteOff", "PolyAftertouch", "ControlChange", "ProgramChange",
"Aftertouch" or "PitchBend" — played on a channel, 1 to 16, carrying two
numbers, which are the numbers that kind takes in the order the wire
carries them: the note and how hard it was struck, the key and the weight
leaned on it, the controller and where it now stands. A kind that takes
one number — a program, a whole keyboard's weight, a wheel's distance
from centre — takes the first and leaves the second off the wire.

It is empty for a kind the wire has no status byte for, so half a message
never goes out. A number past what its place on the wire holds is written
at the nearer end of that place rather than wrapped, since a wrapped note
is a note nobody played.

`MidiWords` is ONE MIDI MESSAGE WRITTEN IN WORDS: the kind, the channel
it is played on, and the numbers that kind carries, which is the same
message `midiMessage` spells and is how it is said where there is no form
to fill in. The second number stands at nothing for a kind that carries
one, since the wire carries none for it.

`midiWords` reads WHAT THOSE WORDS SAY: the kind, the channel and the
numbers that kind carries, blanks between them and nothing else —
"NoteOn 1 60 100" for a kind that carries two numbers and
"ProgramChange 2 7" for a kind that carries one. It answers nothing when
the first word is no kind a channel carries, when a number is not written
out whole, and when there are more or fewer numbers than that kind takes,
so a number nobody said is never played as one they did.

`dmxMessage` is THE BYTES OF ONE ART-NET PACKET: a universe of dimmers,
whose levels a JSON list of numbers spells — each 0 to 255, in the order
the desk numbers them, so the channel a desk calls 1 is the first of the
list. It is empty when the levels are not a list, so a desk is never sent
half a universe; a list with nothing in it is every fixture dark, which
is a thing a desk says.

`Sender::openPeer` opens a URI as the peer every send reaches from then
on, through the wires the sender was made over. A peer that cannot be
opened is answered all the same, carrying the sentence that says why.

`Sender::send` sends bytes to the peer once, and is false when there is
no peer, when the wire is one-way, and when it is closed.

`Sender::repeat` sends bytes every period of seconds for as long as
`Sender::tick` runs, beginning with the next tick; a period that is not
positive stops the repeat instead. `Sender::stopRepeating` takes the
repeat off and leaves the peer and the message it was sending.

`Sender::tick` sends the repeated message when one is due at a time on
the caller's clock. A host that stalled sends one message and waits a
whole period again: what was missed while nothing was running is not made
up for in a burst.

## See also

`README.md` beside the library for the application over these.
