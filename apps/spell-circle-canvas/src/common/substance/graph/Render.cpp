/** @file
 * A graph cooked and read: render() pushes the instance through the
 * engine and turns every image result into an SkImage, keyed by
 * identifier and by usage; output() and outputsByUsage() read them.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkImageInfo.h>

#include <cstring>

#include "GraphImpl.h"

namespace sigil::substance {

namespace {

namespace air = SubstanceAir;

/** The engine's blend-platform result → an SkImage. RGBA8 becomes an
 *  N32 image, L8 a grey one; anything else (16-bit, float, compressed)
 *  is refused, which the package's output options prevent by only
 *  allowing those two. */
sk_sp<SkImage> imageFrom(const SubstanceTexture& tex) {
  const int w = tex.level0Width, h = tex.level0Height;
  if (!tex.buffer || w <= 0 || h <= 0) return nullptr;
  const unsigned fmt = tex.pixelFormat & Substance_PF_MASK;
  const unsigned channels = fmt & Substance_PF_MASK_RAWChannels;
  const unsigned precision = fmt & Substance_PF_MASK_RAWPrecision;
  if ((fmt & Substance_PF_MASK_RAWFormat) != Substance_PF_RAW ||
      precision != Substance_PF_8I)
    return nullptr;
  if (channels == Substance_PF_L) {
    SkBitmap bm;
    bm.allocPixels(
        SkImageInfo::Make(w, h, kGray_8_SkColorType, kOpaque_SkAlphaType));
    for (int y = 0; y < h; ++y)
      std::memcpy(bm.getAddr(0, y),
                  (const uint8_t*)tex.buffer + (size_t)y * (size_t)w,
                  (size_t)w);
    bm.setImmutable();
    return bm.asImage();
  }
  if (channels != Substance_PF_RGBA && channels != Substance_PF_RGBx)
    return nullptr;
  // Channel order: the engine may hand back RGBA or BGRA; Skia has a
  // colour type for each, so no swizzle pass is needed.
  const SkColorType ct = tex.channelsOrder == Substance_ChanOrder_BGRA
                             ? kBGRA_8888_SkColorType
                             : kRGBA_8888_SkColorType;
  SkBitmap bm;
  bm.allocPixels(SkImageInfo::Make(w, h, ct, kUnpremul_SkAlphaType));
  std::memcpy(bm.getPixels(), tex.buffer, (size_t)w * (size_t)h * 4);
  bm.setImmutable();
  return bm.asImage();
}

}  // namespace

bool Graph::render() {
  Impl& impl = *m_impl;
  impl.renderer->push(*impl.instance);
  impl.renderer->run();
  impl.byIdentifier.clear();
  impl.byUsage.clear();
  const std::vector<Output> described = outputs();
  size_t index = 0;
  for (air::OutputInstance* o : impl.instance->getOutputs()) {
    const Output& d = described[index++];
    air::OutputInstance::Result result(o->grabResult());
    if (!result || !result->isImage()) continue;
    auto* img = static_cast<air::RenderResultImage*>(result.get());
    sk_sp<SkImage> image = imageFrom(img->getTexture());
    if (!image) continue;
    impl.byIdentifier[d.identifier] = image;
    impl.byUsage[d.usage.empty() ? d.identifier : d.usage] = image;
  }
  return !impl.byIdentifier.empty();
}

sk_sp<SkImage> Graph::output(std::string_view name) const {
  const std::string key(name);
  if (auto it = m_impl->byIdentifier.find(key);
      it != m_impl->byIdentifier.end())
    return it->second;
  if (auto it = m_impl->byUsage.find(key); it != m_impl->byUsage.end())
    return it->second;
  return nullptr;
}

boost::container::map<std::string, sk_sp<SkImage>> Graph::outputsByUsage()
    const {
  return m_impl->byUsage;
}

}  // namespace sigil::substance
