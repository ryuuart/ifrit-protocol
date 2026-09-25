# The registry and its plates

## Going through the registry

The window composes four responsibilities: `SketchCatalog` supplies rows
and thumbnails, `Browser` filters and selects those rows,
`CanvasPane` owns the viewport, input gestures and compile-error panel, and
`SketchActions` runs frame, video and benchmark commands from a supplied
row. Selection does not launch work. Commands own their subprocess and
status, so exporting a sketch does not change the catalog or the live
canvas. A shared notice keeps command progress and its result visible outside
the details drawer until dismissed. An empty command row selects the full
registry for video export.

- `core/Catalog.h` — `catalog` reads `CatalogSources` into one `CatalogRow`
  per registry entry and per file, without Qt; the browser maps each to the
  row QML reads and adds the thumbnail and the canvas a session learns.
- `core/State.h` — `setStateDirectory`, `stateDirectory` and
  `stateLocation`: the one root a process keeps what it writes between
  runs under, and where each kind of state stands beneath it.
- `core/Sources.h` — `SourceMetadata` and `sourceMetadata` read author prose
  without Qt; `sourceOf`, `directorySketch`, `sourcesUnder` and `unitsOf`
  resolve the files and translation units belonging to a sketch. `headersOf`
  follows its local quoted includes across owner directories.

The thumbnail worker is the existing `ThumbnailQueue`; the catalog
marshals its results onto the GUI thread. Captures and device readback
remain on the render thread, sharing the context that owns the live
session's images. What follows a readback does not: `Host::still` hands
back the pixels and `ThumbnailWriter` encodes and writes them on a
worker of its own, one still in flight, so a frame that photographs a
sketch for the store pays the readback and nothing after it.

The library and canvas occupy two resizable panels. Search, grouping, view
mode and sorting live together in the library; the canvas has explicit Fit
and actual-size controls. Details opens a drawer over the right edge at any
window size and closes on Escape or a click outside it. The drawer starts
closed, leaving the artwork its space. Both panel headings align their title
and contextual detail; the canvas heading identifies the current sketch's
collection, language and dimensions. Browsing and presentation stay separate:

* **selection is a look.** Arrow keys move it, a click moves it, and all
  it moves is the selection and its details. Whatever the canvas was
  presenting keeps presenting while you read.
* **Enter presents.** So does a double click, the Open action beneath
  the results, and the inspector's Open. This is the only thing that changes what is drawn —
  and the resident set is what makes it cheap, because a sketch already
  opened comes back without being built again. It is also what ends the
  thumbnail fill: from there on the canvas is what draws.
* **A click on the canvas gives it the keyboard.** The pointer over the
  canvas and the keys while it holds focus reach the running session in
  the sketch's own canvas units, through `Session::pointer` and
  `Session::key`, for a sketch that reads them; a click on the list takes
  the arrows back. A sketch with nothing for a pointer to do ignores what
  arrives, and a drag over a set still orbits it.

The library's group picker opens a navigation tree grouped by **subject**, using tags from each
sketch's opening comment. Paths such as `Typography/Paragraph` make an
expandable tree. Selecting a parent includes all its descendants; a sketch
with several tags appears in several groups, while every count and result
list includes that sketch only once. Expanding a branch changes the tree
without rebuilding the result views or changing the canvas.

The tree's **Collections** option builds a tree from registration categories:
`Study · Type` becomes Study → Type. These are logical groups independent of
source directories. **All sketches** clears the group filter, and **Untagged**
keeps sketches without subject tags reachable. The selected group, grouping
mode and expanded branches survive relaunches; each group and search keeps
separate list and gallery scroll positions during the run. The inspector's
tag buttons open the corresponding subject group.

Two views share the selected group, search and sort order, with a toggle in
the library header. The adjacent sort menu selects a field and direction for
either view. Switching views preserves the selection and each view's scroll
position; it does not jump to the selected sketch. Equal sort values,
including unknown session facts, are ordered by name so narrowing a search
does not reshuffle ties.

* **the list** — one row per sketch, with the thumbnail,
  blurb, collection, runtime, canvas, declared moment and line count in
  columns where width permits. A compact metadata line keeps collection,
  runtime and known dimensions visible in narrow panes. Clicking a column
  heading orders by it; clicking again reverses.
* **the gallery** (the default when no view preference is saved) — every
  matching sketch as its own still, a two-line description, collection,
  source language and line count, with canvas dimensions when known. The
  Open action and current-canvas status stay visible in each card.

