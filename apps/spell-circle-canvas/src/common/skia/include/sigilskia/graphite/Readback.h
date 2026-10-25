#pragma once

/** @file
 * @ingroup skia-graphite
 * Reading a Graphite surface into pixels owned by the caller.
 */

class SkImage;
class SkPixmap;
class SkSurface;

namespace skgpu::graphite {
class Recorder;
}  // namespace skgpu::graphite

namespace sigil::skia {

class GraphiteContext;

/** Submit the recorder's pending draws and synchronously read the whole
 *  @p surface into @p pixels, rescaling with nearest sampling when their
 *  dimensions differ. Call on the context's recording thread; the helper
 *  holds the context lock to submit and each time it asks after
 *  completion, never across the wait, so another thread's inserts and
 *  submits proceed while the read is on the GPU. Where the device raises
 *  an event the host can wait on, the thread sleeps on it until the read
 *  has run, so the call blocks as long as the device takes and no
 *  longer.
 *  False when the output is invalid, submission fails or readback fails.
 *  A completion that
 *  does not arrive within five seconds fails without writing the output;
 *  its callback remains valid until Graphite completes or cancels it. */
[[nodiscard]] bool readbackPixels(GraphiteContext& context, SkSurface& surface,
                                  const SkPixmap& pixels);

/** The same read of an IMAGE standing on @p context's device — a
 *  surface's snapshot, a buffer's picture — whole, into @p pixels, with
 *  the same rescale, lock, deadline and answers. What @p recorder, one of
 *  @p context's recorders, recorded is submitted first, so the image
 *  holds what that recorder drew; call it on @p recorder's thread. */
[[nodiscard]] bool readbackPixels(GraphiteContext& context,
                                  skgpu::graphite::Recorder& recorder,
                                  const SkImage& image,
                                  const SkPixmap& pixels);

}  // namespace sigil::skia
