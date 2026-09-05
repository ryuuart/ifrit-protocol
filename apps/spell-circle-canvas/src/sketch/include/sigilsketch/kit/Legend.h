#pragma once

/** @file
 * COLOUR, NAMED: the key that says what each colour in a picture stands
 * for, the strip that shows a ramp's steps in order, and the chip one
 * word is set inside.
 */

#include <include/core/SkColor.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilsketch/kit/Theme.h>

#include <optional>
#include <string>
#include <vector>

namespace sigil::sketch::kit {

/** ONE ENTRY OF A KEY: a colour and what it stands for. */
struct LegendEntry {
  compose::Fill swatch;
  std::u8string label;
  /** After the label, in the quieter ink. */
  std::u8string note;
};

/** THE KEY TO A PICTURE. */
struct Legend {
  std::vector<LegendEntry> entries;
  /** true (default) stacks the entries; false runs them along a line, for
   *  the key that stands under a plot rather than beside it. */
  bool column = true;
  /** The side of each swatch; unset is the theme's. */
  std::optional<float> swatch;
  /** Between entries; unset is the theme's row gap down a column and its
   *  label gap along a line. */
  std::optional<float> gap;
  /** Rounds every swatch. 0 is the square a drafting key uses. */
  float corners = 0;
  /** Draws each swatch as an OUTLINE of this width rather than as a
   *  filled patch. 0 (default) fills. A key to a map whose own marks are
   *  outlines has to be outlined too, or the key and the map disagree. */
  float strokeWidth = 0;
  /** Lets a run along a line wrap to a second line rather than
   *  overflowing the width it is given. */
  bool wrap = false;
};

/** THE KEY, in the theme's caption registers.
 *
 *      sketch::kit::legend({.entries = {{Fill::color(kWarm), u8"lit"},
 *                                       {Fill::color(kCool), u8"shaded"}}})
 */
[[nodiscard]] compose::Element legend(const Legend& key);

/** A RAMP'S STEPS IN ORDER, each shown at the same size, with the words
 *  that name them under. */
struct SwatchStrip {
  std::vector<compose::Fill> swatches;
  /** Parallel to the swatches, and shorter is allowed: a strip that names
   *  only its ends labels only its ends. An empty label names nothing. */
  std::vector<std::u8string> labels;
  compose::Dim width;
  compose::Dim height;
  /** Between neighbours; unset is the theme's row gap. 0 butts the
   *  swatches, which is what a continuous ramp wants. */
  std::optional<float> gap;
  float corners = 0;
};

/** THE STRIP.
 *
 *      sketch::kit::swatchStrip({.swatches = steps,
 *                                .labels = {u8"0", {}, {}, u8"1"},
 *                                .width = Dim(28), .height = Dim(14),
 *                                .gap = 0})
 */
[[nodiscard]] compose::Element swatchStrip(const SwatchStrip& strip);

/** ONE WORD ON ITS OWN GROUND: a state, a tier, a tag. */
struct Chip {
  std::u8string label;
  /** Unset is the theme's figure colour. */
  std::optional<compose::Fill> ground;
  /** Unset is the theme's page ground, so the word is knocked out of the
   *  chip rather than laid over it. */
  std::optional<SkColor4f> ink;
  float corners = 2;
};

/** THE CHIP, set in the theme's eyebrow register.
 *
 *      sketch::kit::chip({.label = toU8("PINNED")})
 *
 *  It sizes itself to its word plus the theme's chip padding, so a run of
 *  chips in a row is a row of them at their own widths. */
[[nodiscard]] compose::Element chip(const Chip& tag);

}  // namespace sigil::sketch::kit
