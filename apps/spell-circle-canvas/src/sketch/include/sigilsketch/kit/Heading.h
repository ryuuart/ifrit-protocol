#pragma once

/** @file
 * The two lines that ANNOUNCE something: the card a picture is titled
 * with, and the header that names one section inside a page.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilsketch/kit/Theme.h>

#include <string>

namespace sigil::sketch::kit {

/** THE PROSE A TITLE CARD CARRIES — an eyebrow over a title over a
 *  subtitle, any of which may be absent.
 *
 *  It is the header half of a page, standing on its own: what a sketch
 *  that is one picture rather than a run of specimens puts at its top
 *  left, and what a panel inside a page puts at the top of the panel. */
struct TitleCard {
  /** The small tracked line over the title — a family, a source, a
   *  number. */
  std::u8string eyebrow;
  std::u8string title;
  std::u8string subtitle;
  /** How the lines range against each other. */
  compose::Align align = compose::Align::Start;
  /** A hairline under the block, in the theme's rule colour. */
  bool ruled = false;
};

/** THE CARD, set in the theme's registers and spaced by its gaps.
 *
 *      sketch::kit::titleCard({.eyebrow = toU8("SIGIL · COMPOSE"),
 *                              .title = toU8("THE STROKE ATLAS"),
 *                              .subtitle = toU8("every rail, at one width")})
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
