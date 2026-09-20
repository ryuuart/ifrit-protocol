#pragma once

/** @file
 * @ingroup weave-layout
 *
 * THE INITIAL LETTER: a block's opening set large enough to span several
 * lines, with the lines beneath it wrapping the notch it cuts. Declare
 * one on ParagraphStyle::initial; the layout sizes it from the block's
 * pitch and the face's own metrics, cuts the notch, shapes its glyphs
 * and reports where it put them in ParagraphLayout::initial.
 */

#include <include/core/SkPoint.h>
#include <include/core/SkRect.h>

#include <cstdint>
#include <optional>

#include "sigilweave/fonts/Shaper.h"
#include "sigilweave/style/TextStyle.h"
#include "sigilweave/style/Type.h"

namespace sigil::weave {

class FontContext;

/** A BLOCK'S OPENING, SET LARGE — how tall, how far down, how much of
 * the text, against which metric, and how close the following lines
 * come. THE SIZING RULE is the whole reason this is a layout value and
 * not a font size someone picked: the initial's top reference point
 * aligns with the first line's, its baseline with the baseline of the
 * line it sinks to, and the size follows from the face's own ratio.
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
  /// sits: 0 is a raised initial and as high as one goes, and a negative
  /// sink is read as none. Unset drops it by `lines` rounded down less
  /// one — the dropped cap.
  std::optional<int> sink;
  /// How many GRAPHEME CLUSTERS of the block's opening the initial
  /// takes; an accented capital counts one and a digraph two.
  uint32_t graphemes = 1;
  Align align = Align::kAlphabetic;
  Wrap wrap = Wrap::kBox;
  /// How far the following lines stand off the initial, px.
  float margin = 0;
  /// What the initial is set in: a PARTIAL over the style the block's
  /// opening carries, at the derived size. Empty sets it in the opening's
  /// style outright.
  Type style;

  bool operator==(const InitialLetter&) const = default;
};

/** THE SIZE AN INITIAL LETTER IS SET AT, from the rule rather than by
 * eye. @p firstLineReference is the FIRST LINE'S own reference metric
 * under @p align, and the initial's must span
 * `(lines - 1) · linePitch + firstLineReference`. It is public because a
 * caller drawing its own ornament in the initial's place wants the same
 * number. */
[[nodiscard]] float initialLetterSize(FontContext& fontContext,
                                      const TextStyle& cap, float linePitch,
                                      float lines, float firstLineReference,
                                      InitialLetter::Align align);

/** WHERE THE LAYOUT PUT THE INITIAL, and what it took to put it there.
 * @trap The initial's glyphs are ordinary runs in `ParagraphLayout::runs`
 * and draw with everything else, so this is a report and not a second
 * thing to draw.
 */
struct PlacedInitial {
  bool placed = false;               ///< false when the block declared none
  SkRect box = SkRect::MakeEmpty();  ///< the initial's advance box, margin
                                     ///< excluded, in flow coordinates
  SkPoint baseline = {0, 0};         ///< where the initial's pen sat
  float fontSize = 0;                ///< the size the rule derived
  int bands = 0;    ///< how many bands the notch cut, which runs on into
                    ///< the block after this one when this one is shorter
                    ///< than the initial sinks
  float notch = 0;  ///< pen travel the notch took on those bands
  /// One past the last UTF-16 unit of the text the initial took.
  uint32_t textEnd = 0;
};

}  // namespace sigil::weave
