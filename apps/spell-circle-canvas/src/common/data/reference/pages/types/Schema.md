---
kind: type
library: SigilData
name: Schema
qualified: sigil::data::Schema
group: Buffers
status: stable
---

# Schema

## Description

A SCHEMA AS ONE VALUE, and the generated root that carries one.

A FlatBuffer says nothing about itself: the names of its fields are in
the schema and nowhere in the message. A `Schema` is that schema as one
copyable token, which converts a buffer to its JSON form and a JSON form
back to a buffer without naming the root type again — what a door
reading a wire holds, where the type of the next message is not known at
the call site.

`sigil::data::schema` makes one out of the schema a generated header
embeds, so nothing in the header reads a schema file.
`Schema::fromBinarySchema` makes the same token out of a schema file's
own bytes, for a tool that has no generated header for what it is
looking at and was handed the schema instead. Every way of making a
schema comes through that one call, so a schema made from a file and one
made from a generated type are one schema made one way.

NOTHING OF THE READER UNDER IT IS NAMED IN THE HEADER. The schema is
read, and both conversions are run, by a parser whose headers are opened
in the one translation unit that defines what the token holds. So a
consumer that holds a schema compiles against the standard library
alone. The binary schema is read once and held behind a shared pointer,
so the token is copied for the cost of that pointer and every copy is
the same schema: a scene keeps one in a field and hands it to whatever
reads a wire.

```cpp
const Schema sky = schema<feed_sky::Sky>();
const std::optional<std::string> form = sky.text(arrived);
const std::optional<std::vector<std::byte>> out = sky.binary(text);
```

### The root is the one the schema file declares

A generated header embeds its file's whole schema beside every type in
it, so naming another type of the same file makes the same schema;
`Schema::rootName` says which root both conversions go through, fully
qualified — `feed_sky.Sky`. The view is into the schema and stands as
long as it does.

`sigil::data::CarriesSchema` is whether a generated root carries its
binary schema — a header written with the embed flag — which is what
converting the JSON form needs.

### Both refuse what does not fit

Rather than answering part of it: a buffer is verified against that root
before a byte of it is read, and text carrying a field the schema does
not declare is no buffer. So a reader holding a schema knows that what
it got back means what the schema says it means.

`Schema::verifies` is that check on its own, for a holder that wants the
answer and not the reading: false for a buffer of another schema, one
cut short, and for a schema that is none, with `why` saying which where
it is asked for. So bytes that are kept and read field by field later
are proved once rather than converted whole to prove them.

`Schema::text` answers the buffer as the schema's own JSON form. The
bytes are verified against the root first, so a buffer of another
schema, or one cut short, answers nothing rather than a reading of
whatever the bytes happened to be.

`Schema::binary` answers the buffer the JSON text makes, read through
the schema. Nothing where the text does not fit it — a field the schema
does not declare, a value of the wrong type, text that is no document —
and the `why` out-parameter carries the parser's own message, the line,
the column and the field, where it is asked for. What comes back
verifies as the root, so whoever is handed it may read it in place.

A schema made from bytes that are no schema, or from one that declares
no root type, is none: a schema with no root has nothing to read a
buffer AS. The bytes are copied, so the caller keeps nothing for it.

## See also

`sigil::data::FlatBuffer`, `sigil::data::Connection`,
`sigil::data::values::Read`.
