# Building and hosting sketch plugins

A native plugin is an ordinary compiled consumer of the Sigil libraries.
SigilSketch supplies its registration and session adapter. The host watches
the compiled artifact and adopts a valid replacement after its candidate
session opens. A build failure, incompatible artifact or failing factory
preserves the running session.

## Build with the Sigil targets

In a CMake tree defining the existing libraries:

```cmake
sigil_sketch_plugin(my_sketch
  SOURCES MySketch.cpp Helpers.cpp
  LIBRARIES SigilComposeCore)
```

The helper consumes SigilSketch and the additional originating targets'
compile requirements. It creates a module with hidden implementation symbols,
registration metadata and the matching host identity. Framework symbols
resolve from the host; the helper does not link another set of framework
static archives into the module. Additional libraries must be present in
that host's exported link surface.

```sh
cmake --build build --config Release --target my_sketch
Sketchbook --plugin /path/to/my_sketch.dylib
Sketchbook --plugin /path/to/my_sketch.dylib --frame out.png --gpu
Sketchbook --plugin /path/to/my_sketch.dylib --headless plates --gpu
```

One module exports one entry. The host invokes no compiler in this mode.
`--frame` and `--headless` open that artifact's session. Canvas captures use
Graphite with `--gpu` and raster without it; Set scenes use the selected World
executor. A requested device or Graphite context that cannot open fails the
capture. `--bench` retains its raster frame path; `--gpu` selects the device
executor for Set and mesh work during that measurement.

On Apple, single-source and module captures of pure Canvas kinds own a Metal
device and Graphite context without starting World or Vulkan. A kind that
declares device work prepares the shared Geometry/World executor before its
availability probe and setup. Canvas bodies declare that work with
`static constexpr bool needsDevice = true;`; a supplied non-CPU painter also
requires it. CPU captures retain their CPU executors. Registry sweeps and the
interactive application retain their shared device startup path.

A module's headless capture writes `plate_<module stem>.png`, at its declared
moment and plate density unless `--at` or `--scale` overrides them. It accepts
`--promotion` and `--no-promotion`; `--kind` must match the module's runtime.
It takes one session's photographs, without the registry sweep's benchmark
phases, so `--ledger` has nothing to skip and is refused along with `--frame`
and `--bench`. `--composites` is refused for module captures.
Its filename stem is the session key, and its containing directory supplies
local assets. The adjacent `assets/` directory is the default `res://` root;
`--assets <dir>` overrides it.

Native library and plugin builds can omit the bundled applications and Python
bindings. Configure the ordinary project with its dependency toolchain and
`-DSIGIL_BUILD_APPS=OFF -DSIGIL_BUILD_PYTHON=OFF`, then build `SigilSketch`,
`SigilSketchSDK` or the desired plugin target. This path discovers no Qt or
pybind11 package. Metadata uses SigilSketch's native library requirements and
does not require Sketchbook or the bundled catalogue. Both options default
to `ON`; the bundled applications require Python bindings.

## A separate CMake build

The `SigilSketchSDK` target writes a convenience helper package for one
framework configuration at `build/sdk/Release/`. It references that
checkout and dependency build tree. The Sigil libraries remain the SDK;
this helper supplies their captured compile surface to a project that cannot
name their in-tree targets. The package declares only the Sigil libraries'
own targets, each carrying the compile requirements of everything it uses
flattened onto it; it declares no third-party target, so a project that
needs Qt, Boost or another package finds that package itself. Test support
libraries are not part of it.

```cmake
cmake_minimum_required(VERSION 3.28)
project(MySketch LANGUAGES CXX)
find_package(SigilSketchSDK CONFIG REQUIRED)
sigil_sketch_plugin(my_sketch
  SOURCES MySketch.cpp Helpers.cpp
  LIBRARIES SigilComposeCore)
```

```sh
cmake -S ~/sketches/my_sketch -B ~/sketches/my_sketch/build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DSigilSketchSDK_DIR=/path/to/framework/build/sdk/Release
cmake --build ~/sketches/my_sketch/build
Sketchbook --plugin ~/sketches/my_sketch/build/my_sketch.dylib
```

The helper supplies include directories, definitions and compiler options.
It requires the matching compiler version, target architecture and
configuration; a multi-config build uses `--config Release`. It refuses
changed public headers or dependency binaries from the originating libraries
the plugin consumes until their metadata is regenerated.
Keep the supplied ABI settings intact. A complete example is in
`cmake/test/plugin/`.

## Compatibility and publication

The entry returns C++ framework types, so plugins must match the running
host's headers and build context. This is a matching-build C++ contract.
The helper is not a relocatable installation or an independently versioned
binary ABI.

