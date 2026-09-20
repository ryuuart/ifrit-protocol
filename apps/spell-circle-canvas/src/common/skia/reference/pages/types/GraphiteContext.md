---
kind: type
library: SigilSkia
name: GraphiteContext
qualified: sigil::skia::GraphiteContext
group: Bring-up
status: stable
---

# GraphiteContext

Owns the Skia Graphite Context + Recorder used to draw into offscreen
textures from SkCanvas. Built on an existing native device/queue so
Graphite's GPU work rides the same command queue as the host's own
rendering: submissions and the host's later passes execute in
submission order on that one queue, which is what lets a submit stay
asynchronous.

Metal and Vulkan are parallel bring-up paths with one factory each,
both Qt-free. A Qt host reaches them through the adapters in
`<sigilskia/qt/QtInterop.h>`, which unwrap a QRhi's native handles and
forward here.

## Threads

A Recorder belongs to one thread; the Context tolerates use from
several threads but never at once. `sigil::skia::GraphiteContext::recorder`
is the thread that made the context; another thread takes its own from
`sigil::skia::GraphiteContext::makeRecorder` and records on it alone;
and every call on `sigil::skia::GraphiteContext::context` — inserting,
submitting, reading back, retiring finished work — from any thread holds
`sigil::skia::GraphiteContext::lockContext` once more than one thread can
reach the context.

## The two recorder preconditions

`sigil::skia::GraphiteContext::makeRecorderOptions` is REQUIRED for every
recorder: pass these to `makeRecorder()`. Two settings in them are
preconditions, and violating either fails silently rather than loudly.

1. The caching ImageProvider. Graphite performs NO implicit uploads: a
   draw that samples a raster (non-Graphite) SkImage asks the recorder's
   provider for a texture version and DROPS the draw when there is none.
   A recorder built without these options renders nothing from any raster
   image and reports no error.
2. Ordered recordings. Every recording this recorder snaps must be
   inserted, in order. A snap that returns null, or one whose recording
   is discarded, skips an ID and permanently kills the recorder — every
   later insert fails and nothing ever renders again. Never snap in order
   to throw the result away.

`sigil::skia::GraphiteContext::makeContextOptions` is one funnel for
ContextOptions too (both backends). It reads `SIGILSKIA_GLYPH_ATLAS_BYTES`
to cap the Graphite glyph-atlas texture budget; unset leaves Skia's own
default in place.

Every context is given one process-wide thread pool to build its device
pipelines on. Without it Graphite compiles each one serially on the
thread that recorded the draw, which for a scene wearing a chain of
runtime shaders is a dozen compiles inside its first frame. The pool is
never released: Skia requires it to outlive every context built over it,
and the last context goes during static teardown.

## Where a shader that would not compile is reported

Graphite builds the fragment program for a draw at record time and
compiles it on the device; a program that fails there is dropped, the
draw paints nothing, and the frame after it tries again. With no handler
installed Skia prints the generated shader and the compiler's errors to
stderr and the process carries on, so a body that compiles as its own
SkSL program and not once Graphite has inlined it into a pipeline is a
scrolling log rather than something a caller can act on. A handler set
through `sigil::skia::GraphiteContext::reportShaderErrorsTo` is given to
every context this factory builds afterwards, which is what makes such a
failure observable — so set it BEFORE the context is created.
Process-wide; the caller keeps ownership and must outlive the contexts.
Null restores Skia's own reporting.

The compile that fails runs on the pool every context builds its
pipelines on, so the handler is called FROM THAT POOL and from several of
its threads at once when a scene's stages fail together: a handler that
collects must guard what it collects into, and one that counts must count
atomically.

## What Graphite did with a pipeline, as it did it

A backend builds one device program per distinct draw and the thread that
recorded the draw waits for it, so a scene wearing a chain of runtime
shaders pays one program per stage the first time it is drawn. Which
pipelines a scene needs, and which of them were already standing, is the
whole of what a warm-up can act on, and nothing else reports it. That is
what `sigil::skia::GraphiteContext::PipelineReporter` answers, and
`sigil::skia::GraphiteContext::reportPipelinesTo` installs one.

A reporter is given to every context this factory builds afterwards, so
set it BEFORE the context is created. Process-wide; the caller keeps
ownership and must outlive the contexts. Null reports nothing.

THE REPORTER MUST BE THREAD-SAFE. A pipeline is built on the pool every
context compiles on, and `added` is called from the thread that built it
— several at once for a scene whose stages are built beside each other.
`found` arrives on whichever thread asked for the pipeline, which is the
thread that recorded the draw, and may run at the same time as an `added`
for another one.

## The runtime effects a serialised pipeline key may name

A pipeline's key describes the whole inlined paint tree, and a runtime
effect in that tree has no name a later run would recognise unless it was
declared through `sigil::skia::GraphiteContext::registerRuntimeEffects`:
without the declaration the key for such a pipeline is absent, and
precompiling a recorded set skips exactly the stages a chain of effects is
made of. The effects are copied and given to every context built
afterwards, so declare them BEFORE the first one. Process-wide, and the
list REPLACES whatever stood before it: a caller that keeps recorded keys
on disk must throw them away whenever this list changes, because the same
effect at a different place in it is a different name.

The backend reserves a fixed block of names for a client's effects, so a
longer list is CUT at the block's length and the effects past the cut keep
the unstable names they had. The answer is how many were taken, and it is
what a caller keying stored keys on the list must key them on: the list it
offered and the list that was declared are not the same one.
`sigil::skia::GraphiteContext::runtimeEffectLimit` is that block's length.

## Standing the pipelines up

`sigil::skia::GraphiteContext::makePrecompileContext` is the helper a
warm-up builds pipelines through, made where the context is and usable on
ANOTHER THREAD — which is the point: a warm-up run where frames are drawn
would be the stall it exists to remove. It holds the context's shared half
rather than the context, so the thread that draws keeps drawing while it
stands. Null when there is no context.

`sigil::skia::GraphiteContext::precompile` rebuilds the pipelines a span of
keys names, on the calling thread. A key this backend or this version of
Skia cannot read is skipped; the answer is how many pipelines were built.
It goes through a helper of its own, so a caller with several kinds of
warming to do takes one `makePrecompileContext()` and spends it on all of
them.

## See also

- `graphite/GraphiteContext.h` — the header: `GraphiteContext`,
  `PipelineReporter`
- [OffscreenSurface](value:sigil::skia::OffscreenSurface) — the texture a
  context draws into, and the fence behind its submit
- [PaintOrderCanvas](value:sigil::skia::PaintOrderCanvas) — the canvas
  that keeps a scene's painting order on the backend that cannot
