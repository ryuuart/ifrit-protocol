# Sketches in Python — the plan

A plan for work that is not yet in the tree. `README.md` beside this
file is the canon for what a sketch IS, and stays so until each part of
this lands. This page says where a reload's seconds go, how a sketch
written in Python fits the seams the live host already has, what the
compose vocabulary spells in Python, and which decisions are still
open. No probe checks it, because it names things that do not exist
yet.

## Where a reload's seconds go

Saving a `.cpp` sketch runs a fresh compiler process with the flags
lifted from `sketches/Anchor.cpp`'s entry in the compilation database,
which is the Release configuration's whole line: its optimization
level, every include directory of every library a sketch may reach, and
the vcpkg tree behind them. A sketch includes
`<sigilsketch/canvas/Sketch.h>`, and that header brings the whole of
compose with it — Skia, Yoga, Boost and choreograph included. Parsing
and instantiating that surface is the floor cost of every build, and it
is the same for a fifty-line sketch as for a five-hundred-line one. No
precompiled header stands anywhere in the tree.

| step, per save | what it costs |
| --- | --- |
| parse and instantiate the include surface | the floor, and most of the time |
| generate code at the Release level for everything instantiated | the second share |
| link, `dlopen`, then `setup()` on the render thread | small, though the setup is a visible hitch |
| the poll, the sibling scan, the skew guard's walk | milliseconds |

What is NOT the problem is the watch. The entry is stamped every frame,
the directories beside it on a short cadence, and the skew guard walks
the framework's public headers once per build start. A save is noticed
within a frame; it is the compile after it that takes the seconds.

### The shorter path that is not this plan

Two changes to the C++ path alone would take most of the compile out of
a save, and neither is this plan:

- a precompiled header of the two includes `Anchor.cpp` names, written
  by the same target that lifts the flags, and `-include-pch` on the
  guest's compile line;
- a lower optimization level for LIVE builds only, since a window is
  not diffed.

`--frame`, `--bench` and the sweep keep the captured flags: a different
optimization level can move floating point, and a plate's bytes with
it. Both changes still leave a compile on every save, which is the
thing a Python sketch removes.

## A Python sketch is a canvas body

The seam already states the shape. `Kind` is a value that opens a
`Session`; every host — the window, `--frame`, `--bench`, the sweep, the
thumbnail warm — drives a `Session` and never learns which runtime it
holds. A canvas session owns the composer, the ticker, the clock,
capture, the still, `redeclare()` and the deterministic flag, and
drives its body through `CanvasBody`, the one vtable a sketch has:
`setup(ctx)` and `update(elapsed, ctx)`.

A Python sketch is therefore not a third runtime. It is a canvas sketch
whose body is Python:

- `PythonBody`, behind `CanvasBody`, holding the instance the file's
  class was constructed into and calling its `setup` and `update`;
- a kind that opens a canvas session on such a body, wrapped so that
  the interpreter is held for the whole of `frame`, `repaint`, `still`
  and `redeclare` — the body's own calls are only part of what reaches
  Python, since pen programs, memo functions and steppables are called
  from inside `Composer::draw` on the same thread;
- the bindings, one module per library, compiled into Sketchbook.

Nothing above the seam changes. The window letterboxes, clears,
photographs and times a Python session exactly as it does a C++ one.

### The file

A class spelling the two names the C++ concept spells, and no globals —
the pen's own stance, that nothing in a process is global, holds here
too:

```python
# hello.py — TAGS: Draw/Starter
from sigil.compose import graphics

class Hello:
    x, y, vx, vy = 200, 100, 3, 2

    def setup(self, ctx):
        ctx.canvas(400, 300)
        ctx.background((0.078, 0.078, 0.078))
        ctx.captureAt(3.0)
        ctx.composer.render(graphics("hello.ball", self.draw))

    def draw(self, pen):
        pen.background(20, 30)
        self.x += self.vx
        if self.x < 20 or self.x > pen.width - 20:
            self.vx = -self.vx
        pen.fill(255, 120, 80)
        pen.circle(self.x, self.y, 40)
```

