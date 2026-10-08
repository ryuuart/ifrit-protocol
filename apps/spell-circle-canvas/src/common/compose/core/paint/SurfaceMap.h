#pragma once

/** @file
 * A PAGE PAINTED AS ONE SURFACE MAP. A composer set to a map role paints
 * every material that states a surface as that map of its surface
 * (`material::skia::asMap`) and everything else — flat colours, images,
 * text, layer styles — as a stock surface: the colour itself in the base
 * colour map, and in every other map the value the stock surface options
 * read there (`material::skia::flatContentMap`), over the coverage it
 * painted, so it takes the scene's light and glows only where emission is
 * stated. The canvas below is where that second half happens, so no site
 * that paints ordinary content has to know.
 */

#include <include/core/SkColorFilter.h>
#include <include/core/SkRefCnt.h>
#include <include/utils/SkPaintFilterCanvas.h>
#include <sigilmaterial/core/Material.h>
#include <sigilmaterial/texture/TextureSet.h>

#include <optional>

class SkCanvas;
class SkMatrix;
class SkPaint;
class SkPicture;

namespace skgpu::graphite {
class Recorder;
}

namespace sigil::compose::detail {

/** THE MATERIAL A PAGE PAINTED AS SURFACE MAPS PAINTS AS ITS MAPS, where
 *  @p material states a surface: one stating `SurfaceOptions`, lit or
 *  unlit, as it is, and the unlit surface program as a material whose
 *  surface is stated unlit, so it is its own colour in the emissive map.
 *  None for content that states no surface, which the canvas below paints
 *  as the stock surface. */
std::optional<material::Material> surfacedForMaps(
    const material::Material& material);

/** Marks @p paint as already carrying a surface map, so the canvas below
 *  passes it through as it is. A lit site painting in map mode calls this
 *  on the paint it draws with; the mark is a colour filter that changes
 *  nothing, and the canvas takes it off before the draw. A paint already
 *  holding a colour filter of its own is left unmarked. */
void markSurfaceMap(SkPaint& paint);

/** FORWARDS EVERY DRAW TO THE CANVAS IT WRAPS AS ONE SURFACE MAP. A paint
 *  marked by `markSurfaceMap` passes unchanged. Any other paint is content
 *  on a stock surface: in the base colour map it draws as it is; in every
 *  other map its colour becomes the stock surface's value there at the
 *  coverage it paints, its image filter's output too, and a blend that mixes
 * colours rather than coverage composites over instead, since a normal or a
 *  roughness multiplied or screened into what lies beneath means nothing.
 *  Pictures are played back through it, so a recorded draw is filtered as
 *  it is replayed and a mark recorded with it is honoured.
 *  @trap A layer's own paint is not filtered: a layer effect that makes
 *  colour of its own (a shadow, a glow) over lit content writes that colour
 *  into the map. The wrapped canvas must outlive this one. */
class SurfaceMapCanvas final : public SkPaintFilterCanvas {
 public:
  SurfaceMapCanvas(SkCanvas& target, material::texture::Role role);

  /** The recorder the wrapped canvas draws into, so a device texture is
   *  bound where it stands. */
  skgpu::graphite::Recorder* recorder() const override;

 protected:
  bool onFilter(SkPaint& paint) const override;
  void onDrawPicture(const SkPicture* picture, const SkMatrix* matrix,
                     const SkPaint* paint) override;

 private:
  SkCanvas* m_target;
  /** Maps a colour to the stock surface's value at its own alpha; null in
   *  the base colour map, where content keeps its colour. */
  sk_sp<SkColorFilter> m_ownColour;
};

}  // namespace sigil::compose::detail
