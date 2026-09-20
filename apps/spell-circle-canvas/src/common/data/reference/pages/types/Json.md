---
kind: type
library: SigilData
name: Json
qualified: sigil::data::Json
group: Reading a file
status: stable
---

# Json

## Description

A JSON DOCUMENT AS A VALUE, and the rectangle inside one. `Json` is the
whole document: nested, ordered, comparable, copyable — what a nested
record is read as when it is not a table at all. A document that IS
rectangular becomes a `sigil::data::Table` instead, in any of the three
shapes data is published in.

The parser is somebody else's and stays behind the header: nothing there
exposes it, so a consumer compiles against the standard library.

### An object keeps the author's order

Because that order is the author's and a reader who prints the document
back should not reorder it. Lookup by key is therefore a scan, which is
what a record of a few fields wants. A document that writes one key
twice keeps both members, and a lookup answers the first, since dropping
one would be an edit to somebody else's document.

```cpp
const Json doc = *decodeJson(text);
for (const Json& node : doc["nodes"].items())
  place(node["x"].number(), node["y"].number(), node["name"].text());
```

`Json::boolean`, `Json::number` and `Json::text` answer the value, or
the fallback when this is something else. Reading the wrong kind is not
an error: a document is somebody else's and a reader that asked for a
number where a string stands wants its own default, not a throw. The
`Json::operator[]` that takes a key answers a reference to a shared
null, so a chain of lookups through members that are not there answers
null instead of crashing.

Two constructors exist for a reason a caller would not guess. A whole
number is a number: without that overload `Json(1)` is ambiguous, an int
converting to bool and to double at the same rank. And a literal is
text: without that overload a `const char*` would pick the boolean
constructor and a name would become `true`.

### Reading and writing the document

`sigil::data::decodeJson` answers the document in text, or nothing when
it is not JSON. A lone number, string, boolean or null is a document,
and so answers a value that is not a list or a record; it holds no
rectangle, so `sigil::data::tableFromJson` answers nothing for it. Text
that is not valid UTF-8, and text nested deeper than the parser reads,
are not documents at all: nothing comes back rather than the part that
parsed.

`sigil::data::encodeJson` writes the value as JSON text, which
`sigil::data::decodeJson` reads back as the same value. Compact: nothing
stands between a member and the next, because what it writes goes on a
wire or into a file rather than in front of an eye. A record keeps the
order its members are in. Text is written as it stands, escaping only
what JSON cannot hold raw, so text that arrived as UTF-8 leaves as the
same UTF-8. A number is written with the fewest digits that read back as
itself, so a whole number is written whole; a number that is not finite
has no JSON spelling and is written null, which is the value a reader
would get back for it.

### The rectangle inside a document

`sigil::data::tableFromJson` answers it in whichever of the three shapes
it is published in, or nothing when the document holds no rectangle:

- a LIST OF RECORDS — one row each, columns being the union of their
  keys in first-appearance order, a record missing a key giving a
  missing cell;
- a RECORD OF LISTS — one column each, named by its key, a key written
  twice keeping both columns with the later one numbered by its
  occurrence (`name`, `name_2`);
- a LIST OF LISTS — one row each, columns named by their 1-based
  position.

A column's type is the one every present value in it shares: numbers
make a number column, booleans a boolean column, strings that are all
instants a time column, and anything else a text column. A cell holding
a list or a record of its own has no place in a rectangle and is
missing; read that document as a `Json` instead.

## See also

`sigil::data::Table`, `sigil::data::decodeCsv`.
