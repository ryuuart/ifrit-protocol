/** @file
 * A pattern's bake: the element tile shaped into a recording while the
 * fonts are in hand, the tile's texture taken, and the repeating paint
 * wrapped around it under the tile's mapping and sampling.
 */

#include "sigilcompose/core/Pattern.h"

#include <include/core/SkCanvas.h>
#include <include/core/SkImage.h>
#include <include/core/SkPicture.h>
#include <include/core/SkSamplingOptions.h>
#include <include/core/SkTypes.h>
#include <sigilmaterial/skia/Texture.h>

namespace sigil::compose {

material::Material Pattern::bake(sigil::weave::FontContext* fonts) const {
  // A pattern that cannot bake paints nothing.
  const material::Material nothing = material::skia::base(material::Paint{});
  if (!m_tile.valid()) return nothing;
  if (m_tree && !m_tile.baked()) {
    if (!fonts) {
      SkDebugf(
          "Pattern::material(): an element tile needs the "
          "material(FontContext&) overload\n");
      return nothing;
    }
    // The element tile is SHAPED HERE, while the fonts are in hand, and
    // the program is the recording it produced. The tile's state
    // outlives this call — a later seed() or invalidate() re-runs the
    // program — so a program holding the borrowed context would shape
    // against a font context that may be gone. A picture holds
    // everything it draws. (Wrapped so the intrinsic-size root adopts
    // the tile's forced dims.)
    sk_sp<SkPicture> pic = snapshot(box().children({*m_tree}), *fonts);
    m_tile.program(
        material::skia::painted([pic](SkCanvas& canvas, SkSize, uint32_t) {
          if (pic) canvas.drawPicture(pic);
        }));
  }
  sk_sp<SkImage> baked = material::skia::image(m_tile.texture());
  if (!baked) return nothing;
  material::Paint m = material::skia::image(
      std::move(baked), material::Repeat::Repeat, material::Repeat::Repeat,
      material::skia::toSkMatrix(m_tile.mapping()),
      SkSamplingOptions(material::skia::toSkFilterMode(
          m_sampling.value_or(m_tile.sampling()))));
  if (m_boundX || m_boundY)
    m.offset(m_boundX,
             m_boundY);  // the live pan rides material::Paint's
                         // bound-matrix channel
  return material::skia::base(std::move(m));
}

}  // namespace sigil::compose
