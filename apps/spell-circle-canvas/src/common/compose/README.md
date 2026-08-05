# SigilCompose

A C++20 static library that turns immutable, value-typed descriptions of a
2D scene into pixels on an `SkCanvas` the caller owns. It runs flexbox
layout through Yoga — with text leaves measured and drawn by SigilWeave,
the sibling paragraph-layout library — diffs each new description against
a retained tree to find what actually
changed, paints in an explicit CSS-like stacking order, and automatically
caches subtrees it can prove are not changing. Animation is Choreograph
outputs and timelines, stepped by a clock the host owns.

It owns no window, no surface, no render loop and no thread. You call
`Composer::render()` when your data changes and `Composer::draw()` inside
whatever paint callback your application already has, and it honours the
canvas's current matrix and clip like any other draw.

The problem it exists for is the middle ground between a paragraph layout
engine and a whole document engine: box-level composition of real
typography and arbitrary Skia drawing, sized by flexbox rules with
baseline alignment, layered with explicit z-order and blending, cached
like a display list, animated at scene rate, and refreshed from data
without rebuilding the world.

---

## Writing a component

A component is a free function from your data to an `Element`. There is no
base class, no lifecycle, and no state inside the library — the state is
the argument.

```cpp
#include <sigilcompose/Compose.h>
#include <sigilcompose/Studio.h>
#include <sigilcompose/Util.h>

#include <ranges>
#include <vector>

using namespace sigil::compose;
using namespace std::chrono_literals;

/// Your data. Copyable and equality-comparable — that is the whole contract.
struct Channel {
  std::string id;
  std::u8string label;
  float level = 0;     // 0..1
  bool alarm = false;
  bool operator==(const Channel &) const = default;
};

Element meter(const Channel &c) {
  const SkColor4f ink =
      c.alarm ? studio::hex(0xff5252) : studio::hex(0x8fd0ff);
  return box()
      .row()
      .gap(10)
      .padding(12)
      .corners({6})
      .fill(studio::hex(0x0e1218))
      .alignItems(Align::Center)
      // A mark on part of the boundary: L-brackets at every tangent break.
      .stroke(spans::corners(12), util::stroke(1.5f, Fill::color(ink)))
      .child(text(c.label, studio::type({.size = 13, .color = ink})))
      .child(box()
                 .grow()
                 .height(6)
                 .fill(ink)
                 .transformOrigin(0.0f, 0.5f)
                 // The bar ramps because the DESCRIBED value moved. Nobody
                 // steps it; the reconciler sees the change and starts a
                 // motion that retargets from wherever the bar is now.
                 .scaleX(animate(to(c.level), Transition{.duration = 220ms})));
}

Element dashboard(const std::vector<Channel> &channels) {
  return box()
      .column()
      .gap(8)
      .padding(24)
      .fill(studio::hex(0x05070a))
      .children(channels | std::views::transform([](const Channel &c) {
                  // memo() skips the describe call entirely while the props
                  // compare equal. key() is what the reconciler matches on
                  // across describes, so rows survive reordering.
                  return memo(c, meter).key(c.id);
                }));
}
```

The host side is three objects, and `util::Stage` bundles them:

```cpp
#include <sigilcompose/Util.h>

sigil::weave::FontContext fonts = /* yours */;
util::Stage stage({960, 540}, fonts);

stage.render(dashboard(model));           // whenever the data changes

// …inside your own paint callback:
bool wantAnotherFrame = stage.frame(canvas);   // tick, draw, needs-more
```

Spelled out, which is what to do when the clock or the ticker is shared
with something else:

```cpp
motion::FrameClock clock;
motion::Ticker ticker;
Composer composer(ticker, fonts);       // both must outlive the composer
composer.setClock(&clock);
composer.setSize({960, 540});
composer.render(dashboard(model));

const double dt = clock.tick();
const bool moving = ticker.tick(dt);
composer.draw(canvas);
const bool again = moving || composer.dirty() || ticker.active();
```

The other write path is a live binding — a `choreograph::Output` the host
mutates every frame, read straight out of paint with no `render()` call:

```cpp
choreograph::Output<float> spin{0.0f};
ticker.add([&](double) {
  spin = studio::phase(ticker.elapsed(), 6.0);   // a wrapping [0,1) phase
  return true;
});

box().rotate(bind(&spin).target(0, 360));
```

---

## The mental model

