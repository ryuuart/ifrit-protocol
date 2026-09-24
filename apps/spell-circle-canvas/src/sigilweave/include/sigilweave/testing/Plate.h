#pragma once

/** @file
 * @ingroup weave-testing
 *
 * A raster plate a passage is drawn onto through Skia's CPU rasterizer,
 * so what it holds is a function of the layout, the faces and the draw
 * alone and never of a device — and which faces those were, since a face
 * is the one input the machine decides.
 */

#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <include/core/SkSurface.h>

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "sigilweave/testing/Passage.h"

class SkCanvas;
class SkNWayCanvas;
class SkPictureRecorder;

namespace sigil::weave::testing {

/** A RASTER SURFACE CLEARED TO ONE GROUND, premultiplied N32. A scene of
 *  several passages and whatever else the test paints draws into
 *  `canvas()` and reads the plate back once it is done. Every draw is
 *  also witnessed, so the plate can say which faces its text was drawn
 *  in whoever drew it — a passage, a kit label or the test itself.
 *  @silent the size is empty or the surface could not be allocated: the
 *  draws land nowhere and the pixels are empty, which a comparison reads
 *  as a plate of another size. */
class Plate {
 public:
  Plate(SkISize size, SkColor ground);
  ~Plate();
  Plate(Plate&&) noexcept;
  Plate& operator=(Plate&&) noexcept;

  /** The canvas every draw of the plate goes through. */
  [[nodiscard]] SkCanvas* canvas() const;
  /** Draws @p passage's layout with its paragraph's paint. */
  void draw(const Passage& passage) const;
  /** The pixels as they stand, borrowed until the next draw. */
  [[nodiscard]] SkPixmap pixels() const;
  /** A copy of the pixels as they stand. */
  [[nodiscard]] sk_sp<SkImage> image() const;
  [[nodiscard]] SkISize size() const;
  /** EVERY FACE THE PLATE'S TEXT WAS DRAWN IN SO FAR, one line each,
   *  sorted: the family, the PostScript name, the style, the revision the
   *  face's own header states and any variation position it was drawn
   *  at. The machine resolves a family and a fallback to these, so two
   *  plates that list different faces were not drawn from the same
   *  inputs whatever their layout.
   *  @trap Asked with every save the test made restored: the witness
   *  starts again after the question, and a save left open is not carried
   *  into it. A face replaced without its header's revision moving lists
   *  as the face it replaced. */
  [[nodiscard]] std::vector<std::string> faces() const;

 private:
  sk_sp<SkSurface> m_surface;
  std::unique_ptr<SkPictureRecorder> m_witness;
  std::unique_ptr<SkNWayCanvas> m_canvas;
  std::unique_ptr<std::set<std::string>> m_faces;  ///< witnessed so far
};

/** Renders @p passage alone onto a plate of @p size cleared to
 *  @p ground, and answers the image. */
[[nodiscard]] sk_sp<SkImage> render(const Passage& passage, SkISize size,
                                    SkColor ground = SK_ColorWHITE);

}  // namespace sigil::weave::testing
