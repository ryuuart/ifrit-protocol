# Python sketch adapter

`SigilSketchPython` connects the reusable bindings in `SigilPython` to the
native sketch runtime. Its public C++ interface is under
`sigilsketch/python/`; the importable Python package, stubs and authoring
instructions belong to `apps/python/sigil`.

`Python.cpp` owns interpreter selection for embedded hosts, fresh source
imports, sketch instances, checked context and service views, callback and
feed leases, and file rendering through the native host. `KitBindings.cpp`
binds sketch specimen values and typed theme providers. `Module.cpp` assembles
one `_sigil` module from common registration followed by these sketch features;
both the embedded application and installed extension use that assembly.

The core `SigilSketch` target does not link Python. Hosts opt into Python loading
through their loader function. This adapter links the common binding library;
the common library has no reverse sketch dependency. Closing a session
invalidates its context before releasing callbacks and resources. A candidate
that fails to open cannot release the working session's shared feed leases.

The native integration cases remain in `test/PythonTest.cpp` and join
`sketch_test`. Common binding tests live with `SigilPython`; Python authoring,
packaging and typing tests live with the Python package.