A body names the parameters it reads, as a C++ body does: `setup()`
beside `setup(ctx)`, `update(elapsed)` beside `update(elapsed, ctx)`,
`draw(pen)` beside `draw(pen, ctx)`. The prefix rule is one
`inspect.signature` read when the body or the program is built, not a
search at every call.

`ctx` hands out its long-lived members — `composer`, `ticker`,
`assets` — as the objects they are, and snapshots the rest. The C++
context is per-frame and non-copyable so that a closure cannot capture
it; the Python one is built so that a closure holding it holds nothing
that dangles.

### The reload

The host's loop is watch, build, adopt, and keep the last good session
while a build is broken. Only BUILD changes, chosen by the entry's
extension:

| entry | build | adopt |
| --- | --- | --- |
| `.cpp` | the compiler, as today: stale units apart, one link | `dlopen`, the ABI check, `sigilSketchEntry` |
| `.py` | import the file into a fresh module, find the body class | construct it, open a canvas session on it |

A failed import or a `setup()` that raises is a traceback in the overlay
with the last good session still running — what a compile error is
today. Generation numbers, the status line, `restartSession`, capture,
the metrics and residency are untouched; a `.py` is keyed by its path
like any other file.

Two places assume `.cpp` and learn `.py`: the argument parser, which
takes a path only when it ends in `.cpp`, and the source helpers that
find a sketch's units and headers. A directory sketch in Python is a
package; the first cut watches the entry and the `.py` files beside it.

### What holds across the seam

- **Determinism.** A capture that will be diffed is deterministic only
  if the sketch draws from the pen's seeded streams and the noise
  field, never from Python's `random`. `ctx.measured()` pins as it does
  now.
- **Exceptions in `update` or in a program.** The first is logged with
  its traceback and the status reads failed until the next save; the
  session keeps presenting, since a kept canvas still holds its last
  frame. A fault in the interpreter itself is the crash reporter's, as
  any fault in a guest is.
- **The sweep.** Sessions on several threads serialize on the
  interpreter lock. A sweep over Python sketches is slower per job and
  no less correct.
- **The registry.** A `.py` cannot be compiled in, so it joins the
  catalog as a file opened by path does — the Workspace group — until
  the sweep learns to walk the sketches folder for them.

## What compose spells in Python

Compose is the same fluent chain, verb for verb. `Element` is a
copy-on-write value, every setter returns the node, and every place the
kernel takes something opaque it takes it through a type-erased seam —
`detail::makeMemo` over a `std::any`, `detail::makeLayout` over an
erased placement function, `PenProgram` over the callable holder that
drops the parameters a body does not name. Those seams are the doors a
binding reaches through, so compose itself changes nothing for the
first surface.

### The same tree

The meter and the dashboard from compose's own README:

```python
from dataclasses import dataclass
from sigil.compose import box, text, memo, Fill, Align, hexColor, spans, stroke
from sigil.motion import animate, to, Transition
from sigil.weave import textStyle

@dataclass(frozen=True)
class Channel:
    id: str
    label: str
    level: float = 0.0
    alarm: bool = False

def meter(c: Channel):
    ink = hexColor(0xff5252) if c.alarm else hexColor(0x8fd0ff)
    return (box().row().gap(10).padding(12).corners(6)
        .fill(hexColor(0x0e1218)).alignItems(Align.Center)
        .stroke(spans.corners(12), stroke(1.5, Fill.color(ink)))
        .children([
            text(c.label, textStyle(size=13, color=ink)),
            box().grow().height(6).fill(ink).transformOrigin(0, 0.5)
                .scaleX(animate(to(c.level), Transition(duration=220))),
        ]))

def dashboard(channels):
    return (box().column().gap(8).padding(24).fill(hexColor(0x05070a))
        .children([memo(c, meter).key(c.id) for c in channels]))
```

The frozen dataclass is the whole memo contract — copyable and
equality-comparable, with `==` the comparator — and the comprehension is
`each`. A specimen sheet, with the theme's dynamic scope as a `with`
block and a pen program where a canvas program stood:

