#pragma once

/** @file
 * @ingroup weave-testing
 *
 * A raster plate a passage is drawn onto through Skia's CPU rasterizer,
 * so what it holds is a function of the layout, the faces and the draw
 * alone and never of a device.
 */

#include <include/core/SkColor.h>
#include <include/core/SkImage.h>
#include <include/core/SkPixmap.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkSize.h>
#include <include/core/SkSurface.h>

#include "sigilweave/testing/Passage.h"

class SkCanvas;

namespace sigil::weave::testing {

/** A RASTER SURFACE CLEARED TO ONE GROUND, premultiplied N32. A scene of
 *  several passages and whatever else the test paints draws into
 *  `canvas()` and reads the plate back once it is done. */
class Plate {
 public:
  Plate(SkISize size, SkColor ground);

  /** The canvas every draw of the plate goes through. */
  [[nodiscard]] SkCanvas* canvas() const;
  /** Draws @p passage's layout with its paragraph's paint. */
  void draw(const Passage& passage) const;
  /** The pixels as they stand, borrowed until the next draw. */
  [[nodiscard]] SkPixmap pixels() const;
  /** A copy of the pixels as they stand. */
  [[nodiscard]] sk_sp<SkImage> image() const;
  [[nodiscard]] SkISize size() const;

 private:
  sk_sp<SkSurface> m_surface;
};

/** Renders @p passage alone onto a plate of @p size cleared to
 *  @p ground, and answers the image. */
[[nodiscard]] sk_sp<SkImage> render(const Passage& passage, SkISize size,
                                    SkColor ground = SK_ColorWHITE);

}  // namespace sigil::weave::testing
