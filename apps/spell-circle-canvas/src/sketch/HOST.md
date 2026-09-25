# The live host

Saving a sketch recompiles it into a small dylib and hot-swaps it into
the running canvas. This is the pattern C++ live coding converged on — a
thin host executable plus a recompiled guest library, rather than
embedding a scripting language — so a sketch never leaves the real API.

* The host executable exports the framework's symbols, so a sketch dylib
  links with `-undefined dynamic_lookup` and builds in a couple of
  seconds: a few small translation units, nothing linked against the
  static libraries. The units — the entry, the sources beside it when
  the sketch is a directory — compile side by side
  into cached objects and link once; a unit is compiled again only when
  its preprocessed contents, compiler settings, or native host build differ.
  The live watcher follows literal quoted local includes recursively;
  the cache lookup resolves all includes through the compiler.
* **The guest compiles hidden**, with `-fvisibility=hidden
  -fvisibility-inlines-hidden` on top of the flags the build captured,
  and that is what makes the file on disk the thing that runs. A sketch
  reaches its host through weak definitions — the vtable and typeinfo
  of the body the registration instantiates for it, `kindOf<T>` and the
  other function templates the macro takes the address of — and
  weak definitions COALESCE. Every image exporting one names the same
  symbol, and the dynamic loader binds them all to whichever came first. The
  executable is always first and carries its own copy of every sketch in
  the registry, so a guest at default visibility would hand back an entry
  whose factory is the host's: the build reports, the dlopen succeeds,
  and the picture is of the file as it stood when the host was built.
  The same rule runs the other way between two builds of one guest,
  since old libraries are never unloaded — build 1 would beat build 2 and
  an edit would never appear, for a sketch outside the registry too.
  Hidden visibility closes both directions at once, because a definition
  that is private to its image joins no coalescing set in either. What
  hidden does NOT touch is an UNDEFINED reference, so the framework still
  resolves out of the host exactly as before; the two entry points the
  registration macro exports carry `visibility("default")` explicitly, so
  `dlsym` finds them. The cost is that a guest gets its own copy of every
  inline the host also has, which is right for code and would be wrong
  only for a mutable static inside one. Typeinfo equality is the other
  thing it touches: the platform compares two copies it marked private
  by name, but a private copy against the one the host shares by
  address, so a library type's identity in a guest differs from the
  host's. A fact on an element is the place that crosses, and
  `compose::Attributes` matches a fact's type by its spelled name for
  that reason. `--frame` on a
  registry sketch with one colour changed is the whole of the proof, and
  the `sketch_reload_runs_the_file` test is exactly that.
* **The build directory belongs to the run that made it.** The objects
  and one dylib per build stand in `<temp>/sigil_sketch_<pid>`, shared by
  every host in the process, and it is removed when the last of them is
  destroyed and again on normal exit — a `--frame` or `--bench` run,
  which ends right after its build, takes its own with it, and a
  `--headless` sweep walks the compiled-in registry, hosts nothing and
  makes none. Removing it disturbs nothing: no dylib is ever dlclosed,
  and an unlinked file that is mapped stays readable until the last
  mapping goes. Reusable builds live separately in the persistent cache.
  A run that was
  killed or that faulted never reached that removal, so before a host
  makes its own directory it removes the sibling ones whose pid no
  process holds; a live pid's directory is never touched, this process's
  own least of all.
* **Successful C++ builds survive session eviction and application restarts.**
  The persistent cache lives in `~/Library/Caches/SigilSketch/builds` on macOS,
  or `SigilSketch/builds` under the XDG cache directory on other platforms.
  A process that names a state root keeps them in `builds/` under it.
  The compiler preprocesses each unit to validate all resolved includes and
  macros. Compiler version, flags, and the running native image's path and
  pinned build timestamp also participate in the key. A matching artifact
  is copied into a unique runtime path before loading, so sessions do not
  share sketch-owned static state merely because they share a cached build.
  Missing artifacts compile normally; failed builds do not populate the cache.
  Unchanged C++ source still needs preprocessing on a cache lookup, but no
  code generation or link. Bundled examples open their compiled-in bodies.
* **A build is named for the host that made it** —
  `sketch_<host>_<build>.dylib` — because every host in a process links
  into that one directory. Named by
  its build number alone, the three resident hosts would all write
  `sketch_1.dylib`: two of them building at once race for the path, and
  the file standing there when one of them dlopens is whichever link
  finished last, so a host adopts a sketch it did not build. The image
  already loaded is safe either way — the linker replaces its output
  rather than rewriting it, so the inode a mapped dylib is reading stays
  alive under it — and it is the gap between a link and the dlopen after
  it that an id per host closes.
* Compile errors overlay while the **last good sketch keeps running**.
* Old libraries are never unloaded. Their statics stay valid — a running
  session may hold a vtable or a string literal that lives in one — and
  one small leak per reload is the trade.
* A sketch this binary already carries opens **instantly**, and the file
  is watched from where it stands: an edit builds, an unedited file
  never does.
* The last **three** sketches opened stay resident. Selecting one swaps
  which of them the window presents rather than building it again, so
  setup runs once per sketch instead of once per visit and the rolling
  frame windows behind the readout survive a look at something else — a
  sketch you come back to shows its own numbers, not a ring filling from
  zero. What leaves is the one presented longest ago. An EDIT is not a
  switch: a rebuild restarts its own session from nothing, which is
  exactly what an edit wants.
