/** @file
 * A texture's image, its image shader, and the sampling crossings.
 */

#include "sigilmaterial/skia/Texture.h"

#include <sigilmaterial/skia/Paint.h>

namespace sigil::material::skia {

sk_sp<SkImage> image(const Texture& texture,
                     std::chrono::duration<double> time) {
  return texture.frameAt(time).image;
}

sk_sp<SkImage> image(const Texture& texture, std::chrono::duration<double> time,
                     skgpu::graphite::Recorder* recorder) {
  return texture.frameAt(time, recorder).image;
}

namespace {

sk_sp<SkShader> imageShader(const Texture& texture, sk_sp<SkImage> picture) {
  if (!picture) return nullptr;
  const SkMatrix placing = toSkMatrix(texture.uv());
  const SkSamplingOptions sampling(toSkFilterMode(texture.sampling()));
  // DATA IS NOT COLOUR-MANAGED: the raw shader hands a program the values
  // the pixels hold, with no conversion between colour spaces and no
  // alpha multiplied into the other channels.
  if (texture.colorSpace() == media::ColorSpace::Data)
    return picture->makeRawShader(toSkTileMode(texture.tileX()),
                                  toSkTileMode(texture.tileY()), sampling,
                                  &placing);
  return picture->makeShader(toSkTileMode(texture.tileX()),
                             toSkTileMode(texture.tileY()), sampling, &placing);
}

}  // namespace

sk_sp<SkShader> shader(const Texture& texture) {
  return imageShader(texture, image(texture));
}

sk_sp<SkShader> shader(const Texture& texture, const FrameData& frame) {
  if (!texture.animated() && !frame.recorder) return shader(texture);
  return imageShader(
      texture, image(texture, std::chrono::duration<double>(frame.seconds),
                     frame.recorder));
}

SkFilterMode toSkFilterMode(Sampling sampling) {
  return sampling == Sampling::Nearest ? SkFilterMode::kNearest
                                       : SkFilterMode::kLinear;
}

SkIRect toSkIRect(const PixelRect& rect) {
  return SkIRect::MakeXYWH(rect.x, rect.y, rect.width, rect.height);
}

}  // namespace sigil::material::skia
