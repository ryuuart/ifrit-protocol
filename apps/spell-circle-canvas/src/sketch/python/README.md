# Python sketch adapter

`SigilSketchPython` connects the reusable bindings in `SigilPython` to the
native sketch runtime. Its public C++ interface is under
`sigilsketch/python/`; the importable Python package, stubs and authoring
instructions belong to `apps/python/sigil`.

## Layout

ONE DIRECTORY PER SUBJECT GROUP, ONE FILE PER SUBJECT. `host/` is what a
sketch is handed — the context, the scene3d sketches and what a Python entry declares
— and `kit/` the specimen values it dresses with. Each directory carries
one registration header naming every bind function the files beside it
define, and one source file per subject that binds something, all of
them named in the CMake list. A submodule the package surfaces before a
native value lives in it is created by the registration it belongs
with: `sketch.scry` by the catalog's. A new subject adds its source to
that list, its call to `Module.cpp` and its declaration to the
registration header, so two subjects are never written in one file.

`Module.cpp` assembles one `_sigil` module from the common registration
followed by these sketch features, and names every registered value for
the package an author imports it from; both the embedded application and
the installed extension use that assembly.

* `host/Registration.h` — `bindRuntime`, `bindSketchCatalog`,
  `bindSketchContextSurface`, `bindSketchPage`, `bindSketchPlates`,
  `bindSketchScene3DSketches`
* `kit/Registration.h` — `bindSketchKit`, `bindSketchKitCharts`,
  `bindSketchKitContent`, `bindSketchKitControls`, `bindSketchKitLegends`,
  `bindSketchKitPanels`,
  `bindSketchKitStreams`, `bindSketchKitVerdict`, `stageContext`

`host/Runtime.cpp` owns interpreter selection for embedded hosts, fresh
source imports, sketch instances, checked context and service views,
callback and feed leases, file rendering through the native host, and a
session's evaluate: the text a client sends handed, with the running
instance and its context, to the package's `sigil._evaluation`, which runs
it in the sketch's module and catches what it raises, and the body declared
again where its class says a setup declares its tree.
`host/Plates.cpp` binds the comparison of two directories of plates as the
rows a script judges, and `host/Catalog.cpp` the registry and a catalog of
sketch files as the rows a browser shows. `host/Page.cpp` binds the page a
frame lights, which `Module.cpp` registers before the contexts so their
`page` answers the class an author imports. `host/Scene3DSketches.cpp` binds
the `Scene3DContext` a scene3d sketch's setup is handed; the
`compose.TextureScene` that context hands out is the compose bindings' own,
registered with the libraries ahead of every sketch feature. `host/Runtime.cpp` opens a
class the decorator marked scene3d through `openScene3D`, and reads the
kind of a source it has not loaded from its text: a `describe`, method or
top-level function, whose return annotation names `Frame`. `kit/Specimen.cpp` binds sketch specimen values and
typed theme providers; `kit/Charts.cpp` the plot frame and its layers,
calling a curve written in Python as the plot is laid out and a row reader
once as its layer is made; `kit/Legends.cpp` the key, the swatch strip,
the chip, the meter and the dial; `kit/Panels.cpp` the headings, the
title card, the backdrop, panel and frame, the scrollbar, the ticker and
the timeline; `kit/Content.cpp` the instrument; and `kit/Streams.cpp` the
console over text rings and the readout of a connection's state.

## Registration

`cmake/register_sketches.py` writes the registry entries for the Python
sketches a host bundles, reading each source as a syntax tree and never
running it: a file joins only when it declares a `@sketch` class or
defines `setup`, `draw` or `describe` as functions at its top level. The key
is the file's stem. A literal `NAME` and `CATEGORY` string in the module
give the name the entry is filed under and its category, defaulting to
the stem and `Python`; `REQUIRES` names the modules its availability
probe asks for, and the module docstring's first paragraph is its blurb.
A declaration that is not a literal string stops the build naming the
file.

## Ownership

The core `SigilSketch` target does not link Python. Hosts opt into Python loading
through their loader function. This adapter links the common binding library;
the common library has no reverse sketch dependency. Closing a session
invalidates its context before releasing callbacks and resources. A candidate
that fails to open cannot release the working session's shared feed leases.

The native integration cases remain in `test/PythonTest.cpp` and join
`sketch_test`. Common binding tests live with `SigilPython`; Python authoring,
packaging and typing tests live with the Python package.