Sketchbook uses the shared `Ifrit.Qt` system-palette theme and controls.
It follows the operating system's light or dark appearance; rendered
sketches keep their own palettes. Panels, headings, search, selection, status
and buttons reuse the same controls as the SigilWeave gallery. View controls and sortable headings are
keyboard controls, search has an accessible clear action, and an empty
result offers to clear the filters. The selected sketch's Open action
remains visible when the details panel is hidden; a presented sketch offers
Replay to restart its animation.

The filter takes free words and field words together, and every word has
to match. Free words search names, categories, tags, blurbs and file stems;
`folder:`, `tag:` and `kind:` narrow on their respective fields. For example,
`tag:typography tag:motion` finds sketches tagged with both subjects.
Navigation counts show search hits before the selected group narrows them,
so another branch says how many results selecting it would show. A selected
group with no hits keeps its name and shows an empty-state message.
`/` puts the cursor in the search field and Escape empties it. **Clear
filters** clears both the search text and selected group in either view;
the search clear action removes only the search; All sketches clears only the group.

**The thumbnails are the app's own.** Sketchbook keeps one store — one PNG
per sketch, under the platform cache location (`thumbnails/` under a
`--state` root instead). Each
file's name carries a KEY: a hash of the sketch's source files and local
headers, including quoted includes followed across owner directories.
A thumbnail whose key no longer matches is stale and is drawn again.

**The host is not in the key.** A library edit changes what a sketch
draws while its source stands still, and every still on disk goes on
claiming to be fresh. That is the trade taken deliberately: keying on the
host would throw all of them away on every rebuild, and the refresh on
opening writes back the frame that was just presented — so a still a
rebuild made wrong heals the moment it is looked at.

They are filled at two moments, and never while a sketch is being
presented.

**The fill, at launch.** The window comes up on the browser with the
canvas dark, and draws a still for every sketch that has none: the
sketch's kind opened and stepped to its declared moment — the same
capture the CPU plate tier takes — scaled to the thumbnail size, one at a
time, on the CPU and never touching the device. That holds whatever the
process installed: a still opens every kind on the CPU runtime — a set's
whole frame and a 2D body's mesh painter alike — so a sketch's thumbnail
is drawn on the mesh executor even in a window whose live canvas is
lighting sets on a device. The status strip counts
them off, `thumbnails 12/41 …`, and each row fills in as its file lands
without remounting the others; a row on screen is moved to the front of
the queue, so what you are looking at is drawn first. **Opening a sketch
ends the fill** — the walk in flight is let go at its next frame and the
queue is dropped — and the fill finishing opens the sketch the run was
pointed at. A run that named a sketch (`--sketch`, a file on the command
line) or that is here to photograph or measure one (`--shot`,
`--window-bench`) opens at once and never fills.

**One still is bounded.** A sketch whose walk runs past the per-sketch
budget, and a sketch that declared itself a plate with `ctx.plate()`
(which is a statement that its subject costs what a plate costs), is
abandoned and gets a one-line NOTE beside where its still would have
gone, under the same key: the note stands in for the picture, the fill
moves on, and the question is asked again only when the sketch's source
changes. `--thumbnail-budget <sec>` names another budget and
`--thumbnail-heavy` walks the declared plates as well. A sketch that
could not be drawn at all is named once in the status strip and not tried
again this run.

**The refresh, on opening.** Once a sketch is presented, its session is
photographed once — as it reaches the moment it declared, or after a
second of its own clock when it declares none — and that frame is written
into the store under the sketch's current key. The encode and the write
are not in that frame — only the repaint and the readback are, because
only they need the thread the frames are drawn on; the pixels go to a
worker beside it and the row is told once the file has landed. A run
measuring frames (`--window-bench`) is out of the refresh entirely: the
photograph's repaint and readback are still taken on the render thread
and inside a frame, which is the one thing a stretch whose whole
subject is how long a frame takes cannot have in it. So the stills
refresh as you browse, they are the frames you were looking at, and nothing renders
in the background to keep them current. A sketch with no thumbnail yet
gets a drawn glyph for the runtime it draws through.

`Sketchbook --thumbnails` fills the store headless, over the same budget
and writing the same notes, and exits non-zero naming the sketches that
failed. It is the same render the window's fill takes, down to the
runtime: a set is drawn on the CPU mesh executor either way.

**What is not in a row is the canvas.** A sketch declares its size, its
ground and the moment it names from inside its own setup, so those are
facts of a RUNNING session and cannot be read off a file that has not
run. They fill in as sketches are presented and the browser keeps them
afterwards, and a row that has never been presented says so rather than
guessing.

### How a sketch introduces itself

The inspector reads prose and subject tags from the top of the sketch's
own file. The rule is small on purpose, so an author can write to it:

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

All three are optional. A file without prose omits those blocks, and a file
without tags remains available under Untagged and its collection.

## Plates

