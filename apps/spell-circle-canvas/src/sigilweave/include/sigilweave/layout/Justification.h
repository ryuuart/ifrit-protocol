#pragma once

/** @file
 * @ingroup weave-layout
 *
 * HOW A JUSTIFIED LINE IS FITTED: the three passes a line spends its
 * slack in, and the limits each of them works between.
 */

#include <cstdint>

namespace sigil::weave {

/** WHERE A JUSTIFIED LINE SPENDS ITS SLACK — CSS's `text-justify`.
 *  `kAuto` is the three passes below as they are tuned; `kInterWord`
 *  spends it in the word separators alone; `kInterCharacter` spends it
 *  after every grapheme cluster but the line's last and every word
 *  separator alike, each opening at most `maxInterCharacterExpansion` of
 *  the size; `kNone` justifies nothing, and a justified line is set at
 *  its start. No method moves the space after a line's last glyph.
 *  @trap The optimizing breaker weighs the gaps' stretch alone, so under
 *  `kInterCharacter` it breaks as it would under `kAuto`. */
enum class JustificationMethod : uint8_t {
  kAuto,
  kInterWord,
  kInterCharacter,
  kNone
};

/** Controls spacing when `TextAlignment::kJustify` is selected. A
 * justified line is fitted in three passes, each spending only what the
 * one before it could not: the WORD GAPS move first, then LETTER SPACING
 * is added between the glyphs, then the glyphs are SCALED across.
 * Shrinking runs the same order, and a pass whose limits equal its
 * desired value contributes nothing and costs nothing.
 */
struct JustificationOptions {
  /// Paragraph-final and hard-break-final lines use this alignment unless
  /// `justifyLastLine` requests full justification.
  TextAlignment lastLineAlignment = TextAlignment::kStart;
  bool justifyLastLine = false;  ///< stretch final lines to full measure too

  /// Where the slack goes. The two methods that name their opportunities
  /// — `kInterWord` and `kInterCharacter` — leave the letter and glyph
  /// passes and `SingleWord::kJustify` out, and a line holding a tab
  /// spends its slack in the gaps past the last tab whatever the method.
  JustificationMethod method = JustificationMethod::kAuto;
  /// The most one opportunity opens under `kInterCharacter`, as a
  /// fraction of the size; what the line needs beyond it goes to the word
  /// separators, and a line with none stays short of the measure.
  float maxInterCharacterExpansion = 0.5f;

  /// CJK text has no spaces, so eligible zero-width ideographic gaps may be
  /// expanded up to `maxIdeographicExpansion * fontSize` per gap.
  bool expandIdeographicGaps = true;
  float maxIdeographicExpansion = 0.5f;  ///< per-gap cap, fraction of fontSize

  // Every pass's DESIRED value widens the line before any of them is
  // fitted, and its two limits bound what it may add on top. The gaps are
  // bounded by `spaceStretch` ONLY where a later pass can spend what they
  // may not, so a justified line reaches its measure whatever the limits
  // are and they decide only where the fit stands. ROOM ABOVE A DESIRED
  // VALUE IS ROOM THE FIT SPENDS: a value meant to HOLD pins its limits
  // either side of itself.

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
