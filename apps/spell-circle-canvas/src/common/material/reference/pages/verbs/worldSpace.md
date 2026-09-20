---
kind: verb
library: SigilMaterial
name: worldSpace
qualified: sigil::material::skia::Paint::worldSpace
group: Uniforms and layer properties
status: stable
---

# worldSpace

## Description

It is for a field that must be continuous ACROSS separately-laid-out
nodes: one light over a whole instrument, weathering across a floor's
tiles. Author the field once against the canvas and every flagged node
samples it where it actually sits, through its layout offset and its
transforms. A ROTATED node samples through its rotation, so the
highlight stays put while the object turns — which is the behaviour a
per-node hand conversion cannot reproduce, since that turns with the
node. `uResolution` becomes the ROOT canvas size when flagged, so
`Paint::linearUnit` and `Paint::glowUnit` read as fractions of the
canvas.

**Per-material-LAYER, deliberately not inherited.** Flagging a
`Paint::blend` does not flag its layers, flagging a `Paint::sksl` parent
does not flag its slots — each paint anchors (or not) for itself. A
flagged parent's slots still SEE root coordinates, because the wrap
re-maps the coordinates the parent's SkSL evaluates them at; that is
Skia's local-matrix composition, not flag inheritance.

**Mechanism.** At resolve the built shader is wrapped
`makeWithLocalMatrix(W⁻¹)`, where W is the node→root matrix the paint
walk accumulated — `PaintFrame::toRoot`, the same matrix the hit test
inverts, so a node draws its field exactly where it can be hit. There is
one such seam, inside the resolve and the build, so every consumer
inherits it: a fill, a coverage gate, an `Effect::slot` material and a
`Paint::blend` layer.

That one build is also why three things follow from the flag: the digest
of varying inputs gains W's six floats, because a digest cannot detect a
change in an input it was never fed; `uResolution` becomes the root
canvas size; and the built shader is wrapped in W⁻¹ BEFORE the memo
stores it, so a field that is holding still keeps a stable shader
pointer.

**It rides the geometry tier**, like `uResolution`: W is layout-derived,
so the paint resolves when the node records, and the library re-records
it when the node or any ancestor moves, when an ancestor's described
transform changes, and every frame while a BOUND transform above it is
connected — releasing that once the transform provably holds still. The
flag is recipe and participates in `Paint::operator==`; W itself belongs
to the system and never compares.

**Resolved outside a composer** — `Paint::asShader`, a standalone
decoration, a measurement — `PaintFrame::toRoot` is identity, and the
paint deterministically degrades to NODE-LOCAL coordinates: the same
picture as the unflagged paint. A grouped subtree refuses to hold a bake
across a moving world-space field, because W is not among the floats the
group memo compares, so it drops to live paint there — conservative,
never stale.

`Paint::worldSpace() const` is the layer-local answer: is THIS paint
flagged. `Paint::usesWorldSpace` is the recursive one — does this paint,
or any blend layer or slot below it, anchor to the root. The reconcile
walk asks the second to flag the instance for W-invalidation; authors
want the first.

## See also

- [Paint](../types/Paint.md) — the value this sits on
- [`fit`](fit.md) — the other flag that makes a paint geometry-dependent
- [`offset`](offset.md) — the pan, which composes with the anchoring