**An `Element` is a shared, copy-on-write description.** It is a value: you
build one with fluent setters, copy it, compare it, throw it away. Copying
an `Element` bumps a refcount; mutating one that is shared clones first, so
a description already handed to the composer can never be altered behind
its back. Hot fields live inline on the node; rare and kind-specific state
is pushed into out-of-line value-semantic blocks, so an absent feature
costs one null pointer.

**An `Instance` is the retained counterpart**, and you never see it: parent
and child pointers, the resolved description, a Yoga node, paint order,
text layout state, animated value slots, derived geometry, and every cache
slot. Elements are write-only. Reads target the composer, after layout —
`Composer::bounds`, `Composer::paragraphLayout`, `Composer::hitTest`,
`Composer::routesAt`, `Composer::stats`, `Composer::profile`. Querying a
description is not offered, because it would invent a second identity
system next to keys.

### The two write paths

There are exactly two ways to change what is on screen, and both are
*declared*:

1. **Describe** — `Composer::render()` and `Composer::renderSlot()`. This
   carries structure and discrete state. Children are reconciled by key,
   falling back to position among unkeyed siblings; keyed reconciliation
   *is* the child-swap API. There is no imperative node mutation, and that
   absence is deliberate: it is the door that would make every cache
   unsound.
2. **Bind** — store a pointer to a live `choreograph::Output` in the
   description and mutate it per frame. Bound properties are paint-only by
   contract. They never relayout, and the node's cached content replays
   under the new transform or the new value.

That split is the whole reason caching can be automatic. Volatility is
*derived from the declarations*, not sniffed at runtime, so "does this
subtree change" is a decidable property rather than a heuristic.

### Phase order

`render()` mounts or patches, as a recursive keyed reconcile. A memo
compares its captured environment snapshot and then the author's props
comparator; a hit reuses the previous payload without describing at all.
A structural equality check is the prune: equal means nothing is marked
dirty and no transition is applied, though children still reconcile.
Unequal means dirty marking up the tree, a Yoga style write, a text
content revision bump, and transition application. Paint order among
siblings is a stable sort by `zIndex` then declaration order. Then the key,
slot and edge indices rebuild.

`draw()` detects the backend and host scale, then runs layout: Yoga first,
then up to three convergence rounds of custom `layout()` schemes,
`centerAt` pins, and the derive phase, each of which may re-run Yoga.
Recordings whose baked geometry moved are invalidated. Derive resolves text
exclusions and connector/rail routing over flat edge lists, cycle-guarded.
Released scalars are scanned and volatility computed in one walk. Then
paint runs, selecting a cache tier per node.

### Paint order inside a node

Fixed, and worth memorising, because several traps are just this list:

```
backgrounds · background span passes │ fill · echoes │ overlays │
content leaf │ children │ foregrounds · foreground span passes
```

Decorations dress the node's *outline*, so `clip()` does not clip them —
it bounds the fill, the content leaf and the children. A stacking context
forms on `zIndex`, opacity below 1, a blend mode, a transform, a clip, or a
layer effect, and children cannot interleave outside it: a component cannot
escape the z-order of the site it was composed into.

---

## The header map

Everything lives in `namespace sigil::compose` under
`include/sigilcompose/`. The public include root is `include/` and nothing
else — the two internal headers beside the sources are not reachable from
outside the library.

**Kernel — `Compose.h`.** `Element` and its builders; the factories `box`,
`stack`, `positioned`, `text`, `image`, `custom`, `slot`, `connector`,
`rail`, `band`, `layout`; `Composer`; `memo`; the `env::` inherited-value
channel; the comparable seam values (`Shape`, `Shaper`, `Profile`,
`Decoration`, `CrossingRule`); the stroke grammar (`spans::` and
`Element::stroke`); the masking family (`parts::`, `by::`, `Region`);
`Effect`; the one-shot verbs `snapshot`, `measure`, `metrics`,
`measureRun`; and re-exports of SigilMotion's animation vocabulary
(`Animatable`, `Transition`, `animate`, `bind`, `ease::`) so authoring
never has to name a second library. A user who reads only this header has a
complete and sound model; nothing below it changes kernel semantics.

**Paint values.** `Material.h` is the polymorphic paint value that
supersedes a flat `Fill` — gradients, images, raw SkSL with live uniforms,
blend stacks that compile to one shader, world-space anchoring. `Sdf.h`
gets shape, border, glow and soft shadow out of a single shader pass.
`Pattern.h` and `Patterns.h` bake tile recipes once into repeating
materials, plus stock generators. `Ocio.h` is an output-stage view
transform for `Composer::setView`, compiled only when the build finds
OpenColorIO.