`--headless <outdir>` renders every selected sketch to
`<outdir>/plate_<name>.png` and prints a timing table beside it.
`--at <sec>` takes every plate at that scene time instead of at each
sketch's own moment; sweeping at two times and differencing the plates
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
drawn tree is resolution-independent, so its still is one more frame
re-rendered at up to twice the canvas — a texture bake re-runs at the
capture scale rather than being upsampled. A lit set is FORMED at one
resolution and its still describes nothing, so there is nothing to form
again larger and its plate is the frame it just finished. A pen's
canvas holds every frame's residue at the resolution it was formed at,
so its plate, too, is the frame just finished. `Session::still()` is
that seam.

Which resolution that is comes off the canvas a host hands over. A
plate's canvas is the declared size and carries no transform; a live
window's carries the fit AND the screen's own scale, and a set formed at
its declared size and then fitted upward would be a magnified picture of
a smaller one. So a set reads the scale off the canvas it is given,
forms its frame at that many pixels, and puts the result back on the
declared canvas — which on a plate's canvas is the identity, and is why
the two hosts agree to the byte.

### The promoter, and the one lane that exercises it

A headless session is opened DETERMINISTIC, and a deterministic session
holds the composer's automatic texture promotion off. The promoter
decides by a stopwatch — a node whose paint measures over a millisecond
for eight frames is baked and blitted thereafter — so whether it fires
depends on how busy the machine is, and with it on the same binary draws
two different plates. Holding it off is what makes a hash a verdict.

The cost is that the whole sweep renders the runtime with one of its
features switched out. `--promotion` is the door back: it opens every
session with promotion ON and changes nothing else — same clock, same
fixed step, same declared capture moment, same `ctx.measured()` pins —
so the only difference between the two renders of a scene is the
promoter. `--no-promotion` and `--promotion` ask for opposite runs and
naming both is refused.

IT OPENS THEM EAGER. The stopwatch that makes the promoter load-dependent
would make the lane load-dependent too: on an idle machine nothing
crosses the bar and the run reports a clean sweep it did not earn, while
on a loaded one a different handful of nodes crosses it each time. So
`--promotion` asks for the eager policy — every node the composer's rules
admit is baked from its first frame, whatever it costs — and nothing
about what a bake is allowed to do changes. One scene therefore exercises
the same node set on every machine, and it is the whole promotable set
rather than the few nodes that happened to be slow.

…UNLESS THE SKETCH DECLARED OTHERWISE. `ctx.nonlinearPicture()` says the
sketch's picture is not linear in what went into it: it ends on a step, a
round, a gate or a reciprocal — a view transform quantizing each channel
to a palette, a bright pass through a smoothstep, anything that
unpremultiplies and so carries a gain of 1/alpha. There is then no bound
between a difference UNDER that stage and the difference it shows, so one
code value the promoter is allowed to cost arrives as a whole step, in a
place the difference was never in. Such a sketch holds the promoter off
from its own setup whatever a host asks for, its picture is drawn from
live paint everywhere, and a sweep that asked for the promoter prints
`<name>: declared nonlinear` so the tier can say the scene stood under
its own declaration rather than reporting an agreement it never tested.
The declaration belongs to the sketch, which is where the ablation
showing that the picture UNDER the stage is right has to be stated;
`spacejam_1996` and the three `eva_magi_*` plates carry it.

What comes out is not byte-comparable and is not meant to be. A promoted
node is baked under the live matrix post-translated by an integer, and
inverting that matrix to find a shader's local coordinates does not
cancel the integer to the last bit at a scale whose reciprocal is
inexact, so a shaded pixel can land ONE code value from the live paint
and nothing may land further. Where the held-off plate holds CONTENT the
bake lands on something, and there the bound is two — the node's own
coverage rounded into the bake and the bake rounded onto what it lands
on — PER CACHED RASTER the pixel stood under. Nothing nests, but
independent nodes overlap, and a stack of concentric rings each promoted
on its own puts seven or nine of them over one pixel; each rounds, and
the rounding does not decay. `--composites` writes the count beside each
plate, `counts_<sketch>.png`, one grey level per device pixel, and
`--compare` prices the content difference by it. A difference past that
is a picture that moved — a bake somewhere else, rasterised against
another clip, or gone stale — and that is a defect in the promoter rather
than a plate to adopt.

`scripts/sigil.py plates` drives this: three tiers over one binary. The
CPU tier judges every sketch, canvas and set alike, on byte identity
against one baseline manifest; the device tier renders the same sketches
through the device and judges each against the CPU plate of the same
run, per colour channel; the promotion tier renders each scene with the
promoter held off and again with every promotable node eagerly baked,
and judges the pair within one code value. Only the CPU tier keeps a baseline. The judgement itself is
`scripts/README.md`'s.
