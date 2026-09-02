#pragma once

/** @file
 * WHAT A PAIR OF LETTERS LOOKS LIKE SET TOGETHER, measured off their
 * outlines — optical kerning, and the glyph edge profiles it reads.
 *
 * A face's own kerning is a designer's table of pairs. Optical kerning is
 * the answer when there is no such table, or when the text mixes faces
 * that never met: every pair is set as tight as the face's own even pair,
 * by measuring how close the two outlines actually come.
 *
 * IT IS AN APPROXIMATION AND THIS IS THE WHOLE OF IT. A designer kerns by
 * judging the white between two letters as an area and as a rhythm; this
 * measures the narrowest distance between two edges in bands, and closes
 * every pair to the distance the face's own reference pair leaves. A pair
 * a designer would have opened for legibility, and a pair whose white is
 * wide but shallow, both come out tighter here than a table would set
 * them. The library decides nothing about how tight type should be — the
 * reference is the face's own.
 *
 * Never leaves the fonts directory.
 */

#include <include/core/SkTypeface.h>

#include <array>
#include <cstdint>
#include <vector>

namespace sigil::weave::detail {

/// How many bands a glyph's edges are measured in, over its own ink height.
/// A band is a horizontal slice; the profile is the extreme ink in each.
/// Enough that a diagonal reads as a slope rather than as a box, and few
/// enough that one profile is a cache line or two.
inline constexpr int kProfileBands = 16;

/// ONE GLYPH'S EDGES, in ems: for each band of its ink height, how far the
/// ink reaches from the left edge of the advance and from the right. A
/// band the glyph has no ink in holds `kNoInk`, and a glyph with no ink at
/// all (a space) reports `inked` false and is never kerned against.
struct GlyphProfile {
  static constexpr float kNoInk = 1e9f;
  std::array<float, kProfileBands> left{};   ///< ink's near edge, ems
  std::array<float, kProfileBands> right{};  ///< ink's far edge, ems
  float top = 0;     ///< ink box top, ems above the baseline (positive up)
  float bottom = 0;  ///< ink box bottom, ems
  float advance = 0;  ///< the glyph's own advance, ems
  bool inked = false;
};

/** Measures a glyph's edge profile from its outline, in ems. */
[[nodiscard]] GlyphProfile measureProfile(const SkTypeface& typeface,
                                          uint16_t glyph);

/** The distance between two glyphs set adjacent, in ems: the smallest gap
 *  between the left glyph's right edge and the right glyph's left edge over
 *  the bands they share. Two glyphs whose ink never overlaps vertically
 *  have no such band, and answer `GlyphProfile::kNoInk`. */
[[nodiscard]] float gapBetween(const GlyphProfile& left,
                               const GlyphProfile& right);

}  // namespace sigil::weave::detail
