---
kind: type
library: SigilSkia
name: PaintOrderCanvas
qualified: sigil::skia::PaintOrderCanvas
group: Painting order
status: stable
---

# PaintOrderCanvas

Forwards every draw to the canvas it wraps, and closes the context's
recording after any draw that makes the backend read the destination.
Cheap where nothing reads: a scene without an advanced blend mode
closes nothing and costs one virtual call a draw.

The canvas it wraps must be the context's own, and both must outlive it.

## Why the fence exists

Graphite paints out of order on purpose: a draw that does not read the
destination carries a depth value, the backend may execute it whenever
it likes, and the depth test rejects whatever the original order put
beneath it. A draw that DOES read the destination — any blend mode past
Skia's coefficient modes, or a blender of its own — cannot be reordered
that freely, so Graphite makes it depend on the draws it overlaps and
relies on the depth test to reject it where a later draw has already
landed.

On the Vulkan backend such a read is an input attachment and Graphite
guards it with a barrier inside the render pass; MoltenVK serves that
barrier by restarting the pass, and the depth attachment does not survive
the restart. Every draw the pass had already executed then loses its
depth, and the reading draw paints over content that was described after
it — a solid box over a dense window comes back with holes in it, each
hole exactly the shape of something drawn earlier.

The fence is to close the recording after a reading draw, so that draw is
the last of its render pass and the ones described after it begin a pass
of their own. Draw a scene through this canvas and the picture is the
CPU's; the cost is one render pass per reading draw.

## What it costs, and when nothing

`sigil::skia::PaintOrderCanvas::needed` says whether a context is a
backend whose destination reads cost the depth attachment. A canvas over
a backend that answers false forwards every draw and closes nothing, so a
host wraps unconditionally and pays only where the fence is real.

`sigil::skia::PaintOrderCanvas::fences` is how many recordings this canvas
has closed since it was made. What a test asserts about, and what says
whether a scene pays anything for the fence.

`sigil::skia::PaintOrderCanvas::recorder` is the recorder the wrapped
canvas draws into, so a helper that promotes a texture finds it through
this canvas too.

## Layer bookkeeping

A layer whose own paint reads the destination is composited by its
restore, so the restore is the draw that has to end the pass. Four
overrides track which layer that is: `willSave` opens a layer entry that
reads nothing, `getSaveLayerStrategy` opens one recording whether the
record's paint reads the destination, `willRestore` takes the innermost
entry off remembering whether its composite reads the destination, and
`didRestore` closes the recording when the composite just performed read
one.

Beside them, `onFilter` closes the recording after a draw whose paint
makes the backend read the destination, and leaves the paint alone; and
`onDrawPicture` plays a picture back through this canvas rather than
handing it to the one below, so a reading draw inside it is fenced like
any other.

## See also

- `graphite/PaintOrder.h` — the header: `PaintOrderCanvas`
- [OffscreenSurface](value:sigil::skia::OffscreenSurface) — whose
  `canvas()` is already one of these
