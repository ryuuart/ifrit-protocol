#pragma once

/** @file
 * SigilCompose KIT — the furniture a page of set text carries, as stock
 * values over the seams underneath: readings beside the type (`ruby`,
 * `kenten`), a block's opening letter dropped into its first lines
 * (`dropCap`), a list whose markers hang in the indent (`bullets`), and
 * rules and shading cut to what a block actually occupies (`rules`).
 *
 * None of these is a mechanism. Ruby and kenten are `Annotation` values —
 * a selector, a unit, a reading and a type — and the whole of what
 * distinguishes mono, group and jukugo ruby is which unit is named. A drop
 * cap is an exclusion the body flows around, which is the same exclusion a
 * photograph gets. A list is a hanging indent with a marker in the hang.
 * A rule is a box at the extent `Composer::units` reports.
 *
 * NOTHING HERE DECIDES A RATIO. A reading's size is its own style's, a
 * drop cap's depth is stated in lines, a marker's inset is stated in
 * pixels: the library carries no fraction of a base's size anywhere,
 * because which fraction is right is a decision and decisions are the
 * caller's.
 */

#include <include/core/SkColor.h>
#include <include/core/SkRect.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Text.h>
#include <sigilweave/style/Style.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::compose::kit {

/** FURIGANA: a reading set over the base it reads.
 *
 *      text(passage, body)
 *          .annotate(kit::ruby(sel::text(u8"漢字"), unit::Word,
 *                              {u8"かんじ"}, furigana))
 *
 *  MONO, GROUP AND JUKUGO ARE THE UNIT: `unit::Cluster` gives one reading
 *  per character, `unit::Word` one per word, and a compound annotated per
 *  cluster with the readings its characters take is jukugo. A base that
 *  breaks across a line or a column carries its reading across with it,
 *  split in proportion to the base's advance either side.
 *
 *  It RESERVES: the band the reading occupies goes into the base's strut
 *  before the base is broken, so the line pitch — or the column pitch —
 *  opens to hold it and the base is laid out once. */
[[nodiscard]] inline Annotation ruby(Selector over, Unit unit,
                                     std::vector<std::u8string> readings,
                                     sigil::weave::TextStyle style,
                                     float gap = 0) {
  return Annotation{.where = std::move(over),
                    .unit = unit,
                    .readings = std::move(readings),
                    .style = std::move(style),
                    .side = Annotation::Side::Before,
                    .gap = gap,
                    .reserve = true};
}

/** KENTEN: one emphasis mark beside each character of a phrase.
 *
 *  The mark is a character like any other — a sesame dot (U+FE45), a
 *  bullet, a ring — set in its own type at whatever size that type says.
 *  It stands on the side the writing mode reads its emphasis on: above a
 *  line, to the right of a column.
 *
 *  It RESERVES NOTHING, deliberately. Emphasis marks sit in the room the
 *  line already has, which is why a marked phrase does not open the pitch
 *  of the paragraph it stands in — and why a passage that wants them clear
 *  of the type asks for leading rather than for a reservation. */
[[nodiscard]] inline Annotation kenten(Selector over,
                                       sigil::weave::TextStyle style,
                                       std::u8string mark = u8"\xef\xb9\x85",
                                       float gap = 0) {
  return Annotation{.where = std::move(over),
                    .unit = Unit::Cluster,
                    .readings = {std::move(mark)},
                    .style = std::move(style),
                    .side = Annotation::Side::Before,
                    .gap = gap,
                    .reserve = false};
}

/** A BLOCK'S OPENING LETTER, dropped into the lines beneath it.
 *
 *      const auto [initial, body] = kit::dropCap(u8"W", capType, 3, bodyType);
 *      root.child(box().child(initial).child(body.width(measure)));
 *
 *  There is no drop-cap facility under this, and there does not need to
 *  be: the initial is a text leaf with a key, and the body is a text leaf
 *  that flows around it — the same exclusion a photograph in a column
 *  gets, resolved in the same pass. `lines` states how deep the initial
 *  goes in lines of the body, and `margin` how far the text stands off it.
 *
 *  The caller splits the string, because where a "letter" ends is a
 *  question about the text: one grapheme usually, two for a digraph, a
 *  whole word for an opening word set large. */
struct DroppedCap {
  Element initial;  ///< the letter, keyed and absolutely placed
  Element body;     ///< the rest, flowing around it
};
[[nodiscard]] inline DroppedCap dropCap(std::u8string letter,
                                        sigil::weave::TextStyle capStyle,
                                        std::u8string rest,
                                        sigil::weave::TextStyle bodyStyle,
                                        std::string key = "dropcap",
                                        float margin = 6.0f) {
  Element initial = text(std::move(letter), std::move(capStyle))
                        .key(key)
                        .absolute()
                        .left(Dim(0.0f))
                        .top(Dim(0.0f));
  Element body =
      text(std::move(rest), std::move(bodyStyle)).flowAround(key, margin);
  return {std::move(initial), std::move(body)};
}

