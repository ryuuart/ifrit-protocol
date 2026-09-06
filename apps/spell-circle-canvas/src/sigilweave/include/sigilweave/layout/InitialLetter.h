#pragma once

/** @file
 * @ingroup layout
 *
 * THE INITIAL LETTER: a block's opening set large enough to span several
 * lines, with the lines beneath it wrapping the notch it cuts.
 *
 * The two numbers a dropped or raised initial is made of — the size that
 * makes its cap height span N lines, and which line's baseline it sits on —
 * exist only where the block's pitch and the cap face's own metrics are, so
 * they are answered here rather than guessed by a caller. Declare one on
 * ParagraphStyle::initial; the layout sizes it, cuts the notch out of the
 * bands it covers, shapes its glyphs, and reports where it put them in
 * ParagraphLayout::initial.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>

#include <cstdint>
#include <optional>

#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/style/TextStyle.h"

namespace sigil::weave {

class FontContext;

/**
 * A BLOCK'S OPENING, SET LARGE — how tall, how far down, how much of the
 * text, against which metric, and how close the following lines come.
 *
 * THE SIZING RULE, which is the whole reason this is a layout value and not
 * a font size someone picked: the initial's top reference point is aligned
 * with the FIRST LINE'S top reference point, and its baseline is aligned
 * with the baseline of the line it sinks to. Those two alignments fix the
 * distance the initial's reference metric must span, and the font size
 * follows from the face's own ratio for that metric. A letter chosen by eye
 * is wrong per typeface, because ascent, descent and cap height differ
 * between faces at one size; a letter sized by the rule is right in every
 * face.
 */
struct InitialLetter {
  /// WHICH REFERENCE METRIC the two alignments are made on. Latin setting
  /// aligns cap heights, Han setting aligns em boxes, and a hanging script
  /// aligns the ascent its characters hang from.
  enum class Align : uint8_t { kAlphabetic, kIdeographic, kHanging };
  /// How the following lines meet the initial: the notch is the initial's
  /// advance box, or the outline of its own glyphs, so a line may tuck
  /// under the diagonal of an A.
  enum class Wrap : uint8_t { kBox, kGlyph };

  /// How many lines the initial's reference metric spans; 0 declares none.
  /// Fractional sizes are legal — 2.5 is two and a half lines of cap.
  float lines = 0;
  /// How many lines BELOW THE FIRST BASELINE the initial's own baseline
  /// sits: 1 puts it on the second line's baseline, 0 leaves it on the
  /// first (a raised initial), and a negative number lifts it above the
  /// first. Unset drops it by `lines` rounded down less one, which lands
  /// the baseline on the last line the initial spans — the dropped cap.
  std::optional<int> sink;
  /// How many GRAPHEME CLUSTERS of the block's opening the initial takes.
  /// A cluster is what a reader calls a letter, so an accented capital and
  /// a digraph count as one and two.
  uint32_t graphemes = 1;
  Align align = Align::kAlphabetic;
  Wrap wrap = Wrap::kBox;
  /// How far the following lines stand off the initial, px.
  float margin = 0;
  /// What the initial is set in; unset sets it in the style the block's
  /// opening already carries, at the derived size.
  std::optional<TextStyle> style;

  bool operator==(const InitialLetter&) const = default;
};

/** THE SIZE AN INITIAL LETTER IS SET AT, from the rule rather than by eye.
 *
 * @p firstLineReference is the FIRST LINE'S own reference metric — its cap
 * height under kAlphabetic, its em box under kIdeographic, its ascent under
 * kHanging. The initial's reference metric must reach from the first line's
 * reference point down to the baseline `lines` lines later, so it spans
 * `(lines - 1) · linePitch + firstLineReference`, and the size that gives
 * the face that span is what comes back.
 *
 * Public because a caller drawing its own ornament in the initial's place
 * wants the same number.
 */
[[nodiscard]] float initialLetterSize(FontContext& fontContext,
                                      const TextStyle& cap, float linePitch,
                                      float lines, float firstLineReference,
                                      InitialLetter::Align align);

/** WHERE THE LAYOUT PUT THE INITIAL, and what it took to put it there.
 *
 * The initial's glyphs are ordinary runs of the layout — they are in
 * `ParagraphLayout::runs` and draw with everything else — so this is the
 * report a caller reads to rule a page against the initial, not a second
 * thing to draw.
 */
struct PlacedInitial {
  bool placed = false;    ///< false when the block declared none
  SkRect box = SkRect::MakeEmpty();  ///< the initial's advance box, margin
                                     ///< excluded, in flow coordinates
  SkPoint baseline = {0, 0};  ///< where the initial's pen sat
  float fontSize = 0;         ///< the size the rule derived
  int bands = 0;              ///< how many bands the notch cut
  float notch = 0;            ///< pen travel the notch took on those bands
  /// One past the last UTF-16 unit of the text the initial took.
  uint32_t textEnd = 0;
};

}  // namespace sigil::weave
