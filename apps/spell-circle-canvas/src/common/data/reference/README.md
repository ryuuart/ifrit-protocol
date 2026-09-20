# SigilData reference

The prose the headers' briefs stand over. A header says what a thing is,
what it accepts, what holds when it is not called, and the one trap a
caller cannot read off the signature; a wire's whole mapping, a file
format's edge cases and a rule that takes a paragraph to derive are on a
page here.

| Page | What it holds |
| --- | --- |
| `pages/types/Connection.md` | the door read as values: the frame that drives it, the schema, the typed reading, the queue and the handlers, sending and answering |
| `pages/types/Table.md` | the typed columns, the cell types, and every reshaping written in terms of one row-picking |
| `pages/types/Scale.md` | the one mapping value: the eleven transforms, what happens outside the domain, and the ladders |
| `pages/types/Json.md` | the document as a value, the two constructors a caller would not guess, and the three shapes a rectangle is published in |
| `pages/types/Schema.md` | a schema as one copyable token, and what both conversions refuse |
| `pages/types/FlatBuffer.md` | a buffer read in place, and the two forms its decoder takes |
| `pages/types/Read.md` | what a generated value header stands on: the readings, the writings, and the one seam a value's reading is named at |
| `pages/types/Database.md` | the two engines behind one seam, and what a query's columns come back as |
| `pages/functions/decodeOsc.md` | an OSC packet as the one dynamic value: what an argument reads as, what a value writes as, and what does not survive the round trip |
| `pages/functions/decodeMidi.md` | a MIDI message as the one dynamic value: the kinds, the fields each carries, and the note on that is a note off |
| `pages/functions/decodeArtNet.md` | an Art-Net packet as the one dynamic value: a universe of dimmers, the other three forms, and the pairs the wire counts in |
| `pages/functions/decodeCsv.md` | delimiter-separated text into a table: the quoting rule, how a column's type is decided, and what a ragged file becomes |

## What this library is for

Meaning only, and only of a table: what a row is, what a column holds,
and where a value lands. Where the bytes came from is SigilIO's, and
what a number says about a population is SigilMeasure's. Nothing here
draws, and nothing here knows what a colour is.

## The decoders on a hub

A hub answers a URI with bytes and hands those bytes to whatever decoder
is registered for the type asked for. `sigil::data::TableDecoder` and
`sigil::data::JsonDecoder` are the two this library carries; after
`sigil::data::registerDecoders`, a data file is
`hub.load<Table>("res://data/deaths.csv")`, a nested record is
`hub.load<Json>("res://data/tree.json")`, and an OSC desk is
`hub.load<Json>("osc://desk:9000")`, cached and reloaded like anything
else the hub holds. A host calls that once, wherever it builds its hub;
registering a type again replaces the decoder later asks run, so a host
that wants a pinned delimiter registers its own table decoder
afterwards.

`registerDecoders` is a template over the hub so this library depends on
the byte vocabulary alone and never on the hub, its cache or its codecs.

`TableDecoder` decides a rectangular format by the resource's name where
it has one — `.csv`, `.tsv` and `.json` — and otherwise by the first
character that is not a space: `[` or `{` is a JSON document and
anything else is delimiter-separated text. The bytes are read as UTF-8,
and a byte order mark at the front is not a field. Its `csv` property's
zeroed props still infer per resource, so one decoder serves comma and
tab files alike; setting one pins every resource that decoder reads.

`JsonDecoder` reads a nested record — the one a rectangle cannot hold —
or an OSC packet, which reads into the same value. THE RESOURCE'S NAME
decides which: a name beginning `osc://` or ending `.osc` is a packet
and everything else is JSON text. A URI's SCHEME is the hint a hub hands
its decoder, so what a resource is named by is what decides how its
bytes are read, and a desk that speaks OSC and a file that holds JSON
both answer `load<Json>` without a second type or a second decoder
standing between them.