```python
from sigil.sketch import kit
from sigil.compose import pen, Fill, Cache

class CrossingRuleSheet:
    def cell(self, key, strands, rule, call, note):
        return kit.caption(CELL, call, note,
            pen(key, lambda p: paint_weave(p, strands, rule), Cache.Auto)
                .width(CELL).height(CELL)
                .fill(Fill.color(kit.theme().palette.cellGround)))

    def setup(self, ctx):
        with kit.provide(sheet_theme()):
            kit.stage(ctx, size=(1120, 736), captureAt=0.05)
            stars = kit.cells(cells=[self.cell(...), self.cell(...)], gap=14)
            loops = kit.cells(cells=[self.cell(...), self.cell(...)], gap=14)
            ctx.composer.render(kit.page(
                kit.Page(title="CROSSING RULE",
                         footer="a knot is decided, never drawn in order"),
                kit.cells(cells=[stars, loops], column=True, gap=18)))
```

Live values, as the card flip sketch binds them: outputs are
attributes, a steppable is a function, and `bind` chains as it does
now.

```python
from sigil.compose import box
from sigil.motion import Output, bind, phase

class CardFlip:
    def __init__(self):
        self.flip = Output(0.0)

    def setup(self, ctx):
        ticker = ctx.ticker
        def step():
            self.flip.value = phase(ticker.elapsed(), FLIP_PERIOD)
        ticker.add(step)
        ctx.composer.render(self.card())

    def card(self):
        return (box().width(220).height(320).preserve3d()
            .rotateY(bind(self.flip).target(0, 360))
            .children([face("FRONT", 0), face("BACK", 180)]))
```

### The rules

- **The verbs keep their C++ spelling.** `alignItems`, `styleClass`,
  `Align.Center`. The compose README is the canon for both languages,
  and a sketch ports line by line.
- **The Python packages are the namespaces.** `sigil.compose`,
  `sigil.motion`, `sigil.weave`, `sigil.draw`, `sigil.sketch`,
  `sigil.sketch.kit`, `sigil.compose.kit`. No package re-exports
  another's words.
- **A designated-initializer aggregate is a class built from kwargs.**
  `Stage`, `Page`, `Cells`, `Well`, `Caption`, `Transition`, `Type`,
  `Block`, `Track`, `Spread`, the layout schemes. A verb that takes one
  aggregate also takes its kwargs, so `.font(size=22)` and
  `kit.stage(ctx, size=..., captureAt=...)` read as the C++ does. A
  duration is an integer of milliseconds, as the chrono value is.
- **Literals are unit functions.** `1_em` is `em(1)`, `50_pct` is
  `pct(50)`, `50_pw` is `pw(50)`; a bare number is pixels.
- **A colour is a tuple or `hexColor`.** A four-tuple converts wherever
  a colour is taken; a three-tuple is opaque.
- **`children` takes a list, and nested lists flatten.** Comprehensions
  replace `each`; `each` stays for the separator form only.
- **Two C++ words are Python keywords.** `from` in
  `animate(from(a).to(b))`, and `Cache::None`. The second already means
  the kernel's `Never` underneath, so `Cache.Never` is the spelling. The
  first is an open decision below; `appear` covers the one entrance
  every plate says.
- **RAII is `with`.** `kit.provide(theme)` binds the inherited value for
  the block, which is the describe-time scope it already is.
- **An output is an attribute with a `.value` property.** A binding
  compares by identity, so an output made inside `update` breaks
  pruning in Python exactly as in C++: hold them where the model is.
- **A description is a handle, so aliasing needs `copy()`.** In C++ a
  copied element clones on its next write; in Python two names on one
  node are one node. The composer is safe either way, because the C++
  copy-on-write protects the rendered tree from a later edit.
- **Python never sees a Skia canvas.** The immediate-mode floor is
  `pen(key, program)` and `graphics(key, program)`, with the pen a
  sketch already has. `Cache.Auto` records a static specimen once;
  `Cache.Never` runs the program every frame, which is the p5 loop.
- **A returned `Element&` is the same Python object.** The binding
  layer maps a returned reference back to the instance it already
  wrapped, which is what makes a chain read as one expression.

### What binds, in what order

The library's own preference for values over callables is what makes
it bind well. A keyed program, a `shapes` generator, an `fx` preset, a
stock router and a stock layout scheme are values: they prune and never
call back. The raw-callable escape hatches — a shape from a lambda, an
unkeyed program, a decoration scheme written by hand, a per-glyph effect
body — stay C++ at first. They are always-live by contract anyway, and
a per-glyph body in Python would be called thousands of times a frame.

