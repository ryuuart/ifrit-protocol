# The registry and its plates

## The registry, its rows and its thumbnails

A host reads the registry as rows and photographs it as thumbnails; how it
shows them is its own. Grimoire's browser is one such reader, and [its
chapter](../../../grimoire/BROWSER.md) says how it does.

- `core/Catalog.h` — `catalog` reads `CatalogSources` into one `CatalogRow`
  per registry entry and per file, without Qt; a browser maps each to the
  row it shows and adds the thumbnail and the canvas a session learns.
- `core/State.h` — `setStateDirectory`, `stateDirectory` and
  `stateLocation`: the one root a process keeps what it writes between
  runs under, and where each kind of state stands beneath it.
- `core/Sources.h` — `SourceMetadata` and `sourceMetadata` read author prose
  without Qt; `sourceOf`, `directorySketch`, `sourcesUnder` and `unitsOf`
  resolve the files and translation units belonging to a sketch. `headersOf`
  follows its local quoted includes across owner directories.

`ThumbnailQueue` is the worker a host fills thumbnails on, and the host
marshals its results onto its own GUI thread. Captures and device
readback remain on the render thread, sharing the context that owns the live
session's images. What follows a readback does not: `Host::still` hands
back the pixels and `ThumbnailWriter` encodes and writes them on a
worker of its own, one still in flight, so a frame that photographs a
sketch for the store pays the readback and nothing after it.

### How a sketch introduces itself

`sourceMetadata` reads prose and subject tags from the top of the
sketch's own file, which is what a browser shows beside the picture. The
rule is small on purpose, so an author can write to it:

The header is every line from the first line of the file down to the
first line that is neither a comment nor blank — a run of line comments,
a doc block, or one after the other. The comment markers come off, and a
line reading only `@file` is dropped. What is left reads as
**paragraphs**: runs of non-blank lines, broken by blank lines and by
rule lines (a line of nothing but `=` or `-`). Python headers accept the
opening module docstring and `#` comments; their first paragraph is the
subject, without a separate title paragraph.

* **The subject** is the first paragraph after the title paragraph — the
  title being the first one, which by convention opens `stem.cpp — …`.
  A one-line paragraph that ends no sentence is a heading: it is kept and
  read on into the paragraph below it, so a file that puts `THE PATTERN`
  over its opening prose shows both.
* **Edit these first** is the paragraph opening with a line that reads
  exactly `EDIT THESE FIRST`, minus that line — the knobs the author says
  to reach for, stated once, beside the code they name. It keeps one line
  per knob: a line indented deeper than the first is an entry that ran
  past the file's own margin, and it rejoins the line above.

* **Tags** come from lines beginning `TAGS:`. Commas separate paths and
  slashes nest subjects: `// TAGS: Typography/Paragraph, Motion/Text` files
  one sketch in both groups. Spaces around components and empty components
  are removed; repeated paths count once. Tag lines never become subject
  prose or editing instructions.

* **A Python sketch's name and category** are its module's top-level
  `NAME = "…"` and `CATEGORY = "…"`, each one plain string literal, read
  off the line and never run; the last of each wins. A workspace file
  opened by path is filed under them, keeping its stem as its key; a file
  that states neither reads as its stem in its workspace group.

All of these are optional. A file without prose omits those blocks, and a file
without tags carries none, which a browser files under its collection alone.

## Plates

`sweep` renders every selected sketch to `<outdir>/plate_<name>.png`
and prints a timing table beside it; Grimoire's `--headless <outdir>` is
that sweep, and the flags below are the ones it reads into
`SweepOptions`. `--at <sec>` takes every plate at that scene time instead
of at each sketch's own moment; sweeping at two times and differencing the plates
says which sketches are still moving when they are photographed.

The capture is a function of the **declared moment** and of nothing a
machine decides. Everything the timing table does is a time budget, so
the frames it spends depend on how fast the machine is; a plate cannot
be allowed to. So a sketch that names its moment is reopened and stepped
from zero at a fixed 1/60 to that moment, and one that names none is
topped up to a frame derived from the benchmark caps. `--ledger` skips
the benchmark phases entirely and goes straight there, which is most of
a sweep's wall clock — and produces a bit-identical plate, because the
capture never depended on the phases in the first place.

The runtimes make a plate differently, and each way is load-bearing. A
drawn tree is resolution-independent, so its still is one more frame,
of no length, re-rendered at up to twice the canvas — a texture bake
re-runs at the capture scale rather than being upsampled, and the clock
stays at the moment the plate names. A lit scene is FORMED at one
resolution and its still describes nothing, so there is nothing to form
again larger and its plate is the frame it just finished. A pen's
canvas holds every frame's residue at the resolution it was formed at,
so its plate, too, is the frame just finished. `Session::still()` is
that seam.

Which resolution that is comes off the canvas a host hands over. A
plate's canvas is the declared size and carries no transform; a live
window's carries the fit AND the screen's own scale, and a scene3d sketch formed at
its declared size and then fitted upward would be a magnified picture of
a smaller one. So a scene3d sketch reads the scale off the canvas it is given,
forms its frame at that many pixels, and puts the result back on the
declared canvas — which on a plate's canvas is the identity, and is why
the two hosts agree to the byte.

### Two directories of plates, compared

How far every plate in one directory stands from the plate of the same
name in the other, decoded and differenced channel by channel, is what
Grimoire's `--compare` prints. The comparison is a value first:
`sigil::sketch::compare` answers a `sigil::sketch::Comparison` holding
one `sigil::sketch::PlateComparison` row per plate,
`sigil::sketch::printComparison` writes the rows as lines,
and Python reads the same rows as `sigil.sketch.compare(first, second)`.
Each row prints as the line its outcome opens:

```
compared <name> mean <mean> p99 <p99> max <max> clear <max> content <max>
         graze <max> <how many>
size <name> <W>x<H> <W>x<H>
missing <name> first|second
unreadable <name> first|second
```

Every distance is an absolute difference of one 8-bit channel, in 0..255,
over every channel of every pixel. `clear`, `content` and `graze` are that
worst difference split three ways over the pixels it stands on, because a
caller's tolerance can depend on which it is. `clear` is where the FIRST
plate — the reference — holds transparent black, so nothing was
composited under the difference at all. `graze` is where the difference is
CONFINED TO AN ANTIALIASED EDGE BOTH PLATES DRAW: the picture varies by at
least the difference within a pixel of that point in each of them, and so
does every differing pixel beside it, so what changed is one pixel's
coverage of an edge the two agree about — which is what a mark standing a
fraction of a device pixel from where the other drew it looks like, and
the count beside it says on how many pixels. `content` is everything else:
a difference that reaches a pixel no edge explains, which is what a
picture that MOVED shows — pixels taken off the edges, a mark that is
gone, a wash at another value.

It opens no sketch, needs no fonts, no assets and no device, and it
JUDGES NOTHING — how close is close enough is a tolerance about a
machine, which is the plate ledger's to hold. The ledger's CPU tier is
the caller: it renders a directory of raster plates and reads the rows
against the device plates, and `sigil.py plates compare <dir-a> <dir-b>`
prints the same lines.

### The plate ledger

`scripts/sigil.py plates` drives Grimoire's sweep: two tiers over one
binary. The device tier is the reference: it renders every scene2d and
scene3d sketch on the device and judges each on byte identity against one
device manifest, which `--rebase` adopts; the CPU tier is a smoke run of
the raster path, rendering each scene2d sketch on the CPU and reporting
its drift from the device plate per colour channel, keeping no baseline
and failing only when a sketch cannot render.
The judgement itself is `scripts/README.md`'s.