* The watch covers **everything the sketch is built from**: the entry
  every poll, and on a short cadence the headers standing beside it, the
  units beside it when it is a directory sketch, and local headers reached
  through quoted includes. A helper beside a sketch is reached by a quoted
  include, which resolves relative to the including file and needs no
  include path — so saving the header rebuilds, rather than leaving the
  code that stood before the edit on screen with nothing saying so.
  Beside a BARE sketch the other sources are other sketches, and saving
  one of them is nothing to this one.
* After rebuilding the framework itself, restart the host. The ABI
  version guards deliberate changes to the sketch surface; a separate
  guard refuses to compile while ANY of the framework libraries' public
  headers postdates the running binary, because a dylib built against
  newer headers loads into a host whose structs have the old layout and
  the crash points nowhere near the cause. Every one of those headers
  counts, whatever it happens to declare: a sketch fills a pool the host
  then resizes and builds an element the host then reconciles, so a
  layout read one way on each side corrupts wherever the object is next
  touched.

## A workspace: sketches outside this repository

A `.cpp` path is taken **wherever it stands**, and the app opens on it:

```sh
Sketchbook ~/sketches/my_experiment.cpp
```

One verb writes such a folder, so a new one starts from a sketch that
runs rather than from an empty directory:

```sh
python3 scripts/sigil.py workspace new ~/sketches/aurora_drift
```

It writes the folder's name as the sketch — `aurora_drift.cpp`, a sketch
on this vocabulary that stages a canvas, reads a file through
`ctx.assets` and states the theme's sheet on its root — beside `assets/`,
`captures/` and a README stating the contract below. Files and nothing
else: no build tree, no CMake package, no install step.

The file joins the app's list under its own stem, filed under
**Workspace** with the directory it came from beside the name, and it
compiles, hot-swaps and captures exactly as a sketch in this repository
does. The registry is the compiled-in table and settles the first time
it is read, so the file cannot join it; it joins a session-local list
the listing reads after it, which is why the stem is the name — the
dylib a hot-loaded sketch exports carries neither key nor name of its
own.

So a workspace is just a directory:

```
~/sketches/
  my_experiment.cpp     one sketch, opened by path
  palette.h             a helper, reached by a quoted include
  rain/
    rain.cpp            a sketch that is a directory, opened by its entry
    drops.cpp           a unit of it
  assets/               what mounts at res://
  captures/             where the app's Capture writes
```

The directory form and relative local includes work the same way wherever
the entry stands. Framework headers come from the flags this checkout builds.

For a sketch opened by path, `assets/` beside it is the `res://` root and
`--assets <dir>` names another; in this repository `res://` is the demo
assets root, `build/assets`, which `mise run assets` fills. A sketch's
OWN files stand in its directory, under `data/`: `ctx.local("data/x.csv")`
is the URI of `data/x.csv` under the directory the sketch's entry stands
in — `sketch://<key>/data/x.csv`, the sketches folder mounted at
`sketch://` — which `ctx.assets.table()`, `ctx.assets.image()`,
`ctx.assets.database()` and the hub take as they take any URI. For a
directory sketch that directory is its own, for a bare file it is the
folder the sketches share, so a sketch that carries data of its own is
written as a directory, and a workspace sketch opened by path has the
files beside that path. A `.sqlite` or `.duckdb` file is a data source
like a CSV is: `ctx.assets.database(ctx.local("data/cities.sqlite"))` opens
it in place, cached and reopened when it changes, and its `query()`
answers the same `Table` the CSV decodes to. Saving `palette.h` rebuilds
the sketch that includes it.
Compiling is what makes a workspace file visible, so the flags the build
captured have to be there: the workspace and the `Sketchbook` it opens
in are the same machine and the same checkout, and after rebuilding the
framework the host is restarted like any other.

What a workspace does not get: the plate sweep. `--headless` walks the
registry, which is the compiled-in table — a workspace file is
photographed with `--frame` and measured with `--bench`, one file at a
time.

## One surface, read twice

What a sketch may `#include` is `SigilSketches`' PUBLIC dependencies —
the flags a hot-reloaded sketch compiles with are lifted out of the
compilation database from `sketches/Anchor.cpp`, a source of that same
target, so the include surface cannot drift between a compiled-in sketch
and a reloaded one.

What a sketch may **link** is read off the same target: at configure
time `src/sketch/cmake/SketchLinkSurface.cmake` walks `SigilSketches`' link closure
and force-loads into Sketchbook every archive of this repository's in it
— the public ones, the private ones riding beneath them, and the ones an
optional SDK produced on the machines where it did — with Skia, the one
vendored archive a sketch calls directly, named beside them. An archive
added to the sketch target is therefore in the host without a second
list to keep in step. The failure that list guards against is invisible
everywhere but one place: every sketch still compiles, every compiled-in
sketch still runs, and only a reloaded one fails at `dlopen` with a
symbol not found in the flat namespace — and only for a symbol no
compiled-in sketch happened to pull in, which is why a full tree hides
it and a narrowed one bites. The `sketch_reload_surface` tests exist for
exactly that, one per runtime, and they must go through the dynamic path
to see it.
