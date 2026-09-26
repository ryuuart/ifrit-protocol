# Running a sketch

```sh
Sketchbook [--no-gpu]                       # the app
Sketchbook --sketch <name>                  # the app, on that one
Sketchbook <file.cpp>                       # the app, on that file
Sketchbook --workspace <directory>          # the app, on a folder of sketches
Sketchbook --examples                       # the app, on the bundled catalogue
Sketchbook --list [--kind canvas|set]      # the registry, one per line
Sketchbook --catalog [<file.cpp>]           # the browser's rows, one JSON each
Sketchbook <file.cpp> --frame out.png [--at <sec>] [--scale <n>] [--gpu]
                                  [--frames <count>] [--fps <n>]
                                  [--deterministic | --no-deterministic]
Sketchbook <file.cpp> --bench [--bench-frames <n>] [--jitter-dt [amp]]
Sketchbook --headless [<outdir>] [--gpu] [--sketch <name>] [--kind <k>]
           [--at <sec>] [--ledger] [--no-promotion | --promotion]
           [--composites]
Sketchbook --video out.mp4 [--video-frames <n>] [--video-size <WxH>]
           [--video-bitrate <bits>] [--fps <n>] [--sketch <name>]
           [--kind <k>] [--gpu]
Sketchbook --compare <dir-a> <dir-b>        # two sweeps' plates, differenced
Sketchbook --window-bench [<sec>] [--window-size <WxH>] [--window-scale <n>]
Sketchbook --thumbnails [--sketch <name>] [--kind canvas|set]
           [--thumbnail-budget <sec>] [--thumbnail-heavy]
Sketchbook --publish [<name>] [--sketch <name>]
                                            # the window's frames, live, to
                                            # other applications
Sketchbook --shot <png> [--sketch <name>]   # the whole window, once live
… [--assets <dir>]                          # what mounts at res://
… [--state <dir>]                           # where the run keeps what it
                                            # writes for a later run
… [--inspect[=<port>]]                      # the protocol's endpoint: the
                                            # window mounts it unasked, a
                                            # sweep when asked
Sketchbook --headless --inspect[=<port>] [--state <dir>]
                                            # no window: a host a client
                                            # drives over the protocol
```

`--sketch` takes a case-insensitive substring and answers to a sketch's
filed name or its file stem, which is the loop for visual iteration.
`--headless` writes its plates into `sketch_plates/` when no directory
follows it — unless `--inspect` is given and no sketch or kind is named,
when it serves the protocol instead. [PROTOCOL.md](PROTOCOL.md) is the
chapter on what a client driving a sketch host is answered: the agents,
the harness a test drives one through, and what each lane mounts.

**`--state <dir>` is the one place a run keeps what it writes for a later
run.** The builds of sketch files, the browser's thumbnails, the device
programs a run recorded, the settings and the recent workspaces each take
a directory or a file under it — `builds/`, `thumbnails/`, `pipelines/`,
`settings/` — instead of the platform's own locations, and a window or a
command the run starts is handed the same root. A test or a script that
names a fresh root reads nothing an earlier run left and leaves nothing
behind; `sigil::sketch::setStateDirectory` is the same root for a C++
host that is not Sketchbook, and `sigil::sketch::stateLocation` answers
where one kind of state stands under it. Python does not bind it, so a
sketch built from a Python process keeps its builds in the platform cache
location. Nothing turns the build cache off; a run that must not reuse an
earlier build names a fresh root.
`--shot <png>` captures the app window rather than a sketch, which is
the only way to look at the browser and the inspector.

**`--publish` offers the sketch's own canvas to other applications.**
A program on this machine subscribes to a name and receives every frame
this canvas draws, composited live in its own scene — a VJ program, a
projection mapper, a recorder. The name is the one given, or the stem of
the sketch the run opens on; a subscriber binds to it, so it is the
run's and does not follow the sketch on screen. The status bar's **Publish**
button and Ctrl-P invoke the same action. A normal launch without `--publish`
starts with publishing off and uses **Sketchbook** when enabled. The status
button shows whether publishing is enabled; the status line names a starting
or active publisher and reports failures.
If publication cannot start, the button returns to off and the canvas shows
the reason until the next attempt; CPU rendering cannot publish frames.

