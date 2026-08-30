/** @file
 * A graph's inputs changed: numeric values by identifier, images
 * flattened to 8-bit RGBA, text, the output resolution, everything
 * back to its authored default, and the normal-format input read back.
 */

#include <include/core/SkBitmap.h>
#include <include/core/SkImageInfo.h>

#include <cstring>

#include "GraphImpl.h"
#include "Numeric.h"

namespace sigil::substance {

namespace air = SubstanceAir;

bool Graph::set(std::string_view identifier, std::initializer_list<float> v) {
  return set(identifier, std::vector<float>(v));
}

bool Graph::set(std::string_view identifier, const std::vector<float>& value) {
  air::InputInstanceBase* in = m_impl->input(identifier);
  if (!in) return false;
  return withNumeric(*in, [&](auto& numeric, int n) {
    if ((int)value.size() != n) return false;
    using T = std::decay_t<decltype(numeric.getValue())>;
    numeric.setValue(fromFloats<T>(value, n));
    return true;
  });
}

bool Graph::setImage(std::string_view identifier, const sk_sp<SkImage>& image) {
  air::InputInstanceBase* in = m_impl->input(identifier);
  if (!in || !in->mDesc.isImage()) return false;
  auto* imageInput = static_cast<air::InputInstanceImage*>(in);
  if (!image) {
    imageInput->reset();
    return true;
  }
  const int w = image->width(), h = image->height();
  SkBitmap bm;
  bm.allocPixels(
      SkImageInfo::Make(w, h, kRGBA_8888_SkColorType, kUnpremul_SkAlphaType));
  if (!image->readPixels(nullptr, bm.pixmap(), 0, 0)) return false;
  SubstanceTexture texture = {};
  texture.buffer = nullptr;  // the framework allocates; filled below
  texture.level0Width = (unsigned short)w;
  texture.level0Height = (unsigned short)h;
  texture.pixelFormat = Substance_PF_RGBA;
  texture.channelsOrder = Substance_ChanOrder_RGBA;
  texture.mipmapCount = 1;
  air::InputImage::SPtr held = air::InputImage::create(texture);
  if (!held) return false;
  {
    air::InputImage::ScopedAccess access(held);
    for (int y = 0; y < h; ++y)
      std::memcpy((uint8_t*)access->buffer + (size_t)y * (size_t)w * 4,
                  bm.getAddr(0, y), (size_t)w * 4);
  }
  imageInput->setImage(held);
  m_impl->heldImages.push_back(held);
  return true;
}

bool Graph::setText(std::string_view identifier, std::string_view text) {
  air::InputInstanceBase* in = m_impl->input(identifier);
  if (!in || !in->mDesc.isString()) return false;
  static_cast<air::InputInstanceString*>(in)->setString(
      air::string(text.begin(), text.end()));
  return true;
}

bool Graph::setResolution(int log2Width, int log2Height) {
  return set("$outputsize", {(float)log2Width, (float)log2Height});
}

bool Graph::normalsAreDirectX() const {
  air::InputInstanceBase* in = m_impl->input("$normalformat");
  if (!in) return true;
  bool directX = true;
  withNumeric(*in, [&](auto& numeric, int n) {
    if (n != 1) return false;
    if constexpr (std::is_arithmetic_v<
                      std::decay_t<decltype(numeric.getValue())>>)
      directX = (int)numeric.getValue() == 0;
    return true;
  });
  return directX;
}

void Graph::reset() {
  for (air::InputInstanceBase* in : m_impl->instance->getInputs()) in->reset();
}

}  // namespace sigil::substance
