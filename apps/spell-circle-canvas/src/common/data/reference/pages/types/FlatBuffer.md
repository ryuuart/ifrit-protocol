---
kind: type
library: SigilData
name: FlatBuffer
qualified: sigil::data::FlatBuffer
group: Buffers
status: stable
---

# FlatBuffer

## Description

A FLATBUFFER AS A VALUE, and the decoder that puts one on a hub.

There are two of them, for the two things a holder may know about the
message: `FlatBuffer<Root>` where a generated header names the root, and
`sigil::data::SchemaBuffer` where a schema token names it instead. The
first reads fields through the generated accessors; the second reads the
schema's own JSON form, which is all a holder with no generated header
can name a field by.

A FlatBuffer is read in place: its root is a pointer into its bytes, so
the value IS the bytes, verified once against the schema and read
through the generated accessors from then on. The decoder answers a Root
for the bytes a hub hands it whether they are the buffer itself or the
schema's own JSON form — a scene written by hand, or sent by something
that speaks JSON — which the schema converts. After
`sigil::data::registerFlatBuffer`, `hub.load<FlatBuffer<Root>>(uri)`
answers, cached and reloaded like any resource the hub holds.
Registering a root again replaces the decoder later asks run.

### The schema comes from the generated code

A header written with `flatc --cpp -b --schema --bfbs-gen-embed`
carries the binary schema beside every root, as `Root::BinarySchema`, so
nothing in the header reads a schema file. A header written without it
still decodes the buffer itself; only the JSON form needs the schema,
and a root that carries none refuses that form.

`sigil::data::flatBufferFromBytes` verifies the bytes before any of them
is read, and answers nothing when they are not one — a truncated buffer,
another schema — with the `why` out-parameter saying so where it is
asked for. `sigil::data::flatBufferFromJson` converts the schema's own
JSON form through the schema the generated root carries and verifies it
as one; the `why` carries the reader's own message, the line, the column
and the field.

`sigil::data::flatBufferLooksLikeJson` is how the two forms are told
apart: read from the resource's name where it has one, and otherwise
from its first byte that is not a space, since the JSON form opens with
a brace or a bracket and a buffer does not.

### When the root is a schema and not a type

`sigil::data::SchemaBuffer` is the same bytes verified against the root
a `sigil::data::Schema` names and read back through it — what a holder
that compiles no generated header carries: a tool handed a `.bfbs`, a
process that was given the schema and nothing else. `SchemaBuffer::text`
answers the buffer's own JSON form rather than typed accessors, since
the names of the fields are in the schema and nowhere in the message,
and the form is made on each ask rather than held, the value being the
bytes. `SchemaBuffer::schema` hands the token on to whatever reads the
next message, and `SchemaBuffer::bytes` is what a sink writes or a feed
sends. Two are equal when their bytes are and their schemas name one
root, so one schema read twice out of a `.bfbs` is one schema here.

`sigil::data::schemaBufferFromBytes` and
`sigil::data::schemaBufferFromJson` are the two makings, refusing what
does not fit exactly as the generated-root pair does, and
`sigil::data::registerSchemaBuffer` puts the decoder on a hub so
`hub.load<SchemaBuffer>(uri)` answers. A hub holds one decoder per type
and a `SchemaBuffer` is one type however many schemas there are, so a
second call replaces the schema later asks are read through: a hub
reading two wires of different schemas at once wants a hub for each.

### What the header opens

It speaks SigilIO's byte vocabulary and flatbuffers, and nothing else of
SigilIO: `sigil::data::registerFlatBuffer` is a template over the hub.
Of flatbuffers it opens the buffer's own header alone — the verifier
that checks bytes and the accessor that reads a root out of them —
because that is what this value IS. The reader that converts the JSON
form is named nowhere there: the conversion goes through a
`sigil::data::Schema` token, which is one pointer to a state defined out
of sight.

## See also

`sigil::data::Schema`, `sigil::data::values::Read`.
