#pragma once

/** @file
 * @ingroup compose-kit
 *
 * SigilCompose kit — the plate several feeds usually sit on, and the style
 * set that colours their rows. Both are plain composition over the public
 * API: `plate` is a padded, bordered box with hairline dividers, `tinted` a
 * `sigil::weave::TypeSheet` of one face and one size whose entries differ
 * only in colour, which is the shape `feed::height` measures exactly. `console`
 * is `plate` over `feed::feed`, the verification plate every study prints its
 * checks into.
 */

#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/kit/Feed.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmaterial/color/Color.h>
#include <sigilweave/style/Style.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace sigil::compose::kit {

/** A feed's ROW VOICES by name: a `weave::TypeSheet` whose base, @p base, is
 *  one face at one size and whose entries differ from it only in COLOUR — the
 *  shape `feed::height` measures exactly, and the usual shape for rows
 *  that are all one voice with levels marked in ink. A row names its
 *  entry; no selector rule reaches a feed row, which is set in this
 *  voice alone.
 *
 *  The names mean nothing here, deliberately: callers do not agree on what
 *  the levels are (one reads them as {dim, heading, pass, fail}, another as
 *  {trace, warn, alert}), so there is no fixed vocabulary in the library and
 *  a name is only what the caller's rows say. */
[[nodiscard]] inline sigil::weave::TypeSheet tinted(
    const weave::Type& base,
    std::vector<std::pair<std::string, material::Color>> named) {
  sigil::weave::TypeSheet sheet(weave::textStyle(base));
  // Each level is an entry over the base: the colour alone, the face and
  // the size the base's.
  for (auto& [name, color] : named)
    sheet.set(std::move(name), weave::Type{.color = color});
  return sheet;
}

// ---------------------------------------------------------------------------
// The plate several feeds usually sit on.

/** A bordered plate: N elements laid on one axis with hairline dividers
 *  between them, inside a padded box with a fill and an inner stroke.
 *
 *  **It does not place itself.** A component that decides where it goes
 *  cannot be reused, so this returns an element and the caller positions it:
 *
 *      kit::plate({.columns = {feed::feed(logA, style),
 *                               feed::feed(logB, style)},
 *                   .paddingHorizontal = 14, .paddingVertical = 9, .gap = 18,
 *                   .fill = Fill::color(material::hexColor(0xe4d9c0, 0.78f)),
 *                   .border = Fill::color(material::hexColor(0x241c15, 0.25f)),
 *                   .divider = Fill::color(material::hexColor(0x241c15,
 * 0.18f))}) .rect({64, 1420, kW - 64, 1576})
 *
 *  It covers one axis only. A titled plate, or a grid of plates, is a line
 *  of ordinary composition around this one — nest two `plate()`s in a row
 *  rather than looking for a mode here. */
struct Plate {
  /** In order along the axis. */
  std::vector<Element> columns;
  /** false (default) lays the columns out as a ROW; true stacks them
   *  vertically. Dividers follow the axis either way. */
  bool column = false;
  /** Lengths, so a sheet's properties can state them. */
  Dimension paddingHorizontal = 10.0f, paddingVertical = 8.0f;
  Dimension gap = 12.0f;
  /** The plate's own ground and its keyline. */
  Fill fill, border;
  float borderWidth = 1.0f;
  /** Inner (default) keeps the keyline inside the silhouette; Center makes
   *  it straddle the edge, which moves every pixel along the border by half
   *  a stroke width. It is a parameter because the two are visibly different
   *  at a 1 px keyline, not because either is more correct. */
  PathFormat::Align borderAlign = PathFormat::Align::Inner;
  /** Fill::none() (default) means no dividers. */
  Fill divider;
  float dividerWidth = 1.0f;
  /** Fixed extent per column across the stacking axis; 0 shares the space
   *  equally with flexGrow(1).
   *
   *  There is deliberately no third "size each column to its content" mode.
   *  A feed sizes itself from `Options::visible`, so a column's natural
   *  extent normally EXCEEDS the padded interior — and in that case flex
   *  `flexShrink` (which defaults to 1) distributes the deficit across exactly
   *  the sizes flexGrow(1) would distribute a surplus across, making the two
   *  indistinguishable. They diverge only for columns whose natural extent
   *  is SMALLER than the interior, where flexGrow(1) stretches them and pushes
   *  later siblings along; give those a `columnExtent`. */
  float columnExtent = 0.0f;
};

