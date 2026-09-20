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

**This file is the canon for the literal API — the per-library Doxygen
sites and how they are produced.** The discovery layer above them, the
overview and reference site that catalogues what there is to reach for
and what makes each value, is `REFERENCE.md`. It is written by the
same `docs` target, from the XML inventory the third pass leaves here,
and it never runs Doxygen itself.

## What is in here

| File | What it is |
| --- | --- |
| `Doxyfile.in` | The settings every library's site shares, and the ones each pass overrides. |
| `custom.css` | Project overrides, loaded after the theme. |
| `reference.css`, `reference.js` | The overview-and-reference layer's own styling and behaviour, loaded by no Doxygen page. |
| `REFERENCE.md` | The canon for that layer: what is generated, what is written, how a page is added. |
| `reference_coverage.json` | Its ledger: pages written per library. |
| `overview/`, `guides/`, `glossary.md` | Its hand-written chapters, the ones that belong to no library. |
| `Dockerfile`, `nginx.conf`, `dockerignore` | Serving the generated site. |

The generation itself is `scripts/sigil.py docs`: the three passes, the
theme and tag-file downloads, the HTML header, the navigation layout,
the rendered Doxyfiles, the landing page and the container staging.
CMake keeps what only CMake knows —
whether Doxygen is installed, where it is, and which libraries
registered themselves — and writes that to `build/docs-manifest.txt`,
which is what the script reads. Registration is `sigil_add_docs()` in
`cmake/Docs.cmake`, which `sigil_library_root()` calls for a library;
the whole tree's CMake modules live in `cmake/`, and the manifest points
the verb back here for the templates above.

Nothing here is generated, and nothing here is vendored. The theme is
downloaded at build time.

The build writes two directories. `build/docs/` is the output: the
sites, the landing page, the overview-and-reference layer and the
container files, and nothing else — it is what gets served.
`build/docs-build/` holds the intermediates: the rendered Doxyfiles,
the tag files, the theme, the generated header and layout, the XML
inventory, and the warning logs a documentation check reads. Either can
be deleted; the next `docs` build writes back whatever is missing.

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
  STRIP ${CMAKE_CURRENT_SOURCE_DIR}/include
  INCLUDE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/include)
```

`STRIP` is what comes off a path before it is printed or made into a
page identifier; `INCLUDE_ROOT` is what comes off the `#include` line a
class page prints, and defaults to `STRIP`. A library root passes both:
`include/` and the root itself to strip, `include/` as the include root,
so `<sigilthing/feature/Thing.h>` is the line the page shows while a
chapter beside the library is named from the library.

EVERY PATH MUST BE ABSOLUTE AND MUST EXIST, and the call fails the
configure when one is not. Doxygen resolves a relative input against its
own working directory, finds nothing, and writes a site with that
chapter silently missing — an absence that looks exactly like a page
nobody wrote.

Either call must run before `sigil_finalize_docs()`, which the root
`CMakeLists.txt` invokes after `add_subdirectory(src)`.

A chapter that stands beside the feature it describes is often named
`README.md` too. Doxygen would take such a file for the documentation of
its directory, reachable only from the directory listing, so
`IMPLICIT_DIR_DOCS` is off and a chapter is a page like any other.

## How it is generated

Generation runs in **three passes**.

The first two exist because the libraries reference each other's
types in both directions — SigilWorld takes SigilGeometry's meshes,
SigilCompose takes SigilMotion's animatables — and a Doxygen tag file
can only be read after it has been written. The first pass writes every
tag file and no HTML; the second reads all of them and writes the HTML.
A single pass would resolve only the edges that happen to run in the
order the subdirectories were added.

The result is that a type used across a library boundary links to the
page that defines it, in whichever direction it is used. The same
mechanism links outward: cppreference publishes a tag file of its own,
which is read by every library, so `std::span` and `std::optional` in a
signature are links. It is fetched under the same rules as the theme —
open licence, pinned, hashed, never vendored — out of the release
archive of the offline book, which is the only place it is published;
one member is read back out of the archive. A machine that cannot reach
it still gets its sites, with the standard names left as text. Skia,
HarfBuzz, ICU, Yoga and Diligent publish no tag file at all, so their
types stay text everywhere.

The third pass writes the XML inventory into
`build/docs-build/<Library>/xml/`, for a generator that builds pages out
of the comments rather than a person reading them. It is the one pass
with `EXTRACT_ALL` on: a site leaves out what carries no comment, while
an inventory that left out the same things would be missing exactly the
entities a reader cannot otherwise discover. `--no-xml` skips it.

The overview-and-reference layer is written after those three, out of
that inventory plus the Python declaration stubs and the binding
sources. It is the only consumer of the XML. `--no-reference` leaves it
out; `--reference-only` writes it alone, reading an inventory an
earlier run left behind, which runs no Doxygen and is the loop to use
while prose is moving. With no inventory anywhere it says so and is
skipped. `REFERENCE.md` is the canon for all of it.

A tag file and an inventory are rewritten when a header, a README, or
the Doxyfile that reads them is newer, so a second `docs` build
re-indexes nothing and only writes the HTML. `docs-<Lib>` writes one
library's site and leaves the landing page and the container files
alone, but still brings every tag file up to date first — that is what
its cross-library links resolve against.

