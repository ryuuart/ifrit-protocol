#pragma once

/** @file
 * THE LOG PANEL a study prints into: N feeds of one monospaced voice on
 * one bordered plate.
 */

#include <include/core/SkColor.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Feed.h>
#include <sigilcompose/core/Paint.h>
#include <sigilsketch/kit/Theme.h>

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace sigil::sketch::kit {

/** THE CONSOLE'S CHROME AND ITS VOICE. The rows themselves are the
 *  caller's rings, which outlive the description as a feed's always
 *  do. */
struct Console {
  /** In reading order. A null feed is skipped. */
  std::vector<const compose::feed::TextRing*> feeds;
  /** Feeds per column: 1 (default) gives each its own. */
  int stacked = 1;
  /** How many rows of each feed are built at all. */
  size_t visible = 24;
  /** The colours a row may name, beside the base one every unnamed row is
   *  set in. The names are the caller's rows' — this library has no
   *  vocabulary of levels, because no two studies agree on one. */
  std::vector<std::pair<std::string, SkColor4f>> levels;
  /** Unset is the theme's caption-label size, in the face a CALL is set
   *  in. */
  std::optional<float> size;
  /** Unset is the theme's ink. */
  std::optional<SkColor4f> ink;
  /** Unset is the theme's cell ground. */
  std::optional<compose::Fill> ground;
  /** Unset is the theme's rule. Both the keyline and the dividers between
   *  columns. */
  std::optional<compose::Fill> border;
};

/** THE PANEL.
 *
 *      sketch::kit::console({.feeds = {&checks, &warnings},
 *                            .levels = {{"fail", kAlarm}}})
 *          .rect({64, 1420, kW - 64, 1576})
 *
 *  IT DOES NOT PLACE ITSELF: a component that decides where it goes
 *  cannot be reused, so the caller gives it a rect or a cell. */
[[nodiscard]] compose::Element console(const Console& panel);

}  // namespace sigil::sketch::kit
