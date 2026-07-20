#include "sigilsketch/Assets.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkPaint.h>
#include <include/core/SkSurface.h>

namespace sigil::compose::sketch {

namespace {

/** The classic missing-texture checker: magenta/black, unmistakable. */
std::shared_ptr<const sigil::image::ImageAsset> makePlaceholder() {
  constexpr int kSize = 64, kCell = 16;
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSize, kSize));
  SkCanvas &canvas = *surface->getCanvas();
  SkPaint paint;
  for (int y = 0; y < kSize / kCell; ++y)
    for (int x = 0; x < kSize / kCell; ++x) {
      paint.setColor(((x + y) & 1) ? SK_ColorMAGENTA : SK_ColorBLACK);
      canvas.drawRect(SkRect::MakeXYWH((float)(x * kCell),
                                       (float)(y * kCell), kCell, kCell),
                      paint);
    }
  return std::make_shared<sigil::image::ImageAsset>(
      sigil::image::ImageAsset::wrap(surface->makeImageSnapshot()));
}

std::string uriFor(std::string_view name) {
  return "res://" + std::string(name);
}

} // namespace

Assets::Assets(std::filesystem::path root) : m_root(std::move(root)) {
  m_hub.mount("res://", m_root);
  m_placeholder = makePlaceholder();
}

std::shared_ptr<const sigil::image::ImageAsset>
Assets::image(std::string_view name) {
  if (auto asset = m_hub.image(uriFor(name))) {
    m_placeholders.erase(std::string(name));
    return asset;
  }
  m_placeholders.insert_or_assign(std::string(name), true);
  return m_placeholder;
}

bool Assets::poll() {
  bool changed = m_hub.poll();
  // Placeholders heal the moment their file becomes loadable.
  for (auto it = m_placeholders.begin(); it != m_placeholders.end();) {
    if (m_hub.image(uriFor(it->first))) {
      it = m_placeholders.erase(it);
      changed = true;
    } else {
      ++it;
    }
  }
  return changed;
}

} // namespace sigil::compose::sketch
