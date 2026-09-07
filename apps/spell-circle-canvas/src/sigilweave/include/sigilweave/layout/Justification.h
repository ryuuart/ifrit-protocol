#pragma once

/** @file
 * @ingroup layout
 *
 * HOW A JUSTIFIED LINE IS FITTED: the three passes a line spends its
 * slack in, and the limits each of them works between.
 */

#include <cstdint>

namespace sigil::weave {

/** Controls spacing when TextAlignment::kJustify is selected.
 *
 * A justified line is fitted in three passes, each spending only what the
 * one before it could not: the WORD GAPS move first, from their desired
 * width towards the near limit; then LETTER SPACING is added between the
 * glyphs; then the glyphs themselves are SCALED across. Shrinking runs the
 * same order. A pass whose limits equal its desired value contributes
 * nothing and costs nothing — which is why a caller who sets none of them
 * gets word spacing alone, as this stage has always done.
 */
struct JustificationOptions {
  /// Paragraph-final and hard-break-final lines use this alignment unless
  /// `justifyLastLine` requests full justification.
  TextAlignment lastLineAlignment = TextAlignment::kStart;
  bool justifyLastLine = false;  ///< stretch final lines to full measure too

  /// CJK text has no spaces, so eligible zero-width ideographic gaps may be
  /// expanded up to `maxIdeographicExpansion * fontSize` per gap.
  bool expandIdeographicGaps = true;
  float maxIdeographicExpansion = 0.5f;  ///< per-gap cap, fraction of fontSize

  /// A JUSTIFIED LINE IS FITTED IN THREE PASSES — the word gaps, then
  /// letter spacing between the glyphs, then a horizontal scale on the
  /// glyphs — and each spends only what the one before it could not. Every
  /// pass's DESIRED value widens the line before any of them is fitted, and
  /// its two limits bound what it may add on top of that.
  ///
  /// THE GAPS ARE BOUNDED BY `spaceStretch` ONLY WHERE A LATER PASS CAN
  /// SPEND WHAT THEY MAY NOT — where the letter or glyph limits leave room
  /// past what those passes were asked for. With both shut, a bound on the
  /// gaps would open a hole at the right margin that nothing in the line is
  /// allowed to close, and a hole is worse than a wide gap. What a later
  /// pass then FAILS to spend — because it reached its own limit — goes
  /// back to the gaps for the same reason: the bound stood on the claim
  /// that a later pass takes what the gaps drop, and where that claim
  /// fails the bound goes with it. So a justified line reaches its measure
  /// whatever the limits are, and the limits decide only how much of the
  /// fit stands between the words and how much between the letters.
  ///
  /// ROOM ABOVE A DESIRED VALUE IS ROOM THE FIT SPENDS. A glyph scale of
  /// 0.92 with the limits left at 1 is a scale of 1 on every line that
  /// needed widening, because the pass reaches through its range before
  /// the gaps take anything back. A value meant to HOLD says so with its
  /// limits: pin them either side of it and it is what every justified
  /// line is set at.

  /// The width a justified word gap is AIMED at, as a multiple of the
  /// shaped space width; the elasticity below is measured from it, so the
  /// gap may run from `wordSpacing · (1 - spaceShrink)` to
  /// `wordSpacing · (1 + spaceStretch)`.
  float wordSpacing = 1.0f;
  float spaceStretch = 0.5f;   ///< maximum stretch, as a fraction
  float spaceShrink = 0.333f;  ///< maximum shrink, as a fraction

  /// Letter spacing the second pass may add or remove, as fractions of the
  /// em. `letterSpacing` is applied to every justified line whatever its
  /// fit; the two limits bound what the pass may add on top, and the pass
  /// may always undo its own desired value where the line will not take
  /// it. All three zero leaves the pass out.
  float letterSpacing = 0;
  float letterSpacingMinimum = 0;
  float letterSpacingMaximum = 0;

  /// Horizontal glyph scale the third pass may reach for. `glyphScale` is
  /// applied to every justified line; the limits bound the pass. All three
  /// at 1 leaves it out. Scaling letters is the last thing a page should
  /// do and the defaults never do it.
  float glyphScale = 1.0f;
  float glyphScaleMinimum = 1.0f;
  float glyphScaleMaximum = 1.0f;

  /// A line holding ONE word has no gaps to spend: `kAlign` leaves it at
  /// the block's alignment, `kJustify` stretches it across the measure with
  /// letter spacing alone.
  enum class SingleWord : uint8_t { kAlign, kJustify };
  SingleWord singleWord = SingleWord::kAlign;

  bool operator==(const JustificationOptions&) const = default;
};

}  // namespace sigil::weave
