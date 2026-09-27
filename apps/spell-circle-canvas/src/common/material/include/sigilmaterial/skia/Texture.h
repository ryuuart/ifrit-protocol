#pragma once

/** @file
 * @ingroup material-skia
 *
 * A texture through Skia: the image it samples, the image shader it
 * binds into a slot as, and the crossings between its sampling words and
 * Skia's. The texture value names no renderer; this is where it meets
 * one.
 */

#include <include/core/SkImage.h>
#include <include/core/SkRect.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkShader.h>
#include <sigilmaterial/advanced/FrameData.h>
#include <sigilmaterial/texture/Texture.h>

#include <chrono>

namespace sigil::material::skia {

/** The image @p texture samples at @p time: its source's frame, read
 *  back to host memory when it stands on a device, cut to the region.
 *  Null when the source yields nothing. */
sk_sp<SkImage> image(const Texture& texture,
                     std::chrono::duration<double> time = {});
/** The same, bound for @p recorder when the source's frame stands on
 *  that recorder's device, so nothing is copied back to host memory. */
sk_sp<SkImage> image(const Texture& texture, std::chrono::duration<double> time,
                     skgpu::graphite::Recorder* recorder);

/** @p texture as an image shader: the image tiled, read between pixels
 *  and placed by its uv matrix. Null when there is no image. */
sk_sp<SkShader> shader(const Texture& texture);
/** The same over the frame the source answers at @p frame's time — what
 *  a slot holding a moving texture binds — bound for `frame.recorder`
 *  when the frame stands on its device. */
sk_sp<SkShader> shader(const Texture& texture, const FrameData& frame);

/** The filter Skia reads a texture between pixels with. */
SkFilterMode toSkFilterMode(Sampling sampling);
/** A pixel rectangle as Skia's, and back. */
SkIRect toSkIRect(const PixelRect& rect);
PixelRect toPixelRect(const SkIRect& rect);

}  // namespace sigil::material::skia
