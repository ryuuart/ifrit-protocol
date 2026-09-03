#pragma once

/** @file
 * SigilCompose typography — a READING set beside the type it reads:
 * `Annotation`, the value `Element::annotate` takes, of which furigana,
 * kenten and a gloss under a phrase are three spellings.
 */

#include <sigilcompose/typography/Selector.h>
#include <sigilcompose/typography/Units.h>
#include <sigilweave/style/TextStyle.h>

#include <string>
#include <vector>

namespace sigil::compose {

/** A READING SET BESIDE THE TYPE IT READS — furigana over a compound,
 *  emphasis dots down a column, a gloss under a phrase.
 *
 *  IT IS PART OF THE TEXT, not a thing standing next to it, and the one
 *  fact that makes it so is `reserve`: the band the reading occupies is
 *  stated BEFORE the base is laid out, from the annotation's own metrics,
 *  and goes into the base's strut. The base is then broken and placed once
 *  with the room already there, and the readings are placed on the result.
 *  Nothing chases anything, and there is no round of convergence to run
 *  out of. `kit::annotate` is the other half of the idea — a sibling that
 *  reserves nothing and stands beside the finished text — and marginalia,
 *  callouts and word labels belong there.
 *
 *  MONO, GROUP AND JUKUGO RUBY ARE THE UNIT CHOICE and nothing else.
 *  `unit::Cluster` gives one reading per character, which is mono ruby;
 *  `unit::Word` gives one per word, which is group ruby; and a base that
 *  BREAKS ACROSS A LINE OR A COLUMN reports its units on both, so its
 *  reading splits with it, in proportion to the base's advance either
 *  side. That is not a special case here — it is what reading the units off
 *  the placement means.
 *
 *  THE SIZE IS THE ANNOTATION'S OWN. `style` is a whole TextStyle, and
 *  there is no fraction of the base's size anywhere in the library: a ruby
 *  set at half the base is a decision, and decisions of that kind are the
 *  caller's. */
struct Annotation {
  /** Which of the base's units are annotated. */
  Selector where;
  /** The granularity the readings map to — cluster for mono ruby, word for
   *  group ruby, sentence or line for a note over a passage. */
  Unit unit = Unit::Cluster;
  /** One reading per addressed unit, in draw order. A LIST OF ONE is used
   *  for every unit, which is how a row of identical emphasis marks is
   *  written; a list shorter than the units leaves the rest bare. */
  std::vector<std::u8string> readings;
  /** The reading's own type. */
  sigil::weave::TextStyle style;
  /** Which side of the type it stands on: `Before` is above a line and to
   *  the RIGHT of a column, `After` below a line and to the LEFT of one —
   *  the sides each writing mode reads its furniture on. */
  enum class Side { Before, After };
  Side side = Side::Before;
  /** Standoff from the type's own band, px. */
  float gap = 0;
  /** Whether the band this reading occupies is put into the base's strut
   *  before the base is laid out. False sets the reading over the type it
   *  reads, which is what an emphasis dot does and a furigana never
   *  does. */
  bool reserve = true;

  bool operator==(const Annotation& other) const {
    return where == other.where && unit == other.unit &&
           readings == other.readings && style == other.style &&
           side == other.side && gap == other.gap && reserve == other.reserve;
  }
};

}  // namespace sigil::compose