| tier | surface |
| --- | --- |
| first | the `Element` setters, the factories, `Fill`, `Dimension` and its units, the enums, `animate`, `to`, `through`, `Transition`, `Output`, `bind` and its chain, `Type`, `textStyle`, `StyleSheet` and `rule`, `Composer::render`, `renderSlot` and the queries, and the sketch kit's `stage`, `page`, `cells`, `caption`, `theme` and `Provide` |
| second | the decorations and layer styles, `shapes`, spans, masks, connectors and rails, the grid and table schemes, feeds, instance pools, `PaintContext` |
| third | text fx tracks and selectors, rich text and stories, texture scenes, `bakeSet`, materials, video |

The element has about a hundred and fifty setters, the factories
fifteen, and the two kits a few dozen aggregates. Most are one binding
line each, and the aggregates need not be listed by hand: Boost.PFR is
already a dependency and can name an aggregate's fields under C++20, so
one helper binds a struct field by field.

## Decisions

Each with the recommendation the plan assumes.

- **The binding library.** nanobind: smaller and faster to call than
  pybind11, in vcpkg, and it generates the stubs an editor completes
  from. It builds extension modules and leaves embedding to the C API:
  the interpreter comes up through `Py_InitializeFromConfig`, with each
  module put on the inittab before it does, which is the one step
  pybind11 wraps and nanobind does not.
- **Which Python.** The one uv manages, found through CMake's
  `Development.Embed` component. Sketchbook runs from the build tree,
  so bundling an interpreter into the `.app` is a later chore and not
  this one.
- **The body's shape.** A class spelling `setup` and `update`, mirroring
  the C++ concept, over py5's module-level functions and globals.
- **Where a `.py` stands.** In `sketches/` under the same stem rule as a
  `.cpp`, filed as a file opened by path until the sweep walks them.
- **Naming.** The C++ spellings, camel case and all, so one README
  serves both languages; the repo's Python lint does not select the
  naming rules, so nothing flags a call to `alignItems`.
- **The entrance spelling.** `animate(from(a).to(b))` cannot be
  written. `from_(a).to(b)` is the literal mirror; a keyword on
  `animate` is the other candidate.

## Risks

- **The interpreter lock in a parallel sweep**, as above: correct, and
  slower per job.
- **An output that outlives its Python holder** is a raw pointer in the
  tree. The rule is the C++ one; a safer binding keeps every output
  bound during a describe alive until the next describe.
- **Plate determinism** rests on the sketch drawing from the pen's
  seeded streams.
- **The README's statement** that a sketch is real C++ with no
  scripting layer becomes a statement about the `.cpp` form when this
  lands, and the live host section gains the second builder.
- **Float widths.** Python floats are doubles and every binding narrows
  to the float the setter takes; a plate drawn from the same numbers is
  the same plate, since the narrowing is the same on every machine.

## The first slice

One pass, built and looked at on a machine that builds the tree:

1. `vcpkg.json` gains `nanobind`; the sketch library's CMake finds
   Python's embed component.
2. A new feature directory, `src/sketch/python/`: the interpreter
   brought up once per process, `PythonBody`, the kind that opens a
   canvas session on it and holds the lock around the session's calls,
   the bindings for `SketchContext`, the pen and `graphics`, and a
   test.
3. `<sigilsketch/canvas/Sketch.h>` exposes opening a canvas session on
   an owned body. `CanvasKind` holds a bare function pointer and the
   session class is private to its own unit, so nothing outside that
   unit can open one on a body it made.
4. `live/Host.cpp` splits its build step by extension, and
   `core/Sources.cpp` learns `.py` for units and siblings.
5. `book/Arguments.cpp` accepts a `.py` path where it accepts `.cpp`.
6. A test mirroring `sketch_reload_runs_the_file`: a `.py` fixture
   grounded in a colour no sketch uses, taken through `--frame`, its
   corner pixel read back.

The compose bindings are the pass after: the first tier above, with
the meter and the sheet on this page as the two sketches that prove it,
and the compose README's examples ported one by one as the check that
nothing was left out.