**Geometry.** `Shapes.h` is the silhouette and curve library — every
generator is a comparable value, so a shaped node prunes like an unshaped
one. `Layouts.h` holds the placement schemes for the `layout()` seam
(`layouts::Radial`, `AlongPath`, `ModularGrid`, `Diagonal`, `BaselineGrid`,
`Scatter`). `Routers.h` holds the stock connector and rail routers
(`routers::straight`, `orthogonal`, `polyline`, `octilinear`, `orbit`).

**Marks.** `Decorations.h` has the concrete primitives that plug the
`Decoration` seam — `PathFormat` (stroke formatting), `Slice` (lattice
image mapping), `ContourWalk` (walk the outline and run a program at each
sample), `Wash`, `Border`. `Brushes.h` is the brush engine over them:
`brush::solid`, the composites `brush::layers` and `brush::weave`, and the
archetypes `brush::Scatter`, `brush::Pattern`, `brush::Ribbon`,
`brush::Art`. `Lines.h` is the cartography and diagram stroke vocabulary —
parallel casings, terminal caps, ties, waves. `LayerStyles.h` is the
Photoshop route to rich surfaces: bevels, sheens, inner shadows built from
gradients and blurs rather than shaders.

**Components.** `Kinetic.h` supplies stock per-glyph effects for the
`GlyphFx` seam. `Console.h` is a virtualized append-only log, built purely
by composing the kernel. `Instances.h` renders thousands of sprites as one
leaf, with the pool on your side of the seam. `Web.h` makes a live
Ultralight page a leaf; it is a header-only adapter and the library does
not link SigilScry, so include it only in targets that do.

**Host and tooling.** `Util.h` is deliberately-demoted sugar — `util::Stage`
(the canonical host loop), gradient constructors, `util::stroke`,
`util::Shadow`, `util::disc`, `util::marquee`. `Studio.h` is the file
prelude: `studio::hex`, `studio::type`, `studio::pickFace`, `studio::ramp`,
`studio::phase`, `studio::fmt` — spellings, never decisions.
`GpuImage.h` holds `gpuimg::drawLattice` and `gpuimg::drawSpriteAtlas`,
which are mandatory rather than convenient (see the traps). `Debug.h`
verifies generated geometry — `debug::coverage`, `debug::check`,
`debug::report`, `debug::failures` — for tests and `--verify` paths, not
the paint loop.

**Kit — `kit/Kit.h`.** A tier above the library that adds no kernel state
and no new equality: `kit::Frame` and `kit::Grid` (figure-local polar and
unit coordinates), `kit::ticks` and `kit::chords` (division ladders as one
path), `kit::PixFont` (aliased bitmap-font bakes), `kit::Scrim` and the
halo/shade legibility helpers, and `kit/Strokes.h`'s shapers, profiles and
span compositions. The kit is a **separate CMake library**
(`SigilComposeKit`) whose only include path is compose's public headers,
which is how the public/internal boundary is proven rather than asserted.
Note that the umbrella header does not pull in `kit/Strokes.h`; include it
directly.

---

## The declared-volatility contract

The library caches provably-static subtrees on its own. A `Cache::Auto`
subtree with nothing volatile in it records into a picture; a node that has
been expensive for several consecutive frames may be re-baked into a raster
image and blitted thereafter. None of that is safe unless "static" is
*true*, and nothing in the library can introspect a type-erased value to
find out.

**So anything that changes without a re-describe must say so.** Concretely:

- A decoration whose paint moves — a bound dash phase, a walk keyed to
  elapsed time — declares `bool isAnimated() const`. The seam reads it off
  the value at construction; a scheme that stays silent is treated as
  static and its node's picture will be replayed forever.
- A `custom()` paint program that reads the clock (or anything else the
  library cannot see) must declare `.cache(Cache::None)`. It is the
  immediate-mode floor and it costs a repaint per frame, which is the
  point.
- A `Material` that reads `uTime` or carries a uniform bound to an
  `Output` is live by construction and declares itself; so is an `Effect`
  with a bound uniform or a live child. Tier inheritance is real: a live
  child makes the parent effect live, so no cache can freeze the
  parameter.
