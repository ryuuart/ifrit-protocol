#include "sigilweave/testing/Plate.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkImageInfo.h>
#include <include/core/SkSurface.h>

namespace sigil::weave::testing {

Plate::Plate(SkISize size, SkColor ground)
    : m_surface(SkSurfaces::Raster(SkImageInfo::MakeN32Premul(size))) {
  if (m_surface) m_surface->getCanvas()->clear(ground);
}

SkCanvas* Plate::canvas() const { return m_surface->getCanvas(); }

void Plate::draw(const Passage& passage) const {
  passage.layout.draw(canvas(), passage.paragraph);
}

SkPixmap Plate::pixels() const {
  SkPixmap pixmap;
  if (m_surface) m_surface->peekPixels(&pixmap);
  return pixmap;
}

sk_sp<SkImage> Plate::image() const {
  return m_surface ? m_surface->makeImageSnapshot() : nullptr;
}

SkISize Plate::size() const {
  return m_surface ? SkISize{m_surface->width(), m_surface->height()}
                   : SkISize{0, 0};
}

sk_sp<SkImage> render(const Passage& passage, SkISize size, SkColor ground) {
  Plate plate(size, ground);
  plate.draw(passage);
  return plate.image();
}

}  // namespace sigil::weave::testing
