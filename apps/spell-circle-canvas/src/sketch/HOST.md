# Hosts: how a sketch reaches one

Sigil is a suite of libraries — Compose, World, Draw, Motion, IO and the
rest — each used directly through its own headers and target. SigilSketch
is the framework a scene is hosted through: registration (`SIGIL_SKETCH`
and `sketch::Entry`), sessions (`sketch::Kind` opens a `sketch::Session`),
the canvas and set runtimes those sessions draw through, the live host
(`sketch::Host`) that compiles, loads and swaps sketches, and the
workspace convention for sketches kept outside this repository.
Sketchbook is one host built on that framework: the stock one, and an
example of using it. It lists and runs the sketches it is pointed at —
its compiled-in catalogue, a file, or a workspace — and a custom host
links SigilSketch and drives `sketch::Host` in the same way.

## Three ways a sketch reaches a host

A sketch reaches a host in exactly three ways.

1. **Compiled into the host.** A source the host's own build compiles
   registers itself with `SIGIL_SKETCH`; Sketchbook's build compiles every
   entry under `sketches/`. Such a sketch opens at once, with no compiler.
   Editing its file afterwards rebuilds it as the second way does.
2. **A C++ source file anywhere on disk.** The host compiles it against
   itself, caches the build, and swaps each successful build into the
   running session.
3. **A Python file.** The host imports a module declaring a `@sketch`
   class through its Python importer (`Host::Options::pythonLoader`), and
   imports it again on save. No C++ is compiled.

There is no fourth way: a host never loads a library compiled anywhere
but by itself.

## Why the host compiles a C++ sketch

The boundary between a sketch and the libraries is their whole C++
vocabulary. A sketch constructs the libraries' objects and the host
mutates them; an entry hands back a `sketch::Kind` whose session the host
drives; inline functions, templates and struct layouts from every header
the sketch includes cross that line. Code compiled apart from the host
agrees with it only where every header and every flag agrees, and the
only build that guarantees that is the host's own.

So a C++ sketch outside the host is always compiled BY the host, with the
compile line the host's build captured and against the headers that build
read. Sketchbook's build lifts that line out of the compilation database
from `sketches/Anchor.cpp`, a unit that includes what a sketch includes,
into `sketch_flags.rsp` beside the executable (`Host::Options::flagsFile`).
The sketch's library links with `-undefined dynamic_lookup` and resolves
every framework symbol out of the running executable.

The host refuses to compile when its headers are newer than itself. Before
each build it compares every framework header on the flags file's include
paths with the running image's stamp, read once when the process first
asks (`sketch::hostBinaryTime`, the default of `Host::Options::hostStamp`).
A sketch compiled against a newer header would load into a host whose
structs have the old layout, so the build is refused with "framework
headers are newer than this host", and the previous session keeps running
until the host is rebuilt and restarted. The registration also exports the
framework's `sketch::kAbiVersion`; a library whose version differs from
the host's is refused at load.

## Workspaces

A workspace is how sketches live outside this repository: an ordinary
folder of C++ and Python sketches and the files they stand on.

```sh
python3 scripts/sigil.py workspace new ~/sketches/aurora_drift
Sketchbook --workspace ~/sketches/aurora_drift
Sketchbook ~/sketches/aurora_drift/aurora_drift.cpp
```

`workspace new` writes an entry named for the folder, an `assets/`
directory, `captures/` for what the window's Capture writes, and a README
stating the contract. `Sketchbook --workspace <dir>` lists the
folder's C++ and Python sketches in their own Workspace collection; a path
opens one file directly. [RUNNING.md](RUNNING.md) covers discovery and the
commands a file opened by path can be put through.

A workspace is bound to one host build. Its C++ sketches compile with that
build's `sketch_flags.rsp`, against the headers of the checkout that build
came from, and are refused once those headers are newer than the running
host; after rebuilding the libraries, restart the host before opening the
workspace again. Cached builds are keyed by the running host image, so a
build made for one host is never handed to another.

A workspace brings sources, headers and data, and nothing compiled. Every
framework symbol a sketch calls resolves out of the host, so a workspace
reaches only the libraries its host already contains: those the host's
build force-loaded into its executable. A sketch that needs a library the
host does not contain needs a host that contains it — a library added to
this tree and linked into Sketchbook, or a custom host whose own build
carries it. A Python sketch's own packages come from the `pyproject.toml`
beside it; Sigil itself always comes from the host.

## Source builds

A single entry builds on its own. For a source entry whose directory shares
its stem, every other `.cpp` directly beside it is a unit of that sketch.
Quoted includes reach local headers. The watcher checks the entry every poll
and scans sibling units and literal quoted includes on its configured cadence
(`Host::Options::siblingScanInterval`). The compiler resolves all includes
when preprocessing a cache lookup.

Changed units compile separately and link into one library; unchanged
matching units can reuse cached objects. Successful builds are cached
across sessions and application restarts. The cache key includes the
preprocessed contents, the compiler and its flags, and the running host
image. Failed builds never populate it. The platform cache contains
`SigilSketch/builds`; `--state <dir>` places builds under that state root.