**WHAT LEAVES IS THE CANVAS, NOT THE WINDOW.** A publishing window draws
the sketch once into a texture of its own — the declared canvas, at one
texture pixel per canvas unit, cleared to the declared ground — and
offers that. So a subscriber receives the size the sketch declared
however large the window is and however far it is zoomed, receives no
matte around the picture, and receives THE GROUND'S ALPHA: a sketch that
declares `.background = {0, 0, 0, 0}` publishes a fully transparent
ground and one that declares a translucent colour publishes it
translucent, premultiplied as drawn, so another application composites
the sketch over its own scene. The window keeps showing what it showed
before — the matte, the letterbox and the zoom — with that one frame
magnified into it rather than the sketch drawn a second time, because a
sketch is stateful and one tick is one frame. A frame that cannot be
drawn into a canvas of its own is not offered at all and publishing
stops with the reason on the button: a subscriber handed the window
instead would be composited the matte. `alpha_ground` and
`python_alpha_ground` are the sketches that declare a transparent ground
and say what to do with one.

Seer, the wire and texture reader, is what to check it with:
`Seer --list-textures` says what is being offered, `Seer --texture <name>`
opens a window on it and `Seer --texture <name> --grab <png>` writes its
newest frame to a file.

**Frames come the other way too.** `sigil::sketch::Guest`, from
`<sigilsketch/canvas/Guest.h>`, is the same door read from the inside of
a sketch: made from the context a sketch was handed — a page's
`SketchContext` or a set's `SetContext` — and the name a publication
announces, it answers with the newest frame two ways.
`sigil::sketch::Guest::frame` is the frame as an image on the recorder
the canvas is being drawn on, one per frame that arrived and null while
nothing is publishing; `guest_picture` is the page that wears one. The
frame arrives with its first row at the image's BOTTOM — the order the
surface a publication is carried on is written and read — so it is drawn
once into a target of its own on that recorder and what comes back is
upright, which is the one thing done to it on the way in — the
subscription's own doing, since a subscription is a `media::PixelSource`
whose frames are turned as they are bound. It takes the
`SkCanvas` as well as the recorder, so a caller inside a paint program
asks with what it is already holding and names Graphite nowhere.
`sigil::sketch::Guest::texture` is the same frame as a
`material::Texture`, which is what a surface's base-colour slot takes, so
a body in a set wears the publication the way it wears any other picture;
`guest_body` is the set that turns one under a light. That one reads the
pixels back into host memory, because the renderer that shades a body
does not stand where a publication arrives — a frame is a Metal texture
and the world draws through Vulkan — and a slot that works on every tier
is worth a copy where no handle can cross.
`sigil::sketch::Guest::publishing` and
`sigil::sketch::Guest::application` are what a scene says about the
publication it is wearing. A capture subscribes to nothing at all: what
another application happens to be offering while a still is taken is not
a function of the sketch that took it, so a plate of such a scene is what
it draws with nobody publishing.

What travels is a texture, so publishing wants the window on Graphite.
On the CPU raster fallback there is no texture to offer, and the flag is
REFUSED rather than answered with something else: the console says so
and publishing stays off. Every lane that renders without a window — a sweep, a still, a
montage, a measurement, the warm command — refuses the flag outright.

`--catalog` prints the browser's rows without opening a window, one JSON
object per line — the registry first, and a file this run was pointed at
after it. They are `sigil::sketch::catalog`'s rows, `sigil::sketch::CatalogRow`,
as the browser maps them: `name` is the display spelling and `filedName`
the name a plate is written under. What a script reads off them is what
the browser reads before anything has been built: a compiled-in entry
names the runtime it draws through, and a file opened by path has none
until it has been compiled and says so rather than guessing. `--list` is
the same registry for a reader, `sigil::sketch::registryRows` spelled one
filed name per line; a sketch this machine cannot run is listed too,
greyed on a terminal, with a tab and `unavailable: <reason>` after its
name.

The registry is compiled into Sketchbook, so a script outside it reads
the registry through these JSON rows, as the plate ledger does.
`sigil.sketch.catalog(files)` and `sigil.sketch.registryRows(kind)` answer
the same rows as Python values, but they see the registry only inside
Sketchbook's own interpreter: in a plain one `registryRows` is empty and
`catalog` holds only the files it was handed.

**A capture is taken under a client's clock and a live run under the
wall's.** A session is opened for the clock policy it is drawn at, and
under any but the wall's anything a sketch measured about its own
execution is pinned, so a `--frame` can be diffed while the app and
`--bench` show the machine's own numbers. `--deterministic` names the
Advance policy and `--no-deterministic` the wall's, for either lane,
which is how a sketch's real figures are looked at in a written frame.

