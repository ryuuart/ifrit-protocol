# Findings

Defects found while working. Each entry states what the code does, what it
was evidently intended to do, and what a test should assert once intent is
restored. Delete entries as they are fixed; delete the file when empty.

## `fx::pass` with an explicit `Track::reach` composites the layer at the wrong scale

**What the code does.** A text track whose effect is `fx::pass` and which
declares a nonzero `Track::reach` draws its whole layer visibly scaled up —
glyph placement included. On a ring run of radius ~360 px in a ~720 px box,
`reach = 150` renders every glyph at roughly (box + 2·reach)/box ≈ 1.4× its
laid-out radius and size, spilling far outside the node. The mis-scale is
unconditional: it shows at cascade phase 0, where the pass is otherwise an
exact pass-through. With the reach left defaulted the same track draws at
the correct scale (but clips glyphs that stand proud of the node's box at a
ring's four extremes — the situation `reach` exists to cover).

**What it was evidently intended to do.** `Track::reach` is documented as
growing the pass's painted bounds only: the pass paints the node's box
grown by the track's reach and nothing outside it. Growing the clip must
not move or scale the content — the layer's pixels should land exactly
where the glyphs were placed, with the reach only widening what survives
the cull. The likely shape of the defect is a destination rect grown by
the reach while the source rect (or the layer's resolution) is not, or
vice versa.

**What a test should assert.** Render one glyph run through an `fx::pass`
whose SkSL is a pure pass-through (`return uContent.eval(xy);`), once with
`reach = 0` and once with a large explicit reach, at a phase where nothing
deviates. The two images must be identical inside the smaller bounds; the
larger-reach image may add pixels only OUTSIDE them. Repro: the
`rota_convocationis` study's charge pass with `.reach = 150.0f` restored on
its pass track (the study now buys the allowance with an oversized node box
and an inset-circle baseline instead).
