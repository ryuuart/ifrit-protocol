/** @file
 * The canvas a page is painted through as one surface map, and the mark a
 * lit site puts on a paint that already carries the map.
 */

#include "SurfaceMap.h"

#include <include/core/SkBlendMode.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkImageFilter.h>
#include <include/core/SkPaint.h>
#include <include/core/SkString.h>
#include <include/effects/SkImageFilters.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Lit.h>

#include <array>
#include <optional>

namespace sigil::compose::detail {

namespace {

/** A colour filter that returns its input: the mark's identity is the
 *  object, which is compared and never run. */
const sk_sp<SkColorFilter>& mark() {
  static const sk_sp<SkColorFilter> filter = [] {
    auto [effect, error] = SkRuntimeEffect::MakeForColorFilter(
        SkString("half4 main(half4 colour) { return colour; }"));
    return effect ? effect->makeColorFilter(nullptr) : nullptr;
  }();
  return filter;
}

/** Whether @p paint composites by coverage alone — the Porter-Duff modes —
 *  rather than mixing its colour with what lies beneath. */
bool compositesByCoverage(const SkPaint& paint) {
  const std::optional<SkBlendMode> mode = paint.asBlendMode();
  if (!mode) return false;
  return *mode <= SkBlendMode::kXor;
}

}  // namespace

void markSurfaceMap(SkPaint& paint) {
  if (!paint.getColorFilter()) paint.setColorFilter(mark());
}

SurfaceMapCanvas::SurfaceMapCanvas(SkCanvas& target,
                                   material::texture::Role role)
    : SkPaintFilterCanvas(&target), m_target(&target) {
  if (role == material::texture::Role::Emissive) return;
  const material::Color ground = material::skia::surfaceMapGround(role);
  // Rows act on unpremultiplied colour: red, green and blue become the
  // ground's, alpha stays, so the premultiplied result is the ground at
  // the coverage the content painted.
  const std::array<float, 20> rows{
      0, 0, 0, 0, ground.r,  //
      0, 0, 0, 0, ground.g,  //
      0, 0, 0, 0, ground.b,  //
      0, 0, 0, 1, 0,
  };
  m_ownColour = SkColorFilters::Matrix(rows.data());
}

skgpu::graphite::Recorder* SurfaceMapCanvas::recorder() const {
  return m_target->recorder();
}

bool SurfaceMapCanvas::onFilter(SkPaint& paint) const {
  if (m_ownColour && !compositesByCoverage(paint))
    paint.setBlendMode(SkBlendMode::kSrcOver);
  if (paint.getColorFilter() == mark().get()) {
    paint.setColorFilter(nullptr);
    return true;
  }
  if (!m_ownColour) return true;
  paint.setColorFilter(m_ownColour->makeComposed(paint.refColorFilter()));
  if (paint.getImageFilter())
    paint.setImageFilter(
        SkImageFilters::ColorFilter(m_ownColour, paint.refImageFilter()));
  return true;
}

void SurfaceMapCanvas::onDrawPicture(const SkPicture* picture,
                                     const SkMatrix* matrix,
                                     const SkPaint* paint) {
  this->SkCanvas::onDrawPicture(picture, matrix, paint);
}

}  // namespace sigil::compose::detail
