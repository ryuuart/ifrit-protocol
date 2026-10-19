# Running Grimoire

How Grimoire is run: the window, the files and folders it opens, and every
lane its command line drives without one. What a sketch is and how a host
compiles it are [SigilSketch's](../spell-circle-canvas/src/sketch/README.md);
[README.md](README.md) beside this file says how Grimoire is built.

```sh
Grimoire [--no-gpu]                       # the app; --no-gpu lists no scene3d sketch
Grimoire --sketch <name>                  # the app, on that one
Grimoire <file.cpp>                       # the app, on that file
Grimoire --workspace <directory>          # the app, on a folder of sketches
Grimoire --examples                       # the app, on the bundled catalogue
Grimoire --list [--kind scene2d|scene3d] [--gpu]
                                          # the registry, one per line
Grimoire --catalog [<file.cpp>] [--gpu]   # the browser's rows, one JSON each
Grimoire <file.cpp> --frame out.png [--at <sec>] [--scale <n>] [--gpu]
                                  [--frames <count>] [--fps <n>]
                                  [--deterministic | --no-deterministic]
Grimoire --headless [<outdir>] [--gpu] [--sketch <name>] [--kind <k>]
           [--at <sec>] [--scale <n>] [--ledger]
Grimoire --video out.mp4 [--video-frames <n>] [--video-size <WxH>]
           [--video-bitrate <bits>] [--fps <n>] [--sketch <name>]
           [--kind <k>] [--gpu]
Grimoire --compare <dir-a> <dir-b>        # two sweeps' plates, differenced
Grimoire [<file.cpp>] --window-bench [<sec>] [--window-size <WxH>]
           [--window-scale <n>] [--sketch <name>] [--kind <k>]
Grimoire --thumbnails [--sketch <name>] [--kind scene2d|scene3d]
           [--thumbnail-budget <sec>] [--thumbnail-heavy]
Grimoire --publish [<name>] [--sketch <name>]
                                            # the window's frames, live, to
                                            # other applications
Grimoire --shot <png> [--sketch <name>] [--at <sec>]
                                            # the whole window, once live
… [--assets <dir>]                          # what mounts at res://
… [--state <dir>]                           # where the run keeps what it
                                            # writes for a later run
… [--pipeline-names]                        # each device program named on
                                            # stderr as it is built
… [--inspect[=<port>]]                      # the protocol's endpoint: the
                                            # window mounts it unasked, a
                                            # sweep when asked
Grimoire --headless --inspect[=<port>] [--state <dir>]
                                            # no window: a host a client
                                            # drives over the protocol
```

A file opened by path — a `.cpp` entry or a `.py` module, anywhere on
disk — is compiled or imported by this host and swapped in on every save;
a build that fails keeps the running session. [SigilSketch's host
chapter](../spell-circle-canvas/src/sketch/HOST.md) says how a sketch
reaches a host, what binds a C++ file to one host build, and the workspace
a folder of such files makes.

`--sketch` takes a case-insensitive substring and answers to a sketch's
filed name or its file stem, which is the loop for visual iteration.
`--headless` writes its plates into `sketch_plates/` when no directory
follows it — unless `--inspect` is given and no sketch or kind is named,
when it serves the protocol instead. [SigilSketch's protocol
chapter](../spell-circle-canvas/src/sketch/PROTOCOL.md) says what a client
driving a sketch host is answered — the agents and the harness a test
drives one through — and [What Grimoire mounts](#what-grimoire-mounts)
below what each lane here mounts.

`--frame` and `--headless` with `--gpu` draw scene2d stills through
Graphite and scene3d sketches through the World device executor. A missing
device context fails instead of producing a raster substitute.

**A scene3d sketch is drawn on a device and nowhere else.** Every lane
that draws one — `--frame`, `--headless`, `--video` — needs
`--gpu`. Without it no device comes up and every scene3d sketch is
unavailable with one reason, the one SigilSketch writes: a still or a
measurement of one refuses with it, a sweep skips each one and prints it,
`--video` refuses a selection that holds one, and `--list` and `--catalog`
grey every scene3d sketch with it — which is why both take `--gpu` too, to
list the scene3d sketches a device can draw. `--thumbnails` never brings a
device up, so it notes every scene3d sketch with the same reason and the
window's refresh takes a scene3d sketch's still once it is opened.

For a headless sweep, `--scale <n>` sets device pixels per canvas unit on
both CPU and GPU. It overrides the sketch's oversample and the host's width
ceiling, and applies from the first frame to baking and to held 3D frames.
Output dimensions are the declared canvas dimensions times that density,
with fractional pixels dropped. Without the flag, the sweep uses a declared
oversample when present; otherwise it uses the runtime's oversample within
the width ceiling, never below one.

**`--state <dir>` is the one place a run keeps what it writes for a later
run.** The builds of sketch files, the browser's thumbnails, the device
programs a run recorded and the surface programs it compiled, the settings and the recent workspaces each take
a directory or a file under it — `builds/`, `thumbnails/`, `pipelines/`,
`settings/` — instead of the platform's own locations, and a window or a
command the run starts is handed the same root. A test or a script that
names a fresh root reads nothing an earlier run left and leaves nothing
behind; `sigil::sketch::setStateDirectory` is the same root for a C++
host that is not Grimoire, and `sigil::sketch::stateLocation` answers
where one kind of state stands under it. Python does not bind it, so a
sketch built from a Python process keeps its builds in the platform cache
location. Nothing turns the build cache off; a run that must not reuse an
earlier build names a fresh root.
`--shot <png>` captures the app window rather than a sketch, which is
the only way to look at the browser and the inspector. `--at <sec>` in a
window run holds the canvas at that moment: each sketch the window
presents is stepped there as a still is, then drawn again on every frame
with its clock standing, and the run prints the time it holds — so a shot
at `--at 0.25` shows the scene `--frame --at 0.25` writes, and
`--window-bench` keeps the wall's clock it measures.

**`--publish` offers the sketch's own canvas to other applications.**
A program on this machine subscribes to a name and receives every frame
this canvas draws, composited live in its own scene — a VJ program, a
projection mapper, a recorder. The name is the one given, or the stem of
the sketch the run opens on; a subscriber binds to it, so it is the
run's and does not follow the sketch on screen. The status bar's **Publish**
button and Ctrl-P invoke the same action. A normal launch without `--publish`
starts with publishing off and uses **Grimoire** when enabled. The status
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

Any Syphon client checks it: `sigil::io::frames::publications()` and a
client's own list say what is being offered under what name, and a second
Grimoire running `guest_picture`, below, wears it.

**Frames come the other way too**: a sketch wears another application's
publication through `sigil::sketch::Guest`, which [SigilSketch's
README](../spell-circle-canvas/src/sketch/README.md) defines;
`guest_picture` is the page that wears one and `guest_body` the scene3d
sketch that turns one under a light.

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

The registry is compiled into Grimoire, so a script outside it reads
the registry through these JSON rows, as the plate ledger does.
`sigil.sketch.catalog(files)` and `sigil.sketch.registryRows(kind)` answer
the same rows as Python values, but they see the registry only inside
Grimoire's own interpreter: in a plain one `registryRows` is empty and
`catalog` holds only the files it was handed.

**A capture is taken under a client's clock and a live run under the
wall's.** A session is opened for the clock policy it is drawn at, and
under any but the wall's anything a sketch measured about its own
execution is pinned, so a `--frame` can be diffed while the app shows
the machine's own numbers. `--deterministic` names the Advance policy
and `--no-deterministic` the wall's, which is how a sketch's real
figures are looked at in a written frame.

The app brings a device up and every scene3d sketch draws through it.
`--no-gpu` brings none up, and every scene3d sketch is listed unavailable; a device that will
not come up is reported and the app carries on the same way — unlike the
sweep's `--gpu`, which fails the run. On a Mac the window hands that
device its own Metal queue and Graphite context, so the pages a sketch
paints are painted where the window draws: one context for both, and a
page laid over the canvas is drawn as it stands rather than read back.

**The canvas zooms without growing what it draws into.** The live view
sits in a pan-zoom pasteboard, but it is not scaled by it: the item fills
the pane, its texture is the pane's size in device pixels, and the zoom
and the pan are a view the frame is drawn through — the sketch's canvas
letterboxed, then magnified and moved, and clipped to the pane, so
everything off screen is rejected before it is drawn. A frame at 4× fills
the pane's pixels and not sixteen times them, and a wheel spin never
waits on a new texture. A live scene3d sketch forms no more pixels than
its declared canvas whatever the pane's density or zoom, and the pane
magnifies that picture; a capture forms every pixel the canvas has. While the zoom is moving the sketch is drawn at
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
dragged. Orbiting a scene3d sketch, the wheel's
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
inside it, which the library tree's build writes as
`apps/spell-circle-canvas/build/bin/<config>/Grimoire.app/Contents/MacOS/Grimoire`.

## What Grimoire mounts

Grimoire's command line carries two protocol flags: `--inspect`, with
an optional `=PORT`, and `--state` with a directory.

- **The window** mounts an endpoint on loopback whether or not it was
  asked — any free port, or the one named — answering `host` and
  `registry`, with `host.describe` listing the session the window shows.
  Its event loop dispatches it; with no client attached a dispatch runs no
  handler. The window's frames are its render thread's, so the session
  and clock domains are not mounted there.
- **`Grimoire --headless --inspect`**, naming no plate directory, no
  sketch and no kind, is a host a client drives: no window and no plates,
  the three agents mounted, the address written to
  `<state>/protocol-address` before the first frame, and a loop at the
  window's rate until an interrupt or a termination signal.
- **A sweep** with `--inspect` answers `host` and `registry` between its
  sketches; nothing a plate holds depends on it.
- **Every other lane refuses `--inspect`**: it ends once its output is
  written, and a client would have nothing to drive.

The state root is `--state`, or the platform's own location for
Grimoire. `--deterministic` and `--no-deterministic` name the clock
policy a written still's session is opened for — Advance or the wall's —
and a still is taken under Advance, a measurement under the wall's clock,
when neither is given.

## `--video`: the video montage

Encodes every selected, available registry sketch into one vertical H.264
MP4. The default frame is 1080×1920 at 30 FPS, with ten output frames per
sketch. Each session is opened and advanced in fixed display-sized steps to
the moment it declared with `captureAt`; a sketch that declared no moment uses
1.5 seconds. Recording begins there, so a long entrance or loading sequence is
settled before its cut begins. Each cut is the sketch in one fixed fitted
rectangle on black with its title in white. The sketch's own animation remains
live; the montage adds no border, progress chrome, pulse, scan, or reveal wipe.

Before the first selected session opens, Grimoire preloads the stock shader
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

**Which programs a sketch stands up is what its draws needed last time.**
As the app comes up, and before any Graphite context exists, it declares
every SkSL body the effects are made of and every stock recipe's program to
SigilSkia — the precondition for a program built over one of them having a
name that survives the run. Every program the run then builds is recorded,
with the key that rebuilds it and the description it was built under, and
every program a draw asks for is written down under the sketch drawing it —
by the name the host knows it by before opening it, a registry key or a
file's path. At exit each sketch's set is written under the platform cache
location beside the thumbnails (`Grimoire/pipelines/<digest>-<sketch
digest>.keys`; `pipelines/` under a `--state` root instead). The first
digest is over the declared bodies, in the order they were declared, and the
backend's name: an edited shader, a recipe added or a different device is a
different file rather than a set of keys describing other programs.

**A selected sketch's set stands up before the sketch opens.** The host's
name for the sketch is all the warm-up needs: the set holds every program
its draws wanted, the chain of a stack inlined into theirs, so nothing
about the sketch is read first. It is replayed on a worker, once per sketch
per context, and **the canvas draws nothing until it lands** — skipping
frames rather than blocking the thread that presents, and for a bounded
number of them, because a warm-up gone wrong must be a late sketch and not
an empty one. A first frame recorded beside the warm-up would ask for the
very programs it is building, which is the stall moved rather than removed.

**What is written back is what each sketch's DRAWS wanted**, not
everything the run built: a program a draw asked for, whether it was built
for that draw or found standing because the replay had stood it up. A
replayed program is reported as built again, so a run writing back
everything it recorded would write back its own replay, and a sketch's set
would grow into every program every sketch ever opened here needed. Kept
per sketch, a set is the work in front of that sketch alone, so none is
ever cut to a size, and a sketch that stops being opened costs nothing to
any other.

**A headless sweep on the device fills a sketch's set where none
stands.** It draws the programs an open window draws, with nobody waiting
on any of them, so it declares and records exactly as a launch does and
leaves each swept sketch's set behind, and the FIRST interactive open of
that sketch then has something to replay rather than nothing. It never
replaces a set: a capture draws a sketch at its moment, which is less than
a window that showed it draws, so a sketch that already has a set keeps it
and the sweep's is dropped. And it stands nothing up ahead of itself —
there is no frame to protect, and replaying would put the store's state
inside a lane whose picture has to depend on nothing but the sketch. A
sweep with no `--gpu` does not warm at all, because it builds no device
program to record. Neither does `--frame`: it photographs a scene2d sketch
on a raster surface so the picture is reproducible, and a scene3d sketch is
drawn by the device's own renderer, so a capture builds no Graphite program
either.

A key is replayed only if its description still reads back the same. A key
names the pieces a program is inlined out of by number, and a piece the
reading run cannot yet put a name to reads back as a hole — the backend makes
its own blur and lighting pieces on first use, so a key recorded after a draw
that made one is unreadable by a run where nothing has. Such a key is dropped
and its program is built when a draw asks for it, which is one program rather
than a walk off the end of a name.

With no set written down for this declaration at all — a fresh machine, an
edited shader, a Skia that no longer reads the keys — a worker stands up the
effects' own bodies as stages as soon as the canvas's context exists, and
only those that declare no child: a described paint is expanded into every
combination it allows, so a body with two children is hundreds of programs a
device is asked to hold and a driver that stops compiling. A stack's chain
is built as the draws ask for it, and recorded under the sketch, which is
what makes that sketch's second open the cheap one. Each run says on stderr
what it spent: how many programs it built for a draw, how many it stood up
ahead of one, how many draws found a program already standing, and how many
of the keys it recorded it wrote down; and for each sketch it opens, how many
programs its first frame's draws had to build. A sketch whose set was found
says how much of it stood up and how much of it no longer described what it
described — a set gone wholly stale reads as a warm-up that is neither
helping nor free, and is the one state worth seeing. A run with no Graphite
behind its window says nothing at all: it neither records nor replays, so
the tally would be zeroes.
`--pipeline-names` adds one line per program, naming it, which is how to
see which program a first frame still had to build.

`--video-frames` changes each sketch's share of the edit, `--video-size`
changes the even output dimensions, `--video-bitrate` sets H.264 bits per
second, and `--fps` changes both the encoder rate and the fixed scene clock.
`--sketch` makes a one-sketch video and `--kind` limits the registry by
runtime. A file opened by path or a `--workspace` is not in the registry, so
`--video` beside either is refused while the arguments are read, before any
device or session opens; a file's frames are photographed with `--frame` and
`--frames`. Hardware H.264 is preferred and OpenH264 is the fallback. Unavailable
sketches are named and skipped rather than encoded as failure cards.

`--gpu` is REQUIRED for a selection that holds a scene3d sketch, exactly as
it is for the sweep, and for the same reason: a scene3d sketch is lit by
the device renderer,
so a montage that included one without a device would put a picture no
recipe ran in under that sketch's name. A selection that holds a scene3d
sketch and did not ask is refused, naming `--kind` as the other way out; a run that
asks and cannot have the device fails; a run whose selection needs none
brings none up.

The app's **Export video** action writes the full registry through this path.
The selected sketch's **Video** action writes a one-sketch cut; both use a
native save dialog and run the encoder in a child Grimoire process so the
browser and its live canvas remain responsive.

## `--compare`: two directories of plates

Prints how far every plate in one directory stands from the plate of the
same name in the other, one line per plate, and judges nothing — how close
is close enough is the plate ledger's to hold. The rows, what each figure
measures and the lines they print as are `sigil::sketch::compare`'s, which
[SigilSketch's registry chapter](../spell-circle-canvas/src/sketch/REGISTRY.md)
defines; `sigil.py plates compare <dir-a> <dir-b>` prints the same lines as
this flag. It opens no sketch, needs no fonts, no assets and no device.

## `--frame`: the asset workflow

Steps the clock at `--fps` (default 60) to the moment the sketch
declared with `ctx.captureAt`, then captures `--frames` PNGs
(sequences number as `out_0001.png…`) at
`--scale` (default 1: captures match the declared canvas pixel for
pixel, which is what asset generation wants). Declare the exact canvas,
give it a transparent background, draw, export. Any sketch answers to the
flag, so the sketch that draws the asset is the template.

**`--gpu` puts the run on the device**, exactly as it does for a sweep: a
scene3d sketch draws its frame there, and so does a scene2d sketch's
bake. It is fatal when no device
comes up, because a run that asked for the device and quietly gave the
CPU's picture puts two different pictures under one name. Without it a
scene2d sketch renders on the CPU, which is what its plate is hashed from,
and a scene3d sketch is refused: it is drawn on a device and nowhere else.

**The moment is the sketch's, not the flag's.** `--at <sec>` overrides
it, and a sketch that declared none falls back to 1.5 s; otherwise a
still uses the same declared moment as the plate sweep. The line it prints
says which of the three it used. A declared zero runs one update without
advancing time; a moment between fixed steps uses a final fractional step.
A fraction of a pixel at the requested scale is dropped, as the sweep
drops it (`sigil::sketch::plateExtent`).

**A written still IS the sweep's plate of its moment.** Every still is
taken through the runtime's own `Session::still` — `Host::photograph` —
so `--frame --at 1 --scale 2`, the sweep's plate at `--at 1 --scale 2`,
a protocol session stepped a second and photographed at density 2,
and `sigil.sketch.renderFile` are one picture, byte for byte. A scene2d
sketch re-renders its still in one more frame of no length: the picture is
the scene at the moment named, the sketch ran one more update at that
moment to draw it, and the printed `t=` is the time the pixels show. A
sequence of `--frames` is spaced at `1/--fps` exactly, and each of its
frames is printed with the time it shows and the file it went to; the
`backend:` line names the surface every frame was drawn on, Graphite under
`--gpu` and the CPU raster otherwise.

`--fps` sets the PRE-ROLL step as well as the capture rate. Steps longer
than the session clock's maximum delta are subdivided so the clock reaches
the requested moment. A sketch
using a fixed-rate steppable has a catch-up clamp, so pre-rolling far
below its own rate discards simulated time and lands earlier than you
asked for. Keep `--fps` near the rate you would actually draw at.

## `--window-bench`: each sketch, in the real window

```sh
Grimoire [<file.cpp>] --window-bench [<sec>] [--window-size <WxH>]
           [--window-scale <n>] [--sketch <name>] [--kind scene2d|scene3d]
```

Opens the window at a stated size and device pixel ratio, presents each
selected sketch — or the file named, from its live session — for a
stretch after a warm-up, and prints one machine-readable line each,
prefixed `WINDOW` so a collector can find it, carrying the presented rate, the frame's work mean
and p99, its paint phase, the submit, and the headroom the work alone
would allow; then what opening the sketch cost, kept out of every
number before it: `open`, the seconds from the ask to the first frame
of its session on screen, and `warmMax`, the slowest frame of the
warm-up that followed. Last come the conditions the row was taken
under: `power`, what powers the machine (`AC`, `battery` or `unknown`),
and `gpuBusy`, the GPU's utilization as its driver reports it when the
row closes. Neither says the performance state the GPU is clocked at —
no public interface reports it, and a laptop on battery and idle at the
keyboard holds the GPU in its lowest one, which reads as a slower rate
in the same binary — so rows are compared only when both were taken
under the same power, back to back.

**Where the device's time went.** A scene3d sketch's row ends with `gpu=`: the
device's own time for each stage of the frame, in milliseconds, each the
mean over the measured stretch, as `frame:<ms>,<stage>:<ms>,…` — the
whole frame first, then the stages in the order they ran. A geometry
pass's stages are its name followed by `.shadow` (the shadow maps it
drew), `.laid` (the clear, the sky and every body laid over),
`.colour-copy` and `.mips` (the scene copied, with its smaller levels,
for a body that sees through itself), `.depth-copy` (the depth copied
for one that fades into the scene), `.seen`, `.blended` and `.extras`
(stamps, a variant, coverage); a post pass is its own name; `present` is
the final stage the window's picture leaves through. The stages are
timed with the device's timestamps only over the measured stretch, so
the rest of the run writes none; the stage that dominates `frame` is the
one to cut. `gpu=unavailable` is a device that writes no timestamps, and
a scene2d sketch, which has no device stages, carries no field. A sketch
this machine cannot run is named `SKIPPED` with what is missing, and no
other line is written for it.

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

**It measures the frame a reader waits on.** The frame is drawn through
the surface the window presents, at the window's pixels and its device
pixel ratio, and the numbers carry the host's own overhead with them —
the submit or texture upload that puts the frame on screen, and, for a
scene3d sketch drawn on a device, the readback and blit its paint phase
performs. Selection goes through the same property a click sets, so a
switch takes the path a reader's click takes.

The window declares the process user-facing to the system while any of
it is on screen, in front of other applications or behind them, so a
stretch presented behind another application is not demoted to the
background tier part-way through; a window hidden, minimised or wholly
covered stops declaring it, unless it is publishing.

A presented rate is bounded by the compositor, which means by the
display: a sketch comfortably inside its budget reads at the refresh
rate and says nothing more. The interesting rows are the ones BELOW it,
and the work beside them says how much of that frame was the sketch.

`scripts/sigil.py bench --lane fps` drives it over the registry and judges each
presented rate against `bench/app_fps_<config>.json` within a stated
band, `--rebase` adopting. The baseline is per machine AND per display
mode, so it records the window size and scale it was taken at and the
run says so when they differ.
