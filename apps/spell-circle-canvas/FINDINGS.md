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