- A decoration that paints beyond the node's box declares `bleed()`, and
  one that needs to say how wide the *mark* is declares `reach()`. These
  are different numbers — an inner-aligned stroke bleeds zero while
  painting a mark several pixels wide. Over-reporting is safe;
  under-reporting silently truncates cached pictures.
- A decoration that reads another element's resolved path declares
  `borrows()`, so the element can register the keys without looking inside
  the value.

The counterpart obligation is equality. Anything read live must
participate in the reconciler's structural comparison, or a pruned node
reads a stale value forever. The conservative fallback is built in:
anything holding an incomparable callable compares *unequal* and never
prunes.

---

## Traps

### Silent no-ops, the largest class

Several correct behaviours produce nothing, with no diagnostic, and look
exactly like a layout bug.

- **An unknown key resolves to nothing, everywhere in the derive family.**
  `flowAround("typo")`, `spans::fit("typo")`, `around("typo")`, a
  `connector` to a node not in the tree, a `strand::from` on a missing key
  — every one draws nothing and says nothing. Check your keys first.
- **Hit testing returns any keyed node whose box contains the point,
  painted or not.** A keyed full-bleed layout shell with no fill therefore
  swallows every hit in the frame, and the failure is total and silent.
  The opt-out is `Element::hitTestable(false)`, which excludes the node's
  own box while still testing its children.
- **Skia's native lattice and atlas draws are not implemented on
  Graphite** in this Skia — they draw nothing. Worse, one recorded on a
  raster canvas still vanishes when the recording replays on Graphite, so
  a raster test cannot see it. Use `gpuimg::drawLattice` and
  `gpuimg::drawSpriteAtlas`, which decompose on every backend and never
  emit the native op. This is not an optimisation layer; it is the only
  correct path.
- **A `custom()` leaf sizes like an empty box.** It is literally a box with
  one background program, so it has no intrinsic size: dropped into an
  `absolute().inset(0)` parent it measures zero on the main axis and the
  program runs against a zero-height context. Give it dimensions, or make
  it `absolute().inset(0)` itself.
- **`.key()` on a `slot()` renames the mount.** A slot's name *is* its key,
  so `slot("hud").key("panel")` produces a slot called `"panel"` and
  `renderSlot("hud")` then finds nothing. It warns once.

### Lifetime

Every live binding is a **non-owning raw pointer** to an `Output` the
caller owns; the composer holds its `FontContext` and its `Ticker` by
reference, and both must outlive it. A recreated `Output` at a new address
does more than dangle: bindings compare by *identity*, so the new address
also breaks prune equality and re-patches the node on every describe. Hold
your outputs where you hold your model.

### Pruning

The raw-callable escape hatches — a `Shape` built from a lambda, an
unkeyed `custom()` program, a bare `PaintProgram` decoration — can never
compare equal to a separately constructed one, so their nodes re-patch on
every describe. The fix is to hold the value rather than re-minting it,
or to wrap the node in `memo()`. Two spellings avoid the problem outright:
`custom(key, program)` makes the key the program's identity, and every
`shapes::` generator is a comparable value.

This matters more than it sounds. An inherited value carried through
`env::` that holds a `std::function` is incomparable, and that turns every
`memo` below it into a permanent miss. Materialise derived values *into*
the type: run the function, store the result.

### Ordering

Decoration stacking is a contract, not a hint. `background()` paints
*beneath the fill*, so an opaque fill covers it completely — a bevel added
with `background()` on a filled node draws underneath its own surface and
looks like nothing happened. The slot between the fill and the content is
`overlay()`; above the children is `foreground()`.

Within `stroke()`, unqualified whole-boundary strokes paint first and
span-qualified passes paint over them, each group in declaration order.
Interleaving the two groups by call order is not expressible; make the
whole-boundary one a span pass (`spans::every(1)`) so both are in one list.

Span-qualified passes also *claim* the runs they resolve to, and two claims
that overlap are reported out loud. Stacked masks intersect where their
selections overlap — both gates must pass — and union is spelled inside one
gate value by combining spans with `|`, never across masks.

One coordinate convention, stated once and obeyed everywhere: **positive
`across` is to the LEFT of travel**, which in screen space (y down) is
outside a clockwise path. `bandPointAt`, `Profile::across`,
`strand::offset`, `TextPath::offset` and the `lines::` family all mean the
same side. Relatedly, **fraction 0 on a boundary is the bottom-left
corner**, running up the left edge — so `spans::upTo(0.25f)` on a square is
the left edge, not the top one.

