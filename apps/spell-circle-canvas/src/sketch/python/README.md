# Python sketch adapter

`SigilSketchPython` connects the reusable bindings in `SigilPython` to the
native sketch runtime. Its public C++ interface is under
`sigilsketch/python/`; the importable Python package, stubs and authoring
instructions belong to `apps/python/sigil`.

## Layout

ONE DIRECTORY PER SUBJECT GROUP, ONE FILE PER SUBJECT. `host/` is what a
sketch is handed — the context, the sets and what a Python entry declares
— and `kit/` the specimen values it dresses with. Each directory carries
one registration header naming every bind function the files beside it
define, and one source file per subject, all of them named in the CMake
list. An author fills in a file that already exists; they do not add a
source to that list, a call to `Module.cpp` or a declaration to a
registration header, so two subjects are never written in one file.

`Module.cpp` assembles one `_sigil` module from the common registration
followed by these sketch features, and names every registered value for
the package an author imports it from; both the embedded application and
the installed extension use that assembly.

* `host/Registration.h` — `bindRuntime`, `bindSketchContextSurface`,
  `bindSketchDeviceRuntimes`, `bindSketchEntries`, `bindSketchSchema`,
  `bindSketchSetSketches`, `bindSketchWorldSet`
* `kit/Registration.h` — `bindSketchKit`, `bindSketchKitCharts`,
  `bindSketchKitContent`, `bindSketchKitLegends`, `bindSketchKitPanels`,
  `bindSketchKitRows`, `bindSketchKitStreams`, `stageContext`

`host/Runtime.cpp` owns interpreter selection for embedded hosts, fresh
source imports, sketch instances, checked context and service views,
callback and feed leases, and file rendering through the native host.
`kit/Specimen.cpp` binds sketch specimen values and typed theme
providers.

## Ownership

The core `SigilSketch` target does not link Python. Hosts opt into Python loading
through their loader function. This adapter links the common binding library;
the common library has no reverse sketch dependency. Closing a session
invalidates its context before releasing callbacks and resources. A candidate
that fails to open cannot release the working session's shared feed leases.

The native integration cases remain in `test/PythonTest.cpp` and join
`sketch_test`. Common binding tests live with `SigilPython`; Python authoring,
packaging and typing tests live with the Python package.
