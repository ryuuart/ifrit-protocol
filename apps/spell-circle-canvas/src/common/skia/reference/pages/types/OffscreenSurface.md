---
kind: type
library: SigilSkia
name: OffscreenSurface
qualified: sigil::skia::OffscreenSurface
group: Bring-up
status: stable
---

# OffscreenSurface

Wraps an existing native texture (an offscreen canvas texture, a
CAMetalLayer drawable, a swapchain image) in an SkSurface without
copying it, so SkCanvas draw calls land directly in that texture.
Construct fresh per use — it is a thin, cheap wrapper around a texture
someone else owns — and drive it with the context it was made on.

The wrap is per graphics API, one constructor each, both Qt-free —
those are the escape hatch for a host that holds the API's own object.
A host whose textures are named by a GpuDevice hands the handle
instead and never spells an API. A QRhiTexture is wrapped through
`<sigilskia/qt/QtInterop.h>`.

## Make one

The Metal wrap takes an `id<MTLTexture>` bridged to `void*`, created on
the same device the context was built from. The Vulkan wrap takes a
`sigil::skia::VulkanImage` created on that same device, and leaves
`canvas()` null when the build's Skia carries no Vulkan backend.

The third takes a `core::hardware::TextureHandle` on a
`core::hardware::GpuDevice`, whichever API that device is: the wrap a host
holding a GpuDevice reaches for, in place of the native handle its API
spells. `canvas()` is null when the handle is stale or the wrap failed. A
Vulkan image is wrapped in the layout the device last knew it to be in —
undefined for one the device made and nothing has drawn into, which is to
say its contents before the first draw are not preserved. That
constructor is defined by the hardware feature's device value.

A `sigil::skia::VulkanImage` is a Vulkan image to draw into, as opaque
values so the header pulls in no Vulkan header: `image` is the VkImage (a
64-bit non-dispatchable handle), `layout` and `format` are the
VkImageLayout and VkFormat enumerators the image currently has, and the
size is in pixels. The image must have been created for colour
attachment, input attachment, sampling and transfer in both directions —
the set a GpuDevice creates with — and its memory stays the caller's.

## Moving one

A moved-from surface holds neither the surface nor the context:
`canvas()` and `surface()` are null and every `submit()` on it does
nothing, so the recorder's work is only ever submitted by the surface that
was moved into.

## Submitting

`sigil::skia::OffscreenSurface::submit` snaps the Recorder's accumulated
draw commands into a Recording, inserts it into the Context, and submits
it to the GPU asynchronously. Safe because Graphite shares the host's
command queue: the host's later GPU work is ordered after this submission
on the same queue.

The overload taking a device and a fence submits as `submit()` does, then
queues a signal of that fence on that device behind it and returns the
value the fence will reach (`kFenceInitialValue` for a stale handle, and
for a surface that has been moved from, which submits nothing to signal).
Graphite shares the device's one queue, so the value is reached only once
this frame's drawing has landed. The wait for it belongs on another queue
or on the CPU: a wait queued on this same queue ahead of the signal sits
behind it and never passes. That overload is defined by the device
feature.

## The canvas is already fenced

`sigil::skia::OffscreenSurface::canvas` is null if wrapping the backend
texture failed. Draws described through it keep the order they were
described in, whatever the backend does with them: the canvas is made on
the first call and kept, because it carries the fence's own state across
the draws of one frame.
`sigil::skia::OffscreenSurface::surface` is the wrapped surface itself,
for a readback or a snapshot; null if wrapping failed.

## See also

- `graphite/OffscreenSurface.h` — the header: `OffscreenSurface`,
  `VulkanImage`
- [PaintOrderCanvas](value:sigil::skia::PaintOrderCanvas) — what
  `canvas()` hands back, and why
- [GraphiteContext](value:sigil::skia::GraphiteContext) — the context a
  wrap is driven with
