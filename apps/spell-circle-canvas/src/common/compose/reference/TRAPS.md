# Traps

A chapter of [SigilCompose's README](../README.md).

## Silent no-ops, the largest class

Several correct behaviours produce nothing, with no diagnostic, and look
exactly like a layout bug.

- **A class no sheet in force carries sets nothing.** `styleClass("labl")`
  under no `weave::StyleSheet`, or under one that never registered the
  name, leaves the leaf in whatever it inherits and warns once; the
  symptom is text at the inherited size, which looks like a class that
  did not take. State the sheet with `Element::styleSheet` on the tree
  the element LANDS in, on the element or any node above it: a class is
  resolved where the element lands, not where it is written.
- **A custom property nobody set resolves to nothing.** `Fill::var("acent")`
  paints nothing and a `var("guter")` length is zero, each warning once;
  a property set as a length and read as a colour, or the reverse, is the
  same miss. The nearest ancestor's `Element::var` is what a read finds,
  so a property set on a sibling or on the leaf's own child is not there.
- **A bake is a root.** `snapshot`, an atlas cell, a pattern tile and a
  texture scene inherit nothing from the tree that asked for them: a fill
  written as `Fill::currentInk()` inside one is the initial black, and an
  inheriting leaf inside one is 16 px in the font context's family. Set
  the font and ink on the baked tree itself, or seed the composer with
  `Composer::setInherited`.
- **The theme is lexical; the cascade is not.** A value read through
  `core::environment::Provide` is copied into the element by the code
  that builds it, so a component built outside a scope and mounted inside
  it keeps the outer look — while its text still takes the inner ink,
  because the font, the ink and the properties are carried by the tree.
- **An unknown key resolves to nothing, everywhere in the derive family.**
`contentFlowAround("typo")`, `spans::fit("typo")`, a wire to a node its
operator's scope does not hold, a `strand::from` on a missing key —
every one draws nothing and says nothing. Check your keys first. (A
`weave::rich().slot()` name is the one that is LOUD, once: it names a mount
point the author typed, not a geometry source.)
- **Hit testing returns any keyed node whose box contains the point,
  painted or not.** A keyed full-bleed layout shell with no fill therefore
  swallows every hit in the frame, and the failure is total and silent.
  The opt-out is `Element::hitTestable(false)`, which excludes the node's
  own box while still testing its children.
- **Skia's native lattice and atlas draws are not implemented on
  Graphite** in this Skia — they draw nothing. Worse, one recorded on a
  raster canvas still vanishes when the recording replays on Graphite, so
  a raster test cannot see it. Use SigilSkia's `skia::draw::drawLattice`
  and `skia::draw::drawSpriteAtlas` (over a `skia::draw::SpriteBatch`)
  (`<sigilskia/draw/Direct.h>`), which decompose on every backend and
  never emit the native op. Raster sources use the recorder's image
  provider, so direct draws, slices and atlases share its texture cache
  without keeping cache storage in their descriptions.
- **A `custom()` leaf sizes like an empty box.** It is literally a box with
  one background program, so it has no intrinsic size: dropped into an
  `absolute().inset(0)` parent it measures zero on the main axis and the
  program runs against a zero-height context. Give it dimensions, or make
  it `absolute().inset(0)` itself.
- **`.key()` on a `slot()` renames the mount.** A slot's name *is* its key,
  so `slot("hud").key("panel")` produces a slot called `"panel"` and
  `renderSlot("hud")` then finds nothing. It warns once.

## Lifetime

Every live binding is a **non-owning raw pointer** to an `Output` the
caller owns; the composer holds its `FontContext` and its `Ticker` by
reference, and both must outlive it. A recreated `Output` at a new address
does more than dangle: bindings compare by *identity*, so the new address
also breaks prune equality and re-patches the node on every describe. Hold
your outputs where you hold your model.

## Pruning

The raw-callable escape hatches — a `Shape` built from a lambda, an
unkeyed `custom()` program, a bare `PaintProgram` decoration — can never
compare equal to a separately constructed one, so their nodes re-patch on
every describe. (Each of them names only the parameters it reads: the
laid-out size is offered to an outline and the canvas and its context to a
program, so `.shape([] { return p; })` is as much an outline as
`.shape([](SkSize s) { … })` is. What a callable NAMES has no bearing on
what it compares — which is nothing.) They stay in the grammar and they stay always-live: a
node carrying one is re-patched and re-recorded for as long as it exists,
which is the price of handing over something the library cannot read.

**A callable becomes comparable by declaring what tells it apart**, and
there are three spellings for that, one per kind of identity:

- **the path is the identity** — `.shape(heldPath(p))` over
  `.shape([p] { return p; })`. A path cooked once and held
  compares by its own generation, which every copy carries. Rebuilding the
  path each describe is a new generation and stays conservative: cook it,
  hold it, hand it here. `pathFigure(p, bleed)` is the leaf that also
  gives the node the path's own bounds.
- **a value the body closes over is the identity** —
  `.shape(key, fn)` (or `keyedShape(key, fn)` for the value form) and
  `custom(key, program)`.
- **the whole drawing is a value** — every `geometry::shapes::` generator,
  including the keyed `shapes::parametric(key, …)`.

The keyed forms all take one author contract: **one key names one
drawing**. Anything the body reads that is not in the key is invisible to
the prune, and a pruned node replays the picture it recorded — so a
number left out of the key freezes at whatever it was on the frame that
recorded, with no error and no warning.

This matters more than it sounds. An inherited value carried through
`core::environment::` that holds a `std::function` is incomparable, and that turns every
`memo` below it into a permanent miss. Materialise derived values *into*
the type: run the function, store the result.

## Ordering

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
outside a clockwise path. `bandPointAt`, `TextPath::offset`, the
`lines::` family and every signed distance in `geometry::path` — a
`Profile`'s `across`, `geometry::path::profile::offset`,
`geometry::path::parallel` — all mean the same side. Relatedly, **fraction 0 on a boundary is the bottom-left
corner**, running up the left edge — so `spans::upTo(0.25f)` on a square is
the left edge, not the top one.
