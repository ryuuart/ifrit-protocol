---
kind: type
library: SigilData
name: Database
qualified: sigil::data::Database
group: The query
status: stable
---

# Database

## Description

A SQL engine a drawing asks its data of, behind one seam: SQLite for a
file that will be read for decades, DuckDB for the analytics a plot
wants over a large table. A query answers a `sigil::data::Table`, the
same value a CSV decodes to, so a sketch that shapes its data in SQL and
one that shapes it with `Table::filter` draw from the same thing.

### Which engine stands behind one

`sigil::data::Engine` names it. Both answer the same calls; the
difference is what each is for. SQLite is the file format that stays
readable, one table at a time, and the engine a small store beside a
sketch is written in. DuckDB is the columnar engine that aggregates a
million rows in the time SQLite scans them, reads a CSV or a Parquet
file straight from a query, and holds its store in memory or in a file
of its own.

A file is opened by its extension — `.sqlite`, `.sqlite3` and `.db` are
SQLite's, `.duckdb` is DuckDB's — and `Database::memory` opens an empty
store of either engine that lives as long as the value does. The value
is move-only: one connection, owned. `sigil::data::engineOf` answers the
engine a file's extension names.

`Database::fromBytes` opens the bytes of a SQLite file held in memory —
the form a resource hub hands a decoder — as a read-only store. DuckDB
opens files only, so bytes of a `.duckdb` answer nothing.

### Whether it accepts writes

`sigil::data::Access` says so, and `Database::access` answers which a
store was opened with. `Database::open` with a path alone opens
`Access::ReadWrite`; the overload taking an access opens the other way,
and `Access::ReadOnly` is a connection the ENGINE refuses writes
through, so the file behind it is safe from every holder of the value
rather than from the polite ones.

`Database::writes` answers whether running a text would write, which is
the message a caller shows before `Access::ReadOnly` refuses the
statement outright. Every statement in the text is examined and not only
the first, because a holder that refuses writes has to refuse a writing
statement wherever it stands. A transaction's own verbs, and a reading
statement that calls a writing function, answer false; a statement the
engine cannot parse answers nothing at all and names the error.

### Asking it

`Database::query` answers a table: every column of the result typed by
what its cells hold — an integer or a real is a number, text is text, a
boolean is a flag, a date or a timestamp is an instant, SQLite's
declared DATE and DATETIME and ISO text included — with a NULL cell
marked missing, so a drawing walks it as it walks a decoded CSV.

`Database::execute` runs a statement for its effect.

`Database::insert` writes a table in as a named table, typed from the
columns — a number is a DOUBLE, text is TEXT, a flag is a BOOLEAN, an
instant is a TIMESTAMP — and writes every row, a missing cell as NULL. A
table of that name already there is replaced. This is how a CSV a sketch
already decoded becomes something a query can join, and how a store
beside a sketch is built in the first place.

### On a hub

`sigil::data::DatabaseDecoder` decodes a database file for a resource
hub: `hub.load<Database>(uri)`. A resource that is a file on disk is
opened in place, by its path, so both engines answer and the store is
read as the engine reads it; a resource that is bytes alone — a network
cache with no file, a byte source — is a SQLite store deserialised from
them, and a `.duckdb` from bytes alone is refused. Either way the store
is `Access::ReadOnly`: a hub hands one cached resource to every reader,
so no reader may change the file the others are reading.

## See also

`sigil::data::Table`, `sigil::data::decodeCsv`.
