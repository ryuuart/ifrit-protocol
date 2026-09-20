---
kind: type
library: SigilData
name: Read
qualified: sigil::data::values::Read
group: Buffers
status: stable
---

# Read

## Description

WHAT A GENERATED VALUE HEADER STANDS ON: the readings and the writings
every schema needs, with no schema in any of them.

A FlatBuffer is read in place, and a generated accessor answers a
pointer into the bytes: a string is a `flatbuffers::String*` that may be
null, a vector is a `flatbuffers::Vector*` that may be null, and a
struct is a pointer at an offset. A consumer that wants a value —
something it can copy, hold past the bytes, compare and edit — spells
the same three conversions at every field. These are those conversions,
written once.

### Absent is the empty value

For a string and for a vector: the wire carries no difference between a
field left out and one written empty, so a reading that answered an
optional would invent one. A field the schema declares REQUIRED is the
exception, and the generated reading refuses a buffer that left it out
rather than answering an empty value for it.

### The verifier is run in one place

`sigil::data::values::rootOf`, which every generated reading of a whole
buffer goes through, so bytes that are not the schema's answer nothing
rather than a reading of whatever they were. Reading a table that is
already inside a verified buffer runs no second verification. The
pointer it answers is into the bytes and stands as long as they do,
which is why every generated reading copies out of it.

### A value's own reading is named once

`sigil::data::values::Read` is the seam a generated header specializes:
it says how that value is read out of bytes, so a reader that names the
value type reaches the reading without naming it, and a door templated
over the value compiles against this header alone. A specialization
declares

```cpp
static std::optional<Value> from(std::span<const std::byte> bytes);
```

which verifies the bytes against that value's root and answers nothing
where they are not it. A generated header writes one for every table of
its schema, beside the reading it stands on.

The primary template is left UNDEFINED, so a value nobody wrote a
reading for is a name that cannot be completed rather than a reading
that always answers nothing.

`sigil::data::values::Readable` is whether a value has a reading of its
own, which is what a door asks of the type it is handed before it reads
a message as one. A struct has none — it travels inline inside a table
and is never a root — and neither has a type from outside a schema.

### The readings and the writings

`sigil::data::values::bytesOf` copies out the buffer a builder has
finished: the builder owns its memory until it goes, so a writing that
hands the buffer back copies it out.

`sigil::data::values::readBools` and
`sigil::data::values::writeBools` exist because the wire holds one byte
an entry, which is why the accessor under the reading answers bytes.
`sigil::data::values::readEnums` and `sigil::data::values::writeEnums`
exist because a vector's entries are the enum's underlying integer on
the wire, so the value names the enumerated type and those two are where
the two meet.

`sigil::data::values::readEach` reads one value an entry, for a vector
whose entries always read — a vector of structs, which have no absent
form. `sigil::data::values::readEachOrNone` is for a vector of tables,
one of which may have left a required field out: nothing when any entry
refuses, because half a vector is not the vector the buffer claimed to
carry.

`sigil::data::values::writeStructs` lays a vector of structs down as one
block rather than as offsets, structs being inline.
`sigil::data::values::writeEach` writes every table before the vector
that points at them, because a builder lays its bytes down back to front
and a vector cannot point at what is not there yet.

Of flatbuffers the header opens the buffer's own header alone — the
builder, the verifier, the vector and the string — because a value is
read off those and written back through them. Nothing there knows a
schema, and nothing there is generated.

## See also

`sigil::data::Schema`, `sigil::data::FlatBuffer`,
`sigil::data::Connection`.
