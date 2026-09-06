#pragma once

/** @file
 * The two lines that ANNOUNCE something: the card a picture is titled
 * with, and the header that names one section inside a page.
 */

#include <include/core/SkColor.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/typography/Track.h>
#include <sigilmotion/values/Animated.h>
#include <sigilsketch/kit/Theme.h>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sigil::sketch::kit {

/** ONE LINE OF A CARD: what it says, and — for a card that is PERFORMED
 *  rather than set — the ink it is set in and the beat it rides in on.
 *
 *  THE BEAT IS PER LINE, because a masthead's lines each enter on their
 *  own: one returned Element has no handle on the lines inside it, so a
 *  card whose lines all moved together would be a different picture. A
 *  line written as words alone rests, which is what a set card is:
 *
 *      {.title = {toU8("THE SHIPPING FORECAST")}}
 *      {.title = {.words = toU8("VERTIGO, 1958"), .fx = rise}}
 */
struct Line {
  std::u8string words;
  /** THE LINE'S OWN COLOUR; unset is whatever the theme's palette gives
   *  the register this line is set in. It is here rather than in the
   *  palette because a note ranged at the far edge is often a step
   *  quieter than the subtitle beside it, and one ash cannot say both. */
  std::optional<SkColor4f> ink;
  /** Fades the line in; unset leaves it opaque. */
  std::optional<motion::Animatable<float>> opacity;
  /** Moves the line down, px; unset leaves it where it was laid out. */
  std::optional<motion::Animatable<float>> lift;
  /** An entrance over the line's own glyphs — a rise, a stagger. Unset
   *  attaches none. */
  std::optional<compose::Track> fx;
};

/** THE PROSE A TITLE CARD CARRIES — an eyebrow over a title over a
 *  subtitle, any of which may be absent, and the notes ranged at its far
 *  edge.
 *
 *  It is the header half of a page, standing on its own: what a sketch
 *  that is one picture rather than a run of specimens puts at its top
 *  left, and what a panel inside a page puts at the top of the panel. */
struct TitleCard {
  /** The small tracked line over the title — a family, a source, a
   *  number. */
  Line eyebrow;
  Line title;
  Line subtitle;
  /** THE STACK RANGED AT THE FAR EDGE — the issue time, the bounds a word
   *  in the bulletin stands for, the sources a study was read off — set in
   *  the theme's caption-note register at its row gap, with the LAST note
   *  on the card's last line. Empty leaves the card at its own width;
   *  anything here makes the card a row, and the lines take the rest of
   *  the width. */
  std::vector<Line> notes;
  /** How the lines range against each other. */
  compose::Align align = compose::Align::Start;
  /** A hairline under the block, in the theme's rule colour. */
  bool ruled = false;
  /** The prefix the lines are keyed under — `<key>-eyebrow`, `-title`,
   *  `-subtitle`, `-note0` and so on, so a query can read the card back
   *  and a transition can address one line of it. Empty keys nothing. */
  std::string key;
};

/** THE CARD, set in the theme's registers and spaced by its gaps.
 *
 *      sketch::kit::titleCard({.eyebrow = {toU8("SIGIL \xc2\xb7 COMPOSE")},
 *                              .title = {toU8("THE STROKE ATLAS")},
 *                              .subtitle = {toU8("every rail, at one width")}})
 *
 *  It sizes itself to its lines and takes the width it is given, so it
 *  goes into a column as it is. A missing line is absent and spends no
 *  gap behind it. */
[[nodiscard]] compose::Element titleCard(const TitleCard& card);

/** A SECTION INSIDE THE CONTENT: a name at the left, a remark at the
 *  right, and the rule that fills what the two leave between them. */
struct SectionHeader {
  std::u8string label;
  /** Ranged to the far edge — a count, a unit, a source. */
  std::u8string note;
  /** The hairline that spans the space between the two. false leaves the
   *  space empty, which still ranges the note to the far edge. */
  bool ruled = true;
};

/** THE HEADER, one line high.
 *
 *      sketch::kit::sectionHeader({.label = toU8("DYNAMICS"),
 *                                  .note = toU8("6 presets")})
 *
 *  It stretches across whatever width it is given: the rule is what grows,
 *  so the label stays at the left and the note at the right however wide
 *  the column is. */
[[nodiscard]] compose::Element sectionHeader(const SectionHeader& header);

}  // namespace sigil::sketch::kit