The build emits two files: the module and `<module>.sigil-build`. Deploy both.
The sidecar binds the module bytes to its compiled native runtime and
originating-library identities. The host copies
each changed generation to a unique runtime path and verifies those bytes
and identity before loading it. It then checks the exported ABI and identity
before invoking the C++ entry and factory.

Publish a completed module by atomic rename, then publish its matching
sidecar. During an incomplete or inconsistent publication, the previous
session keeps running. These checks assume trusted native code.

A build may also write the module in place and stamp the sidecar
afterwards, as the helper does. Until it finishes, the host sees new bytes
beside a sidecar that still names this host's build. It treats that as a
publication in progress: no status change and no failure, and it looks
again on the next poll. The disagreement is reported only when the same
module and sidecar are seen on consecutive polls spanning at least
`Host::Options::pluginPublicationGrace` (two seconds by default; zero
reports it on the second such poll). A sidecar naming another build is
reported at once. A refused generation is not counted: the host's
generation numbers only the modules it adopted.

Each originating target's fingerprint covers its transitive public target
closure, compiler-discovered public header dependencies, imported dependency
binaries and compiler/configuration requirements. Indirect public includes
are covered even when absent from the target's header list. Conditional
dependencies are evaluated for the selected configuration. Documentation and
unrelated build declarations do not participate.

The runtime checks SigilSketch's public C++ boundary and each additional
origin named in `LIBRARIES`. An incompatible consumed boundary requires
rebuilding the host and plugins and restarting the host. An unconsumed
library can change independently. The convenience package publishes the
configured Sigil libraries' targets with their flattened compile requirements
and validates only the plugin's consumed boundaries. A custom host still must supply the libraries it uses.

Deleting a sidecar and rebuilding an unchanged target restores the identity
embedded in that artifact. Recovery cannot assign current metadata to an
older module; a newly linked module receives its own compiled identity.

Accepted modules and modules whose callbacks throw remain mapped until
process exit. Framework values and callbacks can outlive their originating
host, so destroying a host does not unload their code. Each adopted
generation retains a mapped image; restarting the process reclaims them.

## Source compilation as a convenience

Sketchbook can also own compilation for a C++ source path:

```sh
Sketchbook ~/sketches/my_experiment.cpp
python3 scripts/sigil.py workspace new ~/sketches/aurora_drift
```

The workspace command creates an entry source, `assets/`, `captures/` and a
README. A workspace is an ordinary directory; native plugins instead use
their own CMake project.

A single entry builds on its own. For a source entry whose directory shares
its stem, every other `.cpp` directly beside it is a unit of that sketch.
Quoted includes reach local headers. The watcher checks the entry every poll
and scans sibling units and literal quoted includes on its configured cadence.
The compiler resolves all includes when preprocessing a cache lookup.

Source mode uses the host build's captured compiler flags. Changed units
compile separately and link into one module; unchanged matching units can
reuse cached objects. Successful builds are cached across sessions and
application restarts. The cache key includes preprocessed contents, compiler
settings and the running host image. Failed builds never populate it.
The platform cache contains `SigilSketch/builds`; `--state <dir>` places
builds under that state root.

Temporary objects and module copies belong to the process's runtime directory.
Each host and generation uses a distinct name. The directory is removed
when the last host is destroyed and on normal exit; an abandoned directory
is removed only when its process is no longer alive. Removing those files
does not unmap already loaded code.

For a single source, `assets/` stands beside the entry. A directory source
sketch shares the `assets/` root above its own directory.
`ctx.local("data/x.csv")` always names a file beside the entry.
`--assets` chooses another resource root.

Bundled entries open their compiled-in bodies before any source edit.
Sketchbook keeps the last three selected sessions resident; returning to one
reuses it, and a rebuild starts that session afresh. A custom host chooses
its own residency policy. C++ and Python source workspace paths use `--frame`
and `--bench`; their `--headless` plate sweep walks the compiled-in registry.

After rebuilding the framework, restart a source host. The source compiler
also refuses framework headers newer than the pinned running-image stamp.

## A custom host

Set `Host::Options::pluginPath` for artifact mode. Compiler and flags fields
are unused. Call `Host::poll` to adopt replacements and resource edits, then
`Host::frame` to advance and draw. The host owns its candidate sessions and
asset mounts; the caller owns the font context, drawing surface and device
runtime that those sessions use.

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

A custom executable must export the framework symbols its plugins call.
Hidden plugin definitions keep each generation's factories, vtables and
inline state in its own image; undefined framework references resolve from
the host. The registration exports remain visible.

The build derives the source convenience compile surface from `sketches/Anchor.cpp`,
which belongs to `SigilSketches`. `SketchLinkSurface.cmake` walks the
framework archives in the scene and host target closures, including private
dependencies, and force-loads them into Sketchbook. This keeps symbol export
requirements with the originating targets. The reload tests exercise symbols
that compiled-in scenes might otherwise omit from the executable.
