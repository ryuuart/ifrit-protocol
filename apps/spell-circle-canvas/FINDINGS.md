# Findings

A work queue: each entry states what the code does, what it was evidently
intended to do, and what a test should assert once intent is restored.
Delete entries as they are fixed; delete the file when it is empty.

## fx()-batched glyphs: a blurred underlay muddies the foreground on the GPU backend

**What the code does.** A `text()` node carrying an `fx()` track draws its
glyphs through `GlyphRSXformBatches` — one bucket per (font, paint pass),
buckets created underlay → foreground → overlay and drawn in creation
order. On the CPU raster backend a hollow style (stroked foreground) with a
blurred dark stroke underlay renders correctly: the halo sits beneath the
stroke and the stroke keeps its colour. Rendered through the GPU backend,
the same node comes out with the foreground stroke dimmed to roughly half
its luminance and the counters darkened — as if the blurred underlay
composited over the foreground — while pixels outside the halo's reach are
untouched. The same style drawn WITHOUT a track (the per-node text painter,
seven one-letter nodes) is correct on both backends, and the same style
WITH a track is correct on the CPU backend, so the defect is specific to
the batched track draw on the GPU path.

Reproduction: `vertigo_titles` — collapse the seven letter nodes into one
`text(u8"VERTIGO", face)` with the blurred-stroke underlay on the face and
an `fx::pop` track; render headless with `--gpu` and compare the word
region against the CPU render at the same capture time (stroke pixels
~(130,127,119) instead of ~(222,216,204) over the same ground).

**Evidently intended.** The batches exist so "within a style every underlay
lands beneath every foreground," and the paint-complete batched draw exists
so a dressed glyph keeps its whole `PaintStyle` while moving. The backend
must not change that compositing; a mask-filtered underlay pass should draw
beneath the foreground pass on the GPU exactly as it does on the CPU.

**What a test should assert.** A GPU text test that draws one glyph whose
style carries a dark blurred stroke underlay beneath a light stroked
foreground, through `GlyphRSXformBatches`, and asserts (a) a pixel at the
centre of the foreground stroke keeps the foreground colour within AA
tolerance, and (b) the GPU render of those batches matches the CPU render
of the same batches within tolerance. Once it passes, `vertigo_titles`'
seven-node "VERTIGO" collapses to one `text()` node with one `fx::pop`
track (the sketch's own comments name this as the blocker).
