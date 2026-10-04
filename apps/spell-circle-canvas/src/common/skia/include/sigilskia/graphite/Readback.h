#pragma once

/** @file
 * @ingroup skia-graphite
 * Reading a Graphite surface into pixels owned by the caller.
 */

class SkPixmap;
class SkSurface;

namespace sigil::skia {

class GraphiteContext;

/** Submit the recorder's pending draws and synchronously read the whole
 *  @p surface into @p pixels, rescaling with nearest sampling when their
 *  dimensions differ. Call on the context's recording thread; the helper
 *  holds the context lock through submission and completion.
 *  False when the output is invalid, submission fails or readback fails.
 *  A completion that
 *  does not arrive within five seconds fails without writing the output;
 *  its callback remains valid until Graphite completes or cancels it. */
[[nodiscard]] bool readbackPixels(GraphiteContext& context, SkSurface& surface,
                                  const SkPixmap& pixels);

}  // namespace sigil::skia