Objects and libraries belong to the process's build directory
(`Host::buildDirectory`). Each host and build uses a distinct name, and a
refused build's library stays there with the rest. The directory is
removed when the last host is destroyed and on normal exit; an abandoned
directory is removed only when its process is no longer alive
(`Host::sweepAbandonedBuildDirectories`). Removing those files does not
unmap already loaded code.

A build that fails to compile, a library that does not load, a factory
that throws and a setup that fails all keep the running session. A frame
that throws fails the session until an edit or `Host::restartSession`
opens it again. Whatever a sketch throws from its entry, factory, setup,
declarations, frames or stills — a standard exception or anything else —
is reported in `Host::errorLog` and does not escape the host.

For a single source, `assets/` stands beside the entry. A directory source
sketch shares the `assets/` root above its own directory.
`ctx.local("data/x.csv")` always names a file beside the entry.
`--assets` chooses another resource root.

Bundled entries open their compiled-in bodies before any source edit.
Sketchbook keeps the last three selected sessions resident; returning to one
reuses it, and a rebuild starts that session afresh. A custom host chooses
its own residency policy. A file opened by path is photographed with
`--frame` and measured with `--bench`; `--headless` walks the compiled-in
registry.

## Captures on the device

`--frame` with `--gpu` draws a Canvas still through Graphite and a Set
scene through the World device executor; a requested device or Graphite
context that cannot open fails the capture. On Apple, a capture of a pure
Canvas kind owns a Metal device and Graphite context without starting
World or Vulkan. A kind that declares device work prepares the shared
Geometry and World executor before its availability probe and setup.
Canvas bodies declare that work with `static constexpr bool needsDevice =
true;`; a supplied non-CPU painter also requires it. `--bench` keeps its
raster frame path; `--gpu` selects the device executor for Set and mesh
work during that measurement.

## Mapped-code lifetime

Accepted libraries are never closed. Framework values and callbacks can
outlive the host that loaded them — a vtable, a string literal, a function
pointer retained elsewhere — so destroying a host does not unload their
code. A library whose callback throws while it is being adopted stays
mapped too, because the exception's message, destructor and type
information may live in it. Only a library refused before it handed the
host anything — one missing the registration's exports, or one whose ABI
query answers another `sketch::kAbiVersion` — is closed at once. Each adopted generation retains
its image until the process ends.

## A custom host

A custom host links SigilSketch and owns a `sketch::Host` per open sketch.
`Host::Options::sketchPath` names the entry, `Host::Options::flagsFile`
the captured compile line and `Host::Options::compiler` the command that
compiles and links; `Host::Options::compiledIn` starts from a sketch the
executable carries. Call `Host::poll` to adopt builds and resource edits,
then `Host::frame` to advance and draw. The host owns its candidate
sessions and asset mounts; the caller owns the font context, drawing
surface and device runtime that those sessions use.

`Host::frame(double)` advances an unobserved capture step through
`Session::discardedFrame`; `Host::prepareCapture` uses this path for its
pre-roll. Drawing runs by default. A Canvas declaration with
`CanvasSpecification::paintDiscardedFrames` false omits painting while the
clock, feeds and body updates still advance. It requires capture state that
does not depend on draw callbacks. Frames supplied a canvas and captured
stills continue to draw.

`Host::Options::prepareSession` can prepare those resources for each candidate
Kind before availability and setup, including reload and restart. Throwing
rejects the candidate and keeps the accepted kind, probe and session together.
Preparation must retain resources used by the running session and must not
switch its capture backend. `Host::needsDevice` reports the accepted kind's
requirement, so a capture host can choose its backend after a successful load.

`Host::restartSession` reopens the accepted kind without compiling.
`Host::status` reports progress, and `Host::errorLog` reports compiler,
loader, setup and resource failures. Resources with problems may continue
rendering their library's fallback; the host replaces only a successfully
opened session. [RUNNING.md](RUNNING.md) defines the application commands.

A custom executable must export the framework symbols its sketches call,
and must contain every library they may reach. Sketchbook's build does
both through `cmake/SketchLinkSurface.cmake`, which walks the framework
archives in the sketch and host target closures, including private
dependencies, force-loads them into the executable and re-exports them;
symbol export requirements stay with the originating targets. A custom
host captures its own flags file the same way, by running
`cmake/SketchFlags.py` over the compile line of a unit that includes what
its sketches may include. The host compiles every sketch with hidden
visibility, so each generation's factories, vtables and inline state stay
in its own image; undefined framework references resolve from the host,
and the registration exports remain visible. The reload tests exercise
symbols that compiled-in scenes might otherwise omit from the executable.

A host that carries no applications or Python bindings configures the
project with `-DSIGIL_BUILD_APPS=OFF -DSIGIL_BUILD_PYTHON=OFF` and builds
`SigilSketch`; that path discovers no Qt or pybind11 package. Both options
default to `ON`, and the bundled applications require the Python bindings.