---

## Boundaries

The library links `SigilImage`, `SigilMotion`, `SigilWeave` and Skia
publicly, and Yoga privately. OpenColorIO is optional and gates `Ocio.h`
alone.

Deliberately *not* linked: SigilScry (the web leaf is a header-only
adapter, exercised by its own test target), EnTT (the instancing header
keeps the registry on your side), SigilShape, Diligent, and Qt — Qt
identifiers are banned outright in exported headers.

What it refuses to be:

- **No markup, parser or external DSL.** Markup can only name
  pre-registered values; the vocabulary here is C++ values and callables.
  A serialization schema can be a *producer* of element values, never the
  API.
- **No imperative node mutation.** Describe or bind, and nothing else.
- **No timeline object.** Multi-beat choreography is windowed bindings
  over one phase output (`bind(&phase).window(lo, hi)`).
- **No surface, loop or thread ownership.** The composer is a guest in
  someone else's canvas, and a host that wants many surfaces makes many
  composers.
- **No depth and no perspective in the model.** There is no z, no
  `rotateX`, no projection. A camera, if you want one, is the host's
  matrix on the canvas — a recording is matrix-independent by
  construction, so a moving camera invalidates nothing the library holds.
  The places that pin pixels to a device rect refuse a perspective matrix
  explicitly.
- **Compositing happens in encoded sRGB, with no linear stage.** Every
  surface compose paints into is `N32Premul` with no colour space
  attached, so the `SkColor4f` you write is the display-encoded number
  that lands in the byte and a shader's channels are those same numbers.
  Any weighting of colour channels inside the library uses coefficients
  defined on encoded values. `Composer::declareInputSpace` lets you state
  what you believe your values are; a mismatched declaration warns once
  and performs **no** conversion, because a colour-managed surface would
  be a breaking change rather than a setting.

---

## Build and test

The library target is `SigilCompose`; the kit is `SigilComposeKit`. From
`apps/spell-circle-canvas`:

```sh
python3 scripts/setup.py --config Debug
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Registered tests: `compose_test`, `compose_kit_test`, `compose_spike_test`,
`compose_gallery_test`, `compose_sketch_smoke`, `compose_sketch_stock`,
`compose_sketch_shape`, plus `compose_gpu_test` (Apple only, needs the
Graphite plumbing) and `compose_web_test` (needs the Ultralight SDK).
`compose_bench` and `compose_demo` are executables, not tests — anything
resembling a performance claim belongs to `compose_bench` and to the plate
ledger, never to prose.

**The gallery** is a macOS app bundle, so headless runs go through the
binary inside it:

```sh
build/bin/<config>/ComposeGallery.app/Contents/MacOS/ComposeGallery \
    --headless <outdir> [--gpu] [--scene <name|index>]
```

`--scene` takes a case-insensitive substring and renders just that one,
which is the loop for visual work. `--shot <png>` captures the application
window itself rather than a scene.

**The sketch host** is `ComposeSketch`, a live-coding loop: point it at a
single `.cpp` file, save, and the recompiled sketch hot-swaps into the
running canvas. It renders headlessly too (`--frame out.png --at <s>`), and
`--bench` reports frame-time phases. Study sketches are compiled into the
gallery as an object library, so one file is both a hot-reload sketch and a
gallery scene.

**Byte-identity sweeps** run through `scripts/plate_ledger.py`, which
renders every gallery scene in parallel, hashes the plates and compares
against a stored baseline. `--rebase` adopts a new baseline; `--stability N`
re-renders movers to separate a self-nondeterministic scene from a real
change.

### The generated doc-probe translation unit

`compose_test` builds a C++ file that does not exist in the source tree.
`test/docs/api_doc_probes.py` reads this document, extracts every qualified
name an author could copy out of it — from fenced code blocks **and** from
inline `code` spans, because the prose carries as many names as the
examples do — and emits probes that only compile if the headers still spell
those names that way. A member is probed through a `requires` expression, a
namespace-scope entity through a using-declaration, and a designated
initialiser through the initialiser itself, which is a stricter question
than whether the name resolves.

The consequence is the point: a name written here that drifts out from
under the prose is a build break, not a confident wrong answer. Names the
document mentions on purpose without their existing — a worked example's
own host type, a spelling recorded because it was removed — go in the
generator's exclusion table with a reason. A documented name that resolves
to nothing and is not excluded fails the generator, so the guard cannot go
quiet.
