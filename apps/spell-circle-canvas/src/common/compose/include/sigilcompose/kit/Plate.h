#pragma once

/** @file
 * SigilCompose kit — the plate several feeds usually sit on, and the style
 * set that colours their rows. Both are plain composition over the public
 * API: `plate` is a padded, bordered box with hairline dividers, `tinted` a
 * `sigil::weave::StyleSet` of one face and one size whose entries differ
 * only in colour, which is the shape `feed::height` measures exactly.
 */

#include <include/core/SkColor.h>
#include <include/core/SkRefCnt.h>
#include <include/core/SkTypeface.h>
#include <sigilcompose/brush/Decorations.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilweave/style/Style.h>

#include <string>
#include <utility>
#include <vector>

namespace sigil::compose::kit {

/** A style set of one face and one size whose entries differ only in
 *  COLOUR — the shape `feed::height` measures exactly, and the usual shape
 *  for rows that are all one voice with levels marked in ink.
 *
 *  The names mean nothing here, deliberately: callers do not agree on what
 *  the levels are (one reads them as {dim, heading, pass, fail}, another as
 *  {trace, warn, alert}), so there is no fixed vocabulary in the library and
 *  a name is only what the caller's rows say. */
[[nodiscard]] inline sigil::weave::StyleSet tinted(
    const sk_sp<SkTypeface>& face, float size, SkColor4f base,
    std::vector<std::pair<std::string, SkColor4f>> named) {
  sigil::weave::StyleSet set(
      weave::textStyle({.face = face, .size = size, .color = base}));
  for (auto& [name, color] : named)
    set.set(std::move(name),
            weave::textStyle({.face = face, .size = size, .color = color}));
  return set;
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
 *                   .paddingX = 14, .paddingY = 9, .gap = 18,
 *                   .fill = Fill::color(hex(0xe4d9c0, 0.78f)),
 *                   .border = Fill::color(hex(0x241c15, 0.25f)),
 *                   .divider = Fill::color(hex(0x241c15, 0.18f))})
 *          .rect({64, 1420, kW - 64, 1576})
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
  float paddingX = 10.0f, paddingY = 8.0f;
  float gap = 12.0f;
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
   *  equally with grow(1).
   *
   *  There is deliberately no third "size each column to its content" mode.
   *  A feed sizes itself from `Options::visible`, so a column's natural
   *  extent normally EXCEEDS the padded interior — and in that case flex
   *  `shrink` (which defaults to 1) distributes the deficit across exactly
   *  the sizes grow(1) would distribute a surplus across, making the two
   *  indistinguishable. They diverge only for columns whose natural extent
   *  is SMALLER than the interior, where grow(1) stretches them and pushes
   *  later siblings along; give those a `columnExtent`. */
  float columnExtent = 0.0f;
};

[[nodiscard]] inline Element plate(Plate p) {
  Element ground = box().fill(p.fill);
  if (p.border.kind != Fill::Kind::None)
    ground.stroke(stroke(p.borderWidth, p.border, p.borderAlign));

  // grow(1) so the padded interior fills whatever rect the caller gave the
  // plate, without the interior needing its own size.
  Element inner = box().padding(p.paddingX, p.paddingY).gap(p.gap).grow(1);
  if (p.column)
    inner.column();
  else
    inner.row();

  const bool dividers = p.divider.kind != Fill::Kind::None;
  bool first = true;
  for (Element& col : p.columns) {
    if (!first && dividers)
      inner.child(p.column ? box().height(p.dividerWidth).fill(p.divider)
                           : box().width(p.dividerWidth).fill(p.divider));
    first = false;
    // Shared-space columns go in DIRECTLY with grow(1) — no wrapper box. The
    // difference is not cosmetic: as a flex child of the row a feed also
    // stretches to the interior height, where inside a wrapper it would take
    // its content height and leave the rest of the cell empty.
    if (p.columnExtent <= 0) {
      col.grow(1);
      inner.child(std::move(col));
      continue;
    }
    Element cell = box().child(std::move(col));
    if (p.column)
      cell.height(p.columnExtent);
    else
      cell.width(p.columnExtent);
    inner.child(std::move(cell));
  }
  ground.child(std::move(inner));
  return ground;
}

}  // namespace sigil::compose::kit