The app brings a device up and every set draws through it, because a
device is what runs a material's own body: the CPU mesh executor has no
compiler, so a surface reaches it as the colour the frame extracted and a
reader would be looking at a picture no recipe ever ran in. `--no-gpu`
keeps sets on that executor, which is what a plate is hashed from and
therefore what a window is worth putting beside one. A device that will
not come up is reported and the app carries on — unlike the sweep's
`--gpu`, which must fail rather than put two different pictures under one
plate's name.

**The canvas zooms without growing what it draws into.** The live view
sits in a pan-zoom pasteboard, but it is not scaled by it: the item fills
the pane, its texture is the pane's size in device pixels, and the zoom
and the pan are a view the frame is drawn through — the sketch's canvas
letterboxed, then magnified and moved, and clipped to the pane, so
everything off screen is rejected before it is drawn. A frame at 4× fills
the pane's pixels and not sixteen times them, and a wheel spin never
waits on a new texture. While the zoom is moving the sketch is drawn at
the scale it stood at and that frame is magnified to the view; it is
drawn at the view's own scale again once the zoom has held still for a
moment, or at once when the zoom has run past twice or half the held
scale. So what a sketch keeps at the scale it is drawn at — a
`graphics()` buffer, a recording traced at that scale — is formed again
at most once per doubling of a gesture rather than at every step of it.
That buffer still follows the zoom, which is why the pane's deepest zoom
is the one at which a buffer the canvas's size, on the window's screen,
fits a 16384-pixel texture edge (never less than actual size, never more
than 16×). A pointer is read back through the view, so a sketch sees
canvas units wherever the reader has moved it. It hears nothing from a
pointer off its canvas unless a press that began on it was dragged there,
and nothing from a press that began off it, wherever that press is
dragged. Orbiting a set, the wheel's
distance and the click that gives the sketch the keyboard belong to the
canvas too; off it a drag, a wheel and a touchpad scroll reach the
pasteboard. Underneath it, the sketch's cached
rasters are pinned to the screen's density rather than to the viewport's
scale (`Session::setBakeDensity`), so a generated material is baked once
and magnified through the zoom the way a bitmap the sketch loaded would
be, instead of being rasterized again at every rung of the composer's
bake ladder the gesture passes through; a pan moves the matrix and takes
no bake again either. The **Capture** action raises the
density for the photograph, so an explicitly asked-for still is written
at its own resolution rather than at the reader's. The plate ledger does
the same before the first frame it steps: it declares the plate's density
to the session it reopens, so a kept canvas or a bake formed on the way
to the capture moment is drawn on the plate's grid rather than magnified
to it.

The app is a macOS bundle, so a headless run goes through the binary
inside it:
`build/bin/<config>/Sketchbook.app/Contents/MacOS/Sketchbook`.

## `--video`: the video montage

Encodes every selected, available registry sketch into one vertical H.264
MP4. The default frame is 1080×1920 at 30 FPS, with ten output frames per
sketch. Each session is opened and advanced in fixed display-sized steps to
the moment it declared with `captureAt`; a sketch that declared no moment uses
1.5 seconds. Recording begins there, so a long entrance or loading sequence is
settled before its cut begins. Each cut is the sketch in one fixed fitted
rectangle on black with its title in white. The sketch's own animation remains
live; the montage adds no border, progress chrome, pulse, scan, or reveal wipe.

Before the first selected session opens, Sketchbook preloads the stock shader
directories through SigilIO and warms their SkSL programs concurrently. The
montage, headless sweep, capture path and live browser all cross that loading
barrier before they render, so no SkSL program is compiled inside a captured
loading frame or the first interactive frame.

The device program a draw runs through is a second compile, built per distinct
draw out of the whole inlined paint tree, and warming the SkSL does not reach
it. Every Graphite context is given a thread pool to build those programs on,
so the stages of a scene wearing a chain of effects are built beside each
other rather than one after another, and the window's canvas stands them up
before its first frame rather than inside it.

**Which programs a launch stands up is what the last launch needed.** As the
app comes up, and before any Graphite context exists, it declares every SkSL
body the effects are made of and every stock recipe's program to SigilSkia —
the precondition for a program built over one of them having a name that
survives the run. Every program the run then builds is recorded, with the key
that rebuilds it and the description it was built under, and written at exit
under the platform cache location beside the thumbnails
(`Sketchbook/pipelines/<digest>.keys`; `pipelines/` under a `--state`
root instead). The digest is over the declared bodies, in the order they were
declared, and the backend's name: an edited shader, a recipe added or a
different device is a different file rather than a set of keys describing
other programs. The next launch replays that set on a worker as soon as the
canvas's context exists, and **the canvas draws nothing until it lands** —
skipping frames rather than blocking the thread that presents, and for a
bounded number of them, because a warm-up gone wrong must be a late sketch
and not an empty one. A first frame recorded beside the warm-up would ask for
the very programs it is building, which is the stall moved rather than
removed.