/** A LIST WHOSE MARKERS HANG IN THE INDENT.
 *
 *  One text leaf per item, each with a hanging indent — a start indent
 *  with the first line pulled back out of it — and the marker set in the
 *  room that hang opens. A number is the caller's to format, which is why
 *  `markers` is a list of strings rather than a numbering scheme: the
 *  schemes people want (roman, lettered, restarting, hierarchical) are
 *  data, and this is the shape they are drawn in.
 *
 *  `hang` is the indent in px, and the marker is set at the block's own
 *  start with the item's text beginning one hang in. */
[[nodiscard]] inline Element bullets(std::span<const std::u8string> items,
                                     std::span<const std::u8string> markers,
                                     sigil::weave::TextStyle style, float hang,
                                     float measure, float gap = 4.0f) {
  Element list = box().column().gap(gap);
  sigil::weave::ParagraphStyle hanging;
  hanging.indent.start = hang;
  hanging.indent.firstLine = -hang;
  for (size_t index = 0; index < items.size(); ++index) {
    const std::u8string& marker =
        markers.empty() ? items[index]
                        : markers[std::min(index, markers.size() - 1)];
    list.child(
        box()
            .child(text(items[index], style)
                       .width(Dim(measure))
                       .paragraph(hanging))
            .child(text(marker, style).absolute().left(Dim(0.0f)).top(
                Dim(0.0f))));
  }
  return list;
}

/** N COLUMNS OF ONE STORY, threaded left to right.
 *
 *      root.child(kit::columns(article, 3, 28, 760, 420));
 *
 *  There is no column geometry under this and there does not need to be: a
 *  Western column is a FRAME, and three of them side by side threaded in
 *  order is what a three-column measure means. The vertical writing mode's
 *  own columns are a different thing entirely and keep their word — those
 *  are one frame's lines turned a quarter turn.
 *
 *  Each column takes an equal share of `width` after the gutters, and the
 *  LAST one keeps whatever ellipsis a caller puts on it: the frames before
 *  it overflow by design and draw nothing. */
[[nodiscard]] inline Element columns(Story story, int count, float gutter,
                                     float width, float height,
                                     std::string keyPrefix = "column") {
  Element row = box().row().gap(gutter);
  if (count < 1) return row;
  const float measure =
      (width - gutter * static_cast<float>(count - 1)) / static_cast<float>(count);
  for (int index = 0; index < count; ++index) {
    Element column = frame(story)
                         .key(keyPrefix + std::to_string(index))
                         .width(Dim(measure))
                         .height(Dim(height));
    if (index + 1 < count)
      column.thread(keyPrefix + std::to_string(index + 1));
    row.child(std::move(column));
  }
  return row;
}

/** WHERE A RULE OR A SHADE STANDS relative to the block it dresses. */
struct BlockRule {
  enum class Where { Above, Below, Behind };
  Where where = Where::Above;
  float thickness = 1.0f;   ///< Above/Below: the rule's own weight
  float gap = 4.0f;         ///< Above/Below: clearance from the type
  float inset = 0.0f;       ///< taken off both ends of the extent
  float bleed = 0.0f;       ///< Behind: added above and below the extent
  SkColor4f colour = {0, 0, 0, 1};
};

/** RULES AND SHADING CUT TO WHAT A BLOCK ACTUALLY OCCUPIES.
 *
 *      root.child(kit::rules(composer, "epigraph", sel::all(),
 *                            {.where = BlockRule::Where::Behind,
 *                             .bleed = 4, .colour = tint})
 *                     .absolute().inset(0));
 *
 *  The extent comes from `Composer::units` over `unit::Line`, so a rule is
 *  as wide as the lines it dresses rather than as wide as the box they sit
 *  in — which is the difference between a rule under a heading and a rule
 *  under the column the heading is in. `Behind` fills one box over the
 *  whole run of lines; `Above` and `Below` draw one rule at the run's two
 *  ends.
 *
 *  Describe-time, from the layout the last draw left standing, on the same
 *  terms as everything else here that reads a resolved layout. */
[[nodiscard]] inline Element rules(const Composer& composer,
                                   std::string_view key, const Selector& where,
                                   BlockRule rule) {
  Element overlay = positioned();
  const std::vector<TextUnit> lines = composer.units(key, where, Unit::Line);
  if (lines.empty()) return overlay;
  SkRect extent = lines.front().rect;
  for (const TextUnit& line : lines) extent.join(line.rect);
  extent.inset(rule.inset, 0);
  const std::string base(key);
  switch (rule.where) {
    case BlockRule::Where::Behind:
      overlay.child(box()
                        .key(base + "-shade")
                        .left(extent.left())
                        .top(extent.top() - rule.bleed)
                        .width(extent.width())
                        .height(extent.height() + rule.bleed * 2)
                        .fill(Fill::color(rule.colour)));
      break;
    case BlockRule::Where::Above:
      overlay.child(box()
                        .key(base + "-rule")
                        .left(extent.left())
                        .top(extent.top() - rule.gap - rule.thickness)
                        .width(extent.width())
                        .height(rule.thickness)
                        .fill(Fill::color(rule.colour)));
      break;
    case BlockRule::Where::Below:
      overlay.child(box()
                        .key(base + "-rule")
                        .left(extent.left())
                        .top(extent.bottom() + rule.gap)
                        .width(extent.width())
                        .height(rule.thickness)
                        .fill(Fill::color(rule.colour)));
      break;
  }
  return overlay;
}

}  // namespace sigil::compose::kit
