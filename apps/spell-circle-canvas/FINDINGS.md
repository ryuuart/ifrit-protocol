# Findings

A work queue: each entry states what the code does, what it was evidently
intended to do, and what a test should assert once intent is restored.
Entries are deleted as they are fixed, and the file is deleted when it is
empty.

## Opening an absent SQLite file creates it despite the absent-file contract

**What the code does.** `data::Database::open` delegates SQLite file opening
with `SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE`, so a missing `.sqlite`,
`.sqlite3` or `.db` path creates a new file and returns a database.

**What it was evidently intended to do.** The public opening contract says
an absent resource returns `nullopt`. Explicit memory construction and database
writing are separate API operations. Python bindings currently preserve the
native opening behavior; asset lookup still rejects an absent resource.

**What a test should assert.** Opening a missing database path returns
`nullopt`, reports absence and creates no file. Opening an existing store
continues to support its documented query and writing operations.

## Python authoring loses static type information at its public boundary

**What the code does.** The public Python builders and sketch decorator have
unannotated signatures. The extension supplies partial runtime signatures,
but conversion boundaries expose broad objects, dictionaries and callbacks.
The package contains no type stubs or typing marker, and the installation
rule copies Python source files only. Editors and static checkers cannot
reliably recover element properties, callback contracts or native return
types from this distribution.

**What it was evidently intended to do.** Python authors should retain type
information for the native objects they construct, including completion and
static checking of properties, material values, sketch callbacks and their
return types. Flexible inputs should be expressed with unions, overloads,
protocols and generic model types.

**What a test should assert.** An isolated installed wheel provides the
native and authoring type declarations. A static-checking fixture infers
native element and paint return types, preserves a decorated sketch class,
accepts supported input forms, and rejects misspelled properties and invalid
argument or callback types. A binding-surface check detects stale declarations
when the exposed native API changes.
