# Skia Graphite plumbing

This directory is the bring-up glue for Skia's Graphite GPU backend on a
device someone else already owns. It builds a Graphite `Context` and
`Recorder` from a native Metal device and command queue — or from a Qt
`QRhi` — and wraps an existing native texture as an `SkSurface`, so ordinary
`SkCanvas` draw calls land directly in that texture with no copy. There is
no scene, product, or drawing logic here.

Global namespace. Headers: `SkiaGraphiteContext.h`, `SkiaOffscreenSurface.h`.

## Using it

Stand the context up once, from handles you already have, and keep it alive
for as long as you draw:

```cpp
#include "SkiaGraphiteContext.h"
#include "SkiaOffscreenSurface.h"

// Qt-free, from raw Metal handles (id<MTLDevice> / id<MTLCommandQueue>
// bridged to void*):
std::unique_ptr<SkiaGraphiteContext> graphite =
    SkiaGraphiteContext::createMetal(device, queue);

// …or from Qt, inside a QQuickRhiItem renderer:
std::unique_ptr<SkiaGraphiteContext> graphite =
    SkiaGraphiteContext::create(rhi());
```

Then, per frame, wrap the texture you want to render into, draw, and submit:

```cpp
SkiaOffscreenSurface surface(*graphite, texture, pixelSize);  // QRhiTexture + QSize
SkCanvas *canvas = surface.canvas();
if (!canvas)
  return;                       // the wrap failed; nothing to draw into

canvas->clear(SK_ColorTRANSPARENT);
drawMyScene(*canvas);
surface.submit();
```

The Metal constructor takes a bridged `id<MTLTexture>` and explicit
dimensions instead: `SkiaOffscreenSurface(*graphite, mtlTexture, width, height)`.

`SkiaOffscreenSurface` is a thin wrapper around a texture someone else owns —
construct it fresh each time rather than caching it. If you need the
underlying objects, `graphite->context()` and `graphite->recorder()` hand
them out.

## The mental model

**Two targets, one API.** `SpellCircleSkia` is Qt-free and, on Apple,
carries the Metal translation units — this is what the native macOS app and
other non-Qt consumers link. `SpellCircleSkiaQt` holds the `QRhi` adapters
on top of it; on Apple those unwrap Qt's native Metal handles and forward,
and on other platforms they *are* the Vulkan implementation. Qt code links
`SpellCircleSkiaQt`, never the bare target.

**Exactly one native translation unit per build defines the `QRhi`
factory,** and it returns null for any backend it does not serve. So
`SkiaGraphiteContext::create(QRhi *)` returning null is a normal, expected
outcome — it means "this build cannot serve that backend" — and callers must
handle it rather than assume Metal.

**Graphite shares the host's command queue.** That is the whole reason this
plumbing exists in the shape it does: because Graphite's submissions and the
host's own render pass go into one queue in submission order, the host's
later work observes the finished texture without any CPU synchronization.
`submit()` is therefore asynchronous, and correct only under that sharing.

## Gotchas

These two are preconditions, not tuning knobs. Missing either produces
silent, total failure — no crash, no error, nothing rendered.

**Recordings must stay ordered.** The recorder is created with ordered
replay required, because unordered replay makes every `snap()` evict the
glyph, path and clip atlases, re-uploading every glyph once per frame. The
price is a hard rule: a `snap()` that returns null, or a snapped `Recording`
that is never inserted, skips an ID and permanently kills the recorder.
Every later `insertRecording` then fails and the process renders nothing for
the rest of its life. Never snap in order to discard, anywhere downstream of
this context. A failed insert warns once on stderr; after that the silence is
all you get.

**Every recorder must carry the caching image provider.** Graphite performs
no implicit uploads. A draw that samples a non-Graphite (raster) `SkImage`
asks the recorder's client image provider for a texture version, and *drops
the draw* when there is none. `SkiaGraphiteContext::makeRecorderOptions()`
installs a provider that promotes on first use and caches by image ID and
mipmap flag, so a raster atlas uploads once rather than per draw; every
backend factory here passes it. Any recorder created outside this plumbing
with a bare `makeRecorder()` will silently swallow raster-image draws.

That provider carries two details worth knowing. Its cache pins textures
until a crude full evict at 256 entries — sized for a handful of long-lived
generated atlases, so a host that churns thousands of distinct images should
revisit it. And it retries `kRGBA_F32_SkColorType` sources as an F16 copy,
because F32 textures are not filterable on Apple GPUs and would otherwise
fail promotion outright.

**Ownership of the Metal handles.** `createMetal` retains the device and
queue for the context's lifetime (that translation unit compiles without
ARC, so the retain is explicit); the caller keeps its own references and
stays the owner.

**A null canvas means the wrap failed.** `SkiaOffscreenSurface::canvas()`
returns null in that case — check it before drawing.

**`SIGILSKIA_GLYPH_ATLAS_BYTES`** caps the Graphite glyph-atlas texture
budget from the environment. Unset leaves Skia's default in place; it exists
so the budget can be varied while measuring.

## Boundary

Public dependency: Skia. `SpellCircleSkiaQt` adds `Qt6::Core` publicly and
uses `Qt6::GuiPrivate` for the `QRhi` API; on Apple, `SpellCircleSkia` links
Foundation.

Nothing here knows what is being drawn. Scene content lives in
`src/spellcircle/shared/scene/SceneRenderer.cpp` and
`src/spellcircle/qt/src/SkiaSceneBackend.cpp`, which link against this.

## Building

Added only when `SPELLCIRCLE_ENABLE_SKIA_CANVAS` is on. There are no test or
executable targets in this directory; the code is exercised through the
products and through `compose_gpu_test` and `scry_gpu_test`.

At run time the Metal path needs a Metal device — on macOS, everything.
The Vulkan translation units are a bring-up draft: they compile only on
non-Apple builds and are untested, and the path is selected by running Qt
with `QSG_RHI_BACKEND=vulkan`.