[[nodiscard]] inline Element plate(Plate options) {
  Element ground = box().fill(options.fill);
  if (options.border.kind != Fill::Kind::None)
    ground.stroke(
        stroke(options.borderWidth, options.border, options.borderAlign));

  // flexGrow(1) so the padded interior fills whatever rect the caller gave the
  // plate, without the interior needing its own size.
  Element inner =
      box()
          .padding(options.paddingVertical, options.paddingHorizontal)
          .gap(options.gap)
          .flexGrow(1);
  if (options.column)
    inner.column();
  else
    inner.row();

  const bool dividers = options.divider.kind != Fill::Kind::None;
  bool first = true;
  for (Element& col : options.columns) {
    if (!first && dividers)
      inner.children(
          {options.column
               ? box().height(options.dividerWidth).fill(options.divider)
               : box().width(options.dividerWidth).fill(options.divider)});
    first = false;
    // Shared-space columns go in DIRECTLY with flexGrow(1) — no wrapper box.
    // The difference is not cosmetic: as a flex child of the row a feed also
    // stretches to the interior height, where inside a wrapper it would take
    // its content height and leave the rest of the cell empty.
    if (options.columnExtent <= 0) {
      col.flexGrow(1);
      inner.children({std::move(col)});
      continue;
    }
    Element cell = box().children({std::move(col)});
    if (options.column)
      cell.height(options.columnExtent);
    else
      cell.width(options.columnExtent);
    inner.children({std::move(cell)});
  }
  ground.children({std::move(inner)});
  return ground;
}

// ---------------------------------------------------------------------------
// The console several check feeds print into.

/** THE CONSOLE a study prints its checks into: N feeds of one voice on
 *  one plate, each in a column of its own — or `stacked` to a column,
 *  the first `stacked` feeds down the first column and so on — inside
 *  the plate's chrome. It is `plate()` over `feed::feed()` with the voice
 *  threaded through once, which is what every verification plate wrote,
 *  each a little differently.
 *
 *      kit::console({.feeds = {&logA, &logB, &logC, &logD},
 *                    .style = voice,
 *                    .stacked = 2,
 *                    .plate = {.paddingHorizontal = 12, .paddingVertical = 8,
 * .gap = 14, .fill = Fill::color(material::hexColor(0x1b1e26, 0.86f)), .border
 * = Fill::color(material::hexColor(0xc7ab74, 0.22f)), .divider =
 * Fill::color(material::hexColor(0xc7ab74, 0.16f))}}) .rect(1383, 882, 690,
 * 468)
 *
 *  `plate.columns` is the console's to fill; anything in it is replaced.
 *  A feed left null is skipped. The rings are the caller's and outlive
 *  the description, as a feed's always do. */
struct Console {
  /** In reading order. */
  std::vector<const feed::TextRing*> feeds;
  /** The voice every row is set in — a `tinted` set, usually. */
  feed::TextOptions style;
  /** Feeds per column: 1 (default) gives each its own. */
  int stacked = 1;
  /** Between feeds stacked in one column, px. */
  float stackGap = 6.0f;
  /** The chrome: padding, gap, ground, keyline, dividers, the axis. */
  Plate plate;
};

[[nodiscard]] inline Element console(Console options) {
  const size_t per = (size_t)std::max(options.stacked, 1);
  std::vector<Element> columns;
  for (size_t i = 0; i < options.feeds.size(); i += per) {
    if (per == 1) {
      if (options.feeds[i])
        columns.push_back(feed::feed(*options.feeds[i], options.style));
      continue;
    }
    Element stack = box().column().gap(options.stackGap);
    const size_t end = std::min(i + per, options.feeds.size());
    for (size_t j = i; j < end; ++j)
      if (options.feeds[j])
        stack.children(
            {feed::feed(*options.feeds[j], options.style).flexGrow(1)});
    columns.push_back(std::move(stack));
  }
  options.plate.columns = std::move(columns);
  return plate(std::move(options.plate));
}

}  // namespace sigil::compose::kit
