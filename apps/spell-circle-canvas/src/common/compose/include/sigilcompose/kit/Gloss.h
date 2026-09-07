#pragma once

/** @file
 * THE SATIN BAND that follows the shape, and the two unit-space ramps a
 * chrome wordmark is filled with.
 *
 * A gloss contour is the shape's own blurred coverage remapped through a
 * 256-entry table — which is what a plain gradient cannot fake, because
 * it follows the shape's distance field rather than a screen axis: on a
 * blob it curves with the blob. The table is SigilMaterial's
 * (`material::kit::contourRing`); what is here is the decoration that
 * paints it, in one image-filter chain, so it composes with the node's
 * other decorations inside a single paint rather than forcing a layer.
 */

#include <include/core/SkCanvas.h>
#include <include/core/SkColor.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Shape.h>
#include <sigilmaterial/kit/LayerStyles.h>
#include <sigilmaterial/skia/Paint.h>

#include <array>
#include <cstdint>

namespace sigil::compose::kit {

/** The sunset-chrome ramp in UNIT space: hand it straight to textFill()
 *  and the hard horizon crosses the capitals at half cap height, whatever
 *  the size —
 *  `text(u8"CHROME", display).textFill(kit::sunsetChromeType())`.
 *  The kit's stops as a linear gradient. */
material::skia::Paint sunsetChromeType();

/** The silver-chrome ramp in unit space, for textFill(). */
material::skia::Paint silverChromeType();

/** The shape's blurred coverage remapped through a 256-entry CONTOUR
 *  table, tinted and clipped inside the shape.
 *
 *  This is the light band in gel and chrome that a plain gradient cannot
 *  fake, because it follows the SHAPE's own distance field rather than a
 *  screen axis: on a blob it curves with the blob. It is one image-filter
 *  chain (blur, then an alpha table), so it composes with the node's other
 *  decorations inside a single paint rather than forcing a layer. */
struct GlossContour {
  SkColor4f color = {1, 1, 1, 0.85f};
  float sigma = 6.0f;
  SkVector offset = {0, -3};
  std::array<uint8_t, 256> table{};

  bool operator==(const GlossContour& o) const {
    return color == o.color && sigma == o.sigma && offset == o.offset &&
           table == o.table;
  }
  float bleed() const { return sigma * 3.0f; }

  void paint(SkCanvas& c, const PaintContext& ctx) const;
};

/** The drop-in gloss band. Attach as a foreground: it reads the node's
 *  outline and paints over the fill. */
GlossContour gloss(SkColor4f color = {1, 1, 1, 0.85f}, float sigma = 6.0f,
                   SkVector offset = {0, -3}, float ringCenter = 0.55f,
                   float ringWidth = 0.35f);

}  // namespace sigil::compose::kit
