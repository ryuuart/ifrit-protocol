"""The documentation guard's generator, one module per subject.

`names` holds the patterns a documented name is found by and the tables
of names no probe form can reach; `documents` reads what a markdown file
carries; `headers` reads what the library's headers declare; `generate`
turns the two into a translation unit of probes and a coverage report;
`selftest` pins the generator's own behaviour against small fixtures.

Each module is imported by name. Nothing is re-exported here, so a
reader of a probe run can follow a name to the module that owns it.
"""
