"""Native JSON/table snapshots, domain mappings and SQL query views.

Cells are Python numbers, strings, booleans or native Instant values. Column
indexing and values() use None for missing cells; at() preserves the native
placeholder and missing() reports absence separately. All returned JSON members,
columns and tables are detached native value copies. Database views retain their
native connection and query results are detached tables.
"""

Value = float | str | bool | Instant