**What is written back is what the run's DRAWS wanted**, not everything it
built: a program a draw asked for, whether it was built for that draw or
found standing because the replay had stood it up. A replayed program is
reported as built again, so a run writing back everything it recorded would
write back its own replay, and the set would grow into every program every
sketch ever opened here needed — each one a program a later launch stands
up before its canvas draws, whether that launch is opening that sketch or
not, and the canvas holds its frames for the warm-up only so long. A sketch
that stops being opened therefore falls out of the set rather than being
stood up forever, and a run that walked the whole registry is cut at a
ceiling no later launch should be made to warm.

**A headless sweep on the device fills a store that stands empty.** It
draws the programs an open window draws, with nobody waiting on any of
them, so it declares and records exactly as a launch does and leaves its
set behind, and the FIRST interactive open of a machine then has
something to replay rather than nothing. What that is worth follows what
was swept: a selection of a few sketches leaves those sketches'
programs, while the whole registry wants far more than the ceiling a
written set is cut at, and what survives the cut is what the run drew
first — not a set chosen for the sketch someone opens next. It never
replaces a set either: a run that drew a whole selection knows less
about what the next launch will open than a window run that drew one
sketch, so a store that already answers for this declaration keeps its
answer and the sweep's set is dropped. And it stands nothing up ahead of
itself — there is no frame to protect, and replaying would put the
store's state inside a lane whose picture has to depend on nothing but
the sketch. A sweep with no `--gpu` does not warm at all, because it
builds no device program to record. Neither does `--frame`: it
photographs a canvas on a raster surface so the picture is reproducible,
and a set is drawn by the device's own renderer, so a capture builds no
Graphite program either.

A key is replayed only if its description still reads back the same. A key
names the pieces a program is inlined out of by number, and a piece the
reading run cannot yet put a name to reads back as a hole — the backend makes
its own blur and lighting pieces on first use, so a key recorded after a draw
that made one is unreadable by a run where nothing has. Such a key is dropped
and its program is built when a draw asks for it, which is one program rather
than a walk off the end of a name.

With no set to replay — a fresh machine, an edited shader, a Skia that no
longer reads the keys — the same worker stands up the effects' own bodies as
stages instead, and only those that declare no child: a described paint is
expanded into every combination it allows, so a body with two children is
hundreds of programs a device is asked to hold and a driver that stops
compiling. That reaches a stage drawn alone; it cannot reach a STACK, because
a backend inlines a whole chain into one program and which chains a sketch
wears is not known before the sketch is read. Those are built as the draws
ask for them, and recorded, which is what makes the second launch the cheap
one. Each run says on stderr what it spent: how many programs it built for a
draw, how many it stood up ahead of one, how many draws found a program
already standing, and how many of the keys it recorded it wrote down. A run
that found a set says how much of it stood up and how much of it no longer
described what it described — a store gone wholly stale reads as a
warm-up that is neither helping nor free, and is the one state worth
seeing. A run with no Graphite behind its window says nothing at all: it
neither records nor replays, so the tally would be zeroes.
`SIGIL_SKETCHBOOK_PIPELINE_NAMES` adds one line per program, naming it, which
is how to see which program a first frame still had to build.

`--video-frames` changes each sketch's share of the edit, `--video-size`
changes the even output dimensions, `--video-bitrate` sets H.264 bits per
second, and `--fps` changes both the encoder rate and the fixed scene clock.
`--sketch` makes a one-sketch video and `--kind` limits the registry by
runtime. Hardware H.264 is preferred and OpenH264 is the fallback. Unavailable
sketches are named and skipped rather than encoded as failure cards.

`--gpu` is REQUIRED for a selection that holds a set, exactly as it is for
the sweep, and for the same reason: a set is lit by the device renderer,
so a montage that included one without a device would put a picture no
recipe ran in under that sketch's name. A selection that holds a set and
did not ask is refused, naming `--kind` as the other way out; a run that
asks and cannot have the device fails; a run whose selection needs none
brings none up.

