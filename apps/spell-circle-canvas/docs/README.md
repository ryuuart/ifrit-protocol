# Documentation

The C++ API reference: one Doxygen site per library, generated from the
headers, cross-linked, and servable as a container.

```sh
cmake --build build --target docs        # everything
cmake --build build --target docs-SigilGeometry   # one library
open build/docs/index.html
```

The target is absent when Doxygen is not installed, and the build says
so at configure time. `brew install doxygen graphviz` — graphviz is
optional and adds inheritance graphs.

## What is in here

| File | What it is |
| --- | --- |
| `Docs.cmake` | Registration: `sigil_add_docs()`, which `sigil_library_root()` calls for a library, and the targets. Included by the root `CMakeLists.txt`. |
| `Doxyfile.in` | The settings every library's site shares. |
| `custom.css` | Project overrides, loaded after the theme. |
| `Dockerfile`, `nginx.conf`, `dockerignore` | Serving the generated site. |

The generation itself is `scripts/build_docs.py`: the two passes, the
theme download, the HTML header, the rendered Doxyfiles, the landing
page and the container staging. CMake keeps what only CMake knows —
whether Doxygen is installed, where it is, and which libraries
registered themselves — and writes that to `build/docs-manifest.txt`,
which is what the script reads.

Nothing here is generated, and nothing here is vendored. The theme is
downloaded at build time.

The build writes two directories. `build/docs/` is the output: the
sites, the landing page and the container files, and nothing else — it
is what gets served. `build/docs-build/` holds the intermediates: the
rendered Doxyfiles, the tag files, the theme, the generated header.
Either can be deleted; the next `docs` build writes back whatever is
missing.

## Adding a library

A Sigil library registers through `sigil_library_root()` in its root
`CMakeLists.txt`, which passes its `include/` tree and `README.md` on to
`sigil_add_docs()` with the README as the site's front page, so the
generated pages open on the document that is already canon for that
library; `DOCS` names further pages. Anything that is not a library root
calls `sigil_add_docs()` itself:

```cmake
sigil_add_docs(
  NAME SigilThing
  BRIEF "One line, shown on the landing page"
  INPUT ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  MAINPAGE ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  STRIP ${CMAKE_CURRENT_SOURCE_DIR}/include)
```

Either call must run before `sigil_finalize_docs()`, which the root
`CMakeLists.txt` invokes after `add_subdirectory(src)`.

## How it is generated

Generation runs in **two passes**. The libraries reference each other's
types in both directions — SigilWorld takes SigilGeometry's meshes,
SigilCompose takes SigilMotion's animatables — and a Doxygen tag file
can only be read after it has been written. The first pass writes every
tag file and no HTML; the second reads all of them and writes the HTML.
A single pass would resolve only the edges that happen to run in the
order the subdirectories were added.

The result is that a type used across a library boundary links to the
page that defines it, in whichever direction it is used.

A tag file is rewritten when a header, a README, or the Doxyfile that
reads them is newer than it, so a second `docs` build re-indexes nothing
and only writes the HTML. `docs-<Lib>` writes one library's site and
leaves the landing page and the container files alone, but still brings
every tag file up to date first — that is what its cross-library links
resolve against.

## What gets documented

`EXTRACT_ALL` is off. The house convention is that a **type** carries a
`/** */` block whose first sentence is its summary, and that
self-evident fields and one-line accessors under a documented type do
not repeat it. Undocumented entities are therefore not warned about by
default; `-DSPELLCIRCLE_DOCS_WARN_UNDOCUMENTED=ON` turns the warnings on
for an audit. Under that flag, an undocumented **type** is a real
finding and an undocumented one-line accessor usually is not.

`detail` namespaces are excluded. They are implementation scaffolding
that is only reachable because C++ has no way to hide a header, and
documenting them would advertise names that carry no compatibility
promise.

## The theme

Doxygen's stock HTML is close to unusable on a phone: fixed-width member
tables, a navigation tree that assumes a mouse, and a layout that
ignores the viewport. [Doxygen
Awesome](https://github.com/jothepro/doxygen-awesome-css) (MIT) replaces
the stylesheet without changing the generated HTML structure, so the
markup Doxygen emits stays the markup the theme expects.

It is pinned to a commit and hash-checked per file in
`scripts/build_docs.py`, fetched into `build/docs-build/theme/` through
the same downloader as the demo assets, and never vendored. A file whose
bytes already match is not re-fetched, so only the first `docs` build
touches the network.

`custom.css` carries what the theme cannot fix from its variables:
Doxygen emits several blocks whose intrinsic width comes from their
content — member tables, code fragments, dot graphs — and on a narrow
screen those widen the page itself rather than overflowing inside it.
Each rule there confines that overflow to the element causing it.

The header is generated rather than checked in. Doxygen emits the header
its own version expects, and a copy frozen in the source tree would
drift on every Doxygen upgrade — which shows up as a half-styled page
rather than an error.

## Serving it

The `docs` target copies `Dockerfile`, `nginx.conf` and
`.dockerignore` into `build/docs`, so the generated tree is a complete
build context:

```sh
cmake --build build --target docs
docker build -t spellcircle-docs build/docs
docker run --rm -p 8080:80 spellcircle-docs
```

Then <http://localhost:8080>. The image is nginx plus static files — no
Doxygen, no toolchain, and nothing in it can regenerate a page.
