/** @file
 * WHERE A UNIT IS — the band a glyph occupies and the advance box that
 * band and a rest pose make, in ONE body for the three readers that place
 * things beside type: the beatsOf query, the mark resolver and a pass
 * track's per-unit rect. Two spellings of "where the third word is" would
 * put a caret and a highlight in different places on the same line.
 */

#include <include/core/SkFontMetrics.h>
#include <sigilweave/fonts/Shaper.h>  // makeFont — the band's metrics

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "TextPose.h"

namespace sigil::compose {

GlyphBand bandOf(const sigil::weave::ShapedWord* shaped,
                 std::vector<std::pair<BandKey, GlyphBand>>& memo) {
  if (!shaped || !shaped->typeface) return {};
  const BandKey key{shaped->typeface.get(), shaped->fontSize};
  for (const auto& [seen, band] : memo)
    if (seen == key) return band;
  SkFontMetrics metrics;
  sigil::weave::makeFont(shaped->typeface, shaped->fontSize)
      .getMetrics(&metrics);
  // Skia reports the ascent as a NEGATIVE offset from the baseline; the band
  // wants both halves positive.
  const GlyphBand band{-metrics.fAscent, metrics.fDescent};
  memo.emplace_back(key, band);
  return band;
}

/** One glyph's advance box, placed and turned the way the layout placed and
 *  turned it, as an axis-aligned bound.
 *
 *  The box is taken around the ADVANCE CENTRE the rest pose reports, which
 *  is what makes one rule cover all four baselines: a wrapped line and a
 *  mixed-style run differ only in where the centre and the band are, a path
 *  run and a rotated column run differ only in which way the box is turned,
 *  and an upright vertical glyph's advance runs down the column instead of
 *  across it. ONE body for three readers — the beatsOf query, the mark
 *  resolver and a pass track's uUnitRect — so none can disagree about
 *  where a unit is. */
SkRect glyphBox(const sigil::weave::PlacedGlyph& placed, const RestPose& pose,
                const GlyphBand& band) {
  const float size = placed.shaped ? placed.shaped->fontSize : 0.0f;
  const bool upright = placed.shaped && placed.shaped->vertical;
  const float halfAlong = std::abs(placed.advance) * 0.5f;
  // Along the advance, then across it. An upright vertical glyph advances
  // DOWN its column and is about one em wide across it; everything else
  // advances along its baseline and stands `band` tall across it.
  const float x0 = upright ? -size * 0.5f : -halfAlong;
  const float x1 = upright ? size * 0.5f : halfAlong;
  const float y0 = upright ? -halfAlong : -band.ascent;
  const float y1 = upright ? halfAlong : band.descent;
  SkRect box = SkRect::MakeEmpty();
  bool first = true;
  for (const SkPoint corner :
       {SkPoint{x0, y0}, SkPoint{x1, y0}, SkPoint{x1, y1}, SkPoint{x0, y1}}) {
    const SkPoint at{
        pose.centre.x() + corner.x() * pose.cosine - corner.y() * pose.sine,
        pose.centre.y() + corner.x() * pose.sine + corner.y() * pose.cosine};
    if (first) {
      box = SkRect::MakeLTRB(at.x(), at.y(), at.x(), at.y());
      first = false;
    } else {
      box.fLeft = std::min(box.fLeft, at.x());
      box.fTop = std::min(box.fTop, at.y());
      box.fRight = std::max(box.fRight, at.x());
      box.fBottom = std::max(box.fBottom, at.y());
    }
  }
  return box;
}

}  // namespace sigil::compose