The app's **Export video** action writes the full registry through this path.
The selected sketch's **Video** action writes a one-sketch cut; both use a
native save dialog and run the encoder in a child Sketchbook process so the
browser and its live canvas remain responsive.

## `--compare`: two directories of plates

Prints how far every plate in one directory stands from the plate of the
same name in the other, decoded and differenced channel by channel. The
comparison is a value first: `sigil::sketch::compare` answers a
`sigil::sketch::Comparison` holding one `sigil::sketch::PlateComparison`
row per plate, `sigil::sketch::printComparison` writes the rows as lines,
and Python reads the same rows as `sigil.sketch.compare(first, second)`.
Each row prints as the line its outcome opens:

```
compared <name> mean <mean> p99 <p99> max <max> clear <max> content <max>
         graze <max> <how many> composited <per composite> <how many stacked>
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

`composited` is that same `content` figure divided by how many CACHED
RASTERS were blitted over each pixel, rounded up, with the count of
content pixels that stood under more than one beside it. A cached raster
is a composite the picture beside it did not make and every composite
rounds, so a difference of four under four of them is the same fact as a
difference of one under one — and a caller whose tolerance is a bound per
composite reads this rather than `content`. The counts come from a plane
the SECOND directory carries beside its plates, named `counts_<sketch>`
and written by a headless sweep asked for `--composites`; with no plane
there every pixel stands under one composite and `composited` is
`content`.

It opens no sketch, needs no fonts, no assets and no device, and it
JUDGES NOTHING — how close is close enough is a tolerance about a
machine, which is the plate ledger's to hold. The ledger's device and
promotion tiers are the callers: each renders two directories of plates
in one run and reads the rows, and `sigil.py plates compare <dir-a>
<dir-b>` prints the same lines as this flag.

## `--frame`: the asset workflow

Steps the clock at `--fps` (default 60) to the moment the sketch
declared with `ctx.captureAt`, then captures `--frames` PNGs
(sequences number as `out_0001.png…`) at
`--scale` (default 1: captures match the declared canvas pixel for
pixel, which is what asset generation wants). Declare the exact canvas,
give it a transparent background, draw, export. Any sketch answers to the
flag, so the sketch that draws the asset is the template.

**`--gpu` puts the run on the device**, exactly as it does for a sweep: a
set draws its frame there, and a canvas sketch's mesh painter
(`sketch::painterRuntime()`) rasterises there. It is fatal when no device
comes up, because a run that asked for the device and quietly gave the
CPU's picture puts two different pictures under one name. Without it a
file renders on the CPU mesh executor, which is what a plate is hashed
from.

**The moment is the sketch's, not the flag's.** `--at <sec>` overrides
it, and a sketch that declared none falls back to 1.5 s; otherwise a
still uses the same declared moment as the plate sweep. The line it prints
says which of the three it used. A declared zero runs one update without
advancing time; a moment between fixed steps uses a final fractional step.
A fraction of a pixel at the requested scale is dropped, as the sweep
drops it (`sigil::sketch::plateExtent`).

**A written still IS the sweep's plate of its moment.** Every still is
taken through the runtime's own `Session::still` — `Host::photograph` —
so `--frame --at 1 --scale 2`, the sweep's plate at `--at 1` on the same
grid, a protocol session stepped a second and photographed at density 2,
and `sigil.sketch.render_file` are one picture, byte for byte. A canvas
sketch re-renders its still, which draws one frame more: the picture is
the scene one sixtieth of a second past the moment named, and the sketch
ran one more update to reach it. For the same reason a sequence of
`--frames` faster than that step is spaced at the step.
`--bench` keeps the 1.5 s default
whatever the sketch declared: its `--at` is a warm-up that has only to
get programs, bakes and atlases hot, and pinning it keeps the measured
run the same run for every sketch.

`--fps` sets the PRE-ROLL step as well as the capture rate. Steps longer
than the session clock's maximum delta are subdivided so the clock reaches
the requested moment. A sketch
using a fixed-rate steppable has a catch-up clamp, so pre-rolling far
below its own rate discards simulated time and lands earlier than you
asked for. Keep `--fps` near the rate you would actually draw at.

## `--bench`: the 60 FPS gate

One machine-readable line, prefixed `BENCH` so a collector can find it,
carrying the sketch, its canvas, the frame count, the step regime, the
percentiles and a verdict — then a human line naming which phase
dominated, and the runtime's own lanes under it.

The gate is **p99 under 16.6 ms** — a sustained 60 FPS at the sketch's
own declared canvas size. It exits 0 whenever it measured: the verdict is
the output, not the exit status, so a slow sketch can sit in a pipeline.
A sketch that never built, or a surface that could not be allocated,
exits 1.

**A sketch that declares `ctx.plate()` is judged on its capture cost, not
on 60 FPS.** Some sketches are plates rather than live scenes: a large
sheet over an expensive material stack whose subject is the sheet's own
size. A canvas the sketch cannot present at is a different statement from
a live sketch that drops frames, so a marked sketch reports the cost of
the still it is photographed as and the verdict reads `PLATE` rather than
`PASS`/`FAIL`. It is never a timeout override, and it changes nothing
about the plate sweep — only what the interactive gate asserts.
`chaucer_astrolabe` is one.

What it does, and why it is not `--frame`'s numbers: the capture path
steps the clock on a tiny scratch surface where every draw is clipped
away, so a sketch whose whole cost is one full-canvas shader reads as
free. `--bench` allocates the real canvas, warms it to `--at` so
programs, bakes, snapshots and glyph atlases are hot, then times real
frames. On a failure it prints the most expensive nodes with how each
produced its pixels, and under any expensive one that is not a bake, a
line saying **why** — because each refusal to bake is individually
correct and individually invisible.

`--jitter-dt` steps a varying frame interval instead of the fixed one. A
fixed step is not a neutral simplification for anything that memoizes on
a per-frame value: under it the values a scene visits repeat on the
scene's own period, so a cost that grows per distinct value reads as
free. A wall-clock host never revisits a value. The sequence is a
golden-ratio rotation — irrational, so it never repeats a step, and
deterministic, so two runs measure the same frames.

## `--window-bench`: the same frames, in the real window

```sh
Sketchbook --window-bench [<sec>] [--window-size <WxH>] [--window-scale <n>]
           [--sketch <name>] [--kind canvas|set]
