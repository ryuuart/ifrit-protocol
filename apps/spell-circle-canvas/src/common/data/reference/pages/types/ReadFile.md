---
kind: function
library: SigilData
name: json
qualified: sigil::data::json
group: Reading a file
status: stable
---

# json, csv, table

## Description

A FILE, READ WHOLE, in one line over a hub. `sigil::data::json` answers
the JSON document at a URI — or the OSC packet a name beginning
`osc://` or ending `.osc` holds — and null where it is missing or is no
document. `sigil::data::csv` answers the table in a delimiter-separated
file, the delimiter and the header sniffed, a `.json` rectangle reading
the same way. `sigil::data::table` answers a table from a store, a CSV
or a JSON rectangle, whole or through `TableOptions::query`:

```cpp
const Json words = data::json(hub, "res://content.json");
const Table sheet = data::csv(hub, "res://deaths.csv");
const Table rows = data::table(hub, "res://cities.sqlite",
                               {.query = "SELECT city FROM cities", .why = &why});
const Table coastal = data::table(hub, "res://cities.csv",
                                  {.query = "SELECT city FROM source WHERE coastal"});
```

A query over a store — `.sqlite`, `.sqlite3`, `.db`, `.duckdb` — runs in
that store, opened in place for reading where the hub resolves it to a
file and otherwise from the bytes the hub reads, which a SQLite store
opens from and a DuckDB one does not. A query over a CSV or a JSON
rectangle runs in DuckDB, in memory, with the file's rows written in as
the table `source`. A store asked for no query answers empty, a store
being many tables.

Each asks the hub for the bytes, so a mounted `res://` URI, a network
URL and a plain path all read; nothing is cached here beyond the bytes
the hub caches. `ReadOptions::why` and `TableOptions::why` say what went
wrong where the answer is null or empty.

`sigil::data::registerDecoders` is the cached path for a host that
reloads what changed: it puts the `Table`, `Json` and `Database`
decoders on a hub once, and `hub.load<T>(uri)` answers from then on.

## See also

`sigil::data::Json`, `sigil::data::Table`, `sigil::data::Database`.
