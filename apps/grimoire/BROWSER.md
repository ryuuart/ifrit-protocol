# The browser

How Grimoire's window goes through the registry: what it is made of, how a
sketch is selected and presented, how the list is narrowed, and the
thumbnails it keeps. The rows, the thumbnail store and the author prose it
reads are [SigilSketch's](../spell-circle-canvas/src/sketch/REGISTRY.md);
what follows is how this application shows them.

The window composes four responsibilities: `SketchCatalog` supplies rows
and thumbnails, `Browser` filters and selects those rows,
`CanvasPane` owns the viewport, input gestures and compile-error panel, and
`SketchActions` runs frame, video and benchmark commands from a supplied
row. Selection does not launch work. Commands own their subprocess and
status, so exporting a sketch does not change the catalog or the live
canvas. A shared notice keeps command progress and its result visible outside
the details drawer until dismissed. An empty command row selects the full
registry for video export.

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

Grimoire uses the shared `Ifrit.Qt` system-palette theme and controls.
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

**The thumbnails are the app's own.** Grimoire keeps one store — one PNG
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

`Grimoire --thumbnails` fills the store headless, over the same budget
and writing the same notes, and exits non-zero naming the sketches that
failed. It is the same render the window's fill takes, down to the
runtime: a set is drawn on the CPU mesh executor either way.

**What is not in a row is the canvas.** A sketch declares its size, its
ground and the moment it names from inside its own setup, so those are
facts of a RUNNING session and cannot be read off a file that has not
run. They fill in as sketches are presented and the browser keeps them
afterwards, and a row that has never been presented says so rather than
guessing.


**What the inspector says about a sketch** — its subject, the knobs to
edit first and the tags that file it in the tree — comes from the top of
the sketch's own file, by the rule [SigilSketch's registry
chapter](../spell-circle-canvas/src/sketch/REGISTRY.md#how-a-sketch-introduces-itself)
states.