```

Opens the window at a stated size and device pixel ratio, presents each
selected sketch for a stretch after a warm-up, and prints one
machine-readable line each — prefixed `WINDOW`, the way `--bench`
prefixes `BENCH` — carrying the presented rate, the frame's work mean
and p99, its paint phase, the submit, and the headroom the work alone
would allow. A sketch this machine cannot run is named `SKIPPED` with
what is missing, and no line is written for it.

**Each row is the sketch that was on screen.** Selecting a sketch is an
ask: the session opens on the render thread and its first frame — the
program compiles, the texture bakes, the glyph atlases — can cost
seconds. So the warm-up starts at the first frame of the selection's own
session and not at the ask, and the rolling windows the readout comes
from are emptied where the measured stretch begins, so a row is that
sketch's frames over that stretch and carries nothing of what opening it
cost. The rate is the whole stretch — the frames that reached the screen
over the time they took — so a hitch inside it weighs what it was, while
the panel's own readout beside the canvas stays the short rolling one a
reader watches change. A selection that does not reach the screen within
the ceiling is named `SKIPPED` with how long it was waited for, as is a
stretch that ended with all but no frames in it; a run that stood any
sketch down that way exits non-zero, because a rate it could not take is
not a rate of zero.

**Nothing else runs inside a measurement.** The store's still is not
written while the lane is measuring, and the window keeps one session at
a time — the one on screen goes as the next opens, rather than standing
warm behind it and being let go in the middle of a later sketch's
frames. So what a sketch reads in a sweep is what it reads presented
alone, which is the only way a row means anything on its own.

**It measures what `--bench` cannot.** The gate renders onto a raster
surface at the sketch's declared size and presents nothing, which is
what makes it a gate: the sketch's own cost, isolated. Here the frame is
drawn through the surface the window presents, at the window's pixels
and its device pixel ratio, and the numbers carry the host's own
overhead with them — the submit or texture upload that puts the frame on
screen, and, for a set drawn on a device, the readback and blit its
paint phase performs. Selection goes through the same property a click
sets, so a switch takes the path a reader's click takes.

A presented rate is bounded by the compositor, which means by the
display: a sketch comfortably inside its budget reads at the refresh
rate and says nothing more. The interesting rows are the ones BELOW it,
and the work beside them says how much of that frame was the sketch.

`scripts/sigil.py bench --lane fps` drives it over the registry and judges each
presented rate against `bench/app_fps_<config>.json` within a stated
band, `--rebase` adopting. The baseline is per machine AND per display
mode, so it records the window size and scale it was taken at and the
run says so when they differ.