## What a page says

Three settings are what make a generated page usable, and each one
answers a question a reader arrives with.

**Which header do I include?** `FULL_PATH_NAMES` is on, with the
library root stripped from a path and the include root stripped from an
include, so a class page prints `#include <sigilcompose/core/Element.h>`
— the line to type — rather than a bare basename that no include
directive spells.

**What is this library made of?** A `@defgroup` per feature is a topic,
`GROUP_NESTED_COMPOUNDS` shows a type inside the topic it belongs to,
and the generated navigation layout puts Topics ahead of the related
pages, so the first screen is the features rather than an alphabetical
class list.

**What will this quietly not do?** Four aliases give the house markers a
rendered form:

| Marker | Renders as |
| --- | --- |
| `@trap` | Trap: |
| `@silent` | Silently does nothing when: |
| `@workaround:` | Workaround for a dependency |
| `@sketch{<stem>}` | Seen in the sketch: `<stem>` |

The workaround marker keeps its colon so that `grep -r 'workaround:'`
still enumerates every one of them, in a comment block and in ordinary
code alike. Nothing else is aliased: a comment stands alone for a reader
who has opened no other document, and an alias that expanded to a
cross-reference would break that.

## What gets documented

`EXTRACT_ALL` is off for the sites. The house convention is that a
**type** carries a
`/** */` block whose first sentence is its summary, and that
self-evident fields and one-line accessors under a documented type do
not repeat it. Undocumented entities are therefore not warned about by
default; `-DSPELLCIRCLE_DOCS_WARN_UNDOCUMENTED=ON` turns the warnings on
for an audit. Under that flag, an undocumented **type** is a real
finding and an undocumented one-line accessor usually is not.

The consequence to know: **an undocumented entity is absent from the
site entirely**, and an undocumented namespace has no page at all, so
every free function in it is reachable only through a file page. That
is what makes a missing comment a discovery failure rather than a
cosmetic one.

`detail` namespaces are excluded. They are implementation scaffolding
that is only reachable because C++ has no way to hide a header, and
documenting them would advertise names that carry no compatibility
promise.

## What a comment in a header says

A header carries a BRIEF and nothing longer. A brief says, in at most
six lines for a member and ten for a class, a namespace or an `@file`,
`@name` or `@defgroup` banner: what the thing IS, what it accepts —
units, or the set of accepted forms — what holds when it is never
stated, and the one thing a caller cannot discover from the signature. A
clause that does not apply is left out rather than padded.

It does not carry narrative, an example longer than two lines, a
rationale, a mechanism, a proof of equivalence, a comparison with
another tool, or an enumeration of how one call interacts with the rest.
Those are prose, and prose lives on a page under the library's
`reference/`, which is compile-checked exactly as its README is: every
sentence a header does not carry is on a page, so a reader of the brief
who wants the argument behind it has one place to look.

Two of the house markers are the spelling for the last two clauses, in
place of running prose:

| Marker | What it opens |
| --- | --- |
| `@trap` | the one thing a caller cannot read off the signature and walks into. One per member; a second trap is prose and belongs on the page. |
| `@silent` | a call that is accepted and does nothing — the wrong kind of node, a state that has no such slot, an input too small to answer from. |

`@name` groups and `@defgroup` structure stay in the headers and in each
library's `docs/Groups.dox`: they are what the site's topics are built
from, and they are structure rather than prose.

## Checking that a comment arrives

```sh
python3 scripts/sigil.py check --docs                  # the branch's libraries
python3 scripts/sigil.py check --docs --all            # every library
python3 scripts/sigil.py check --docs --docs-undocumented
```

A comment can be written perfectly and still never reach a page. An
`@file` with prose on the same line loses the file's documentation to a
filename that does not exist; an `@ingroup` naming another library's
group is dropped; a `#` in a URL is read as a link request; a stray
backtick closes the block early. The tier parses the scoped libraries
and reports each of those. It writes nothing but a warning log, needs a
configured tree only for the manifest, and is OPT-IN — it never runs as
part of an ordinary check, and what it finds is a page that reads wrong
rather than code that is wrong. `scripts/README.md` is the canon for
the verb and its scope rule.

`--docs-undocumented` adds the audit report, which never fails a run:
compounds with no comment listed by name, members counted by kind.

## The theme

Doxygen's stock HTML is close to unusable on a phone: fixed-width member
tables, a navigation tree that assumes a mouse, and a layout that
ignores the viewport. [Doxygen
Awesome](https://github.com/jothepro/doxygen-awesome-css) (MIT) replaces
the stylesheet without changing the generated HTML structure, so the
markup Doxygen emits stays the markup the theme expects.

It is pinned to a commit and hash-checked per file in
`scripts/sigil.py docs`, fetched into `build/docs-build/theme/` through
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
rather than an error. The navigation layout is generated for the same
reason, with one edit applied to what Doxygen writes: the topics tab
moves up beside the front page. A layout frozen here would drift into a
navigation tree missing whatever a later Doxygen added.

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
