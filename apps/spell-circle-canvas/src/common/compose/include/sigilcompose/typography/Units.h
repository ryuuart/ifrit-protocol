#pragma once

/** @file
 * SigilCompose typography — the UNIT of text: the granularity a selector
 * slices and a cascade beats over (`Unit`, spelled `unit::Word`), and one
 * such unit as the layout placed it (`TextUnit`), which is what everything
 * standing beside a passage is placed from.
 */

#include <include/core/SkRect.h>
#include <sigilweave/paragraph/Paragraph.h>
#include <sigilweave/style/ShapingStyle.h>
#include <sigilweave/style/TextStyle.h>

#include <cstdint>

namespace sigil::compose {

/** The granularity a selector slices and a stagger beats over.
 *
 *  `Cluster` is the default and the one that keeps text correct: a base
 *  letter and its combining marks, or the several glyphs an emoji
 *  sequence shapes to, are ONE cluster and move together. `Glyph` is the
 *  raw shaping unit and will separate those marks from what they sit on. */
enum class Unit : uint8_t { Glyph, Cluster, Word, Line, Sentence };

/** The granularity names, spelled the way tracks read: `unit::Word`. */
namespace unit {
inline constexpr Unit Glyph = Unit::Glyph;
inline constexpr Unit Cluster = Unit::Cluster;
inline constexpr Unit Word = Unit::Word;
inline constexpr Unit Line = Unit::Line;
inline constexpr Unit Sentence = Unit::Sentence;
}  // namespace unit

/** ONE UNIT OF A LAID-OUT TEXT, as everything beside the text reads it —
 *  what `Composer::units` reports and what every annotation is placed
 *  from.
 *
 *  A `Beat` is the same rect under a schedule: it needs an `fx()` track, a
 *  stagger and a progress, and it answers about a cascade. This answers
 *  about the TEXT — where a word, a cluster or a line landed, on which
 *  baseline, in which writing mode, set in which style — for a selector and
 *  a unit, with no track anywhere. It is read off the placement rather than
 *  measured again, so it follows a wrapped line, a mixed-style run's own
 *  size, a path run's curve and a vertical column's axis by construction.
 *
 *  ONE ENTRY PER UNIT, in draw order. A selector that addresses several
 *  units reports several entries — which is the whole difference from
 *  `mark()`, whose one rect is the union of them all. */
struct TextUnit {
  /** The unit's laid-out rect in the node's own space: the axis-aligned
   *  bound of the advance boxes of the glyphs the selector addressed in
   *  it. */
  SkRect rect = SkRect::MakeEmpty();
  /** The unit's ordinal among those the selector addressed, from 0 in draw
   *  order. */
  uint32_t index = 0;
  /** HORIZONTAL: the baseline the unit stands on, in y. VERTICAL: the
   *  central axis of the column it stands in, in x — a column has no
   *  baseline, and its glyphs centre themselves across that axis. */
  float axis = 0;
  /** The flow's band depth: the line's pitch, which in a vertical setting
   *  is the width of the column. */
  float pitch = 0;
  /** The band the unit's own face occupies either side of its baseline,
   *  from the face's metrics rather than from its ink — so a unit of
   *  lowercase and a unit of capitals report the same band. */
  float ascent = 0, descent = 0;
  /** Which way the passage runs. */
  sigil::weave::WritingMode writingMode =
      sigil::weave::WritingMode::kHorizontal;
  /** How the unit stands in its column: upright, turned with the column,
   *  or set across it. `kAuto` in a horizontal passage, where the question
   *  does not arise. */
  sigil::weave::VerticalForm verticalForm = sigil::weave::VerticalForm::kAuto;
  /** The text the unit covers, as UTF-16 units into the node's
   *  paragraph. */
  sigil::weave::CharRange range;
  /** The style the unit's first glyph is set in — the annotation's cue for
   *  a size of its own, since the library decides no typographic ratio. */
  sigil::weave::TextStyle style;
  /** The flow line (or COLUMN) the unit landed on. */
  int lineIndex = 0;

  bool operator==(const TextUnit& other) const {
    return rect == other.rect && index == other.index && axis == other.axis &&
           pitch == other.pitch && ascent == other.ascent &&
           descent == other.descent && writingMode == other.writingMode &&
           verticalForm == other.verticalForm && range == other.range &&
           style == other.style && lineIndex == other.lineIndex;
  }
};

}  // namespace sigil::compose
