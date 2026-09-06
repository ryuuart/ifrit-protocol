#pragma once

/** @file
 * SigilCompose KIT — the furniture a page of set text carries, as stock
 * values over the seams underneath: readings beside the type (`ruby`,
 * `kenten`), a block's opening words set in a style of their own
 * (`NestedStyle`), a list whose markers hang in the indent (`bullets`),
 * and rules and shading cut to what a block actually occupies (`rules`).
 *
 * None of these is a mechanism. Ruby and kenten are `Annotation` values —
 * a selector, a unit, a reading and a type — and the whole of what
 * distinguishes mono, group and jukugo ruby is which unit is named. A list
 * is a hanging indent with a marker in the hang. A rule is a box at the
 * extent `Composer::units` reports. A block's opening letter set large is
 * not here at all: it is `Element::initialLetter`, because the size that
 * makes a cap span three lines is answerable only where the block's pitch
 * and the face's own metrics are.
 *
 * NOTHING HERE DECIDES A RATIO. A reading's size is its own style's and a
 * marker's inset is stated in pixels: the library carries no fraction of a
 * base's size anywhere, because which fraction is right is a decision and
 * decisions are the caller's.
 */

#include <include/core/SkColor.h>
#include <include/core/SkRect.h>
#include <sigilcompose/core/Composer.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/typography/Annotation.h>
#include <sigilcompose/typography/Selector.h>
#include <sigilcompose/typography/TextUnit.h>
#include <sigilweave/layout/Story.h>
#include <sigilweave/paragraph/RichText.h>
#include <sigilweave/paragraph/Unit.h>
#include <sigilweave/query/Selector.h>
#include <sigilweave/style/Style.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace sigil::compose::kit {

/** FURIGANA: a reading set over the base it reads.
 *
 *      text(passage, body)
 *          .annotate(kit::ruby(weave::sel::text(u8"漢字"), weave::unit::Word,
 *                              {u8"かんじ"}, furigana))
 *
 *  MONO, GROUP AND JUKUGO ARE THE UNIT: `weave::unit::Cluster` gives one
 *  reading per character, `weave::unit::Word` one per word, and a compound
 *  annotated per cluster with the readings its characters take is jukugo.
 *  A base that breaks across a line or a column carries its reading with it,
 *  split in proportion to the base's advance either side.
 *
 *  It RESERVES: the band the reading occupies goes into the base's strut
 *  before the base is broken, so the line pitch — or the column pitch —
 *  opens to hold it and the base is laid out once. */
[[nodiscard]] inline Annotation ruby(sigil::weave::Selector over,
                                     sigil::weave::Unit unit,
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
[[nodiscard]] inline Annotation kenten(sigil::weave::Selector over,
                                       sigil::weave::TextStyle style,
                                       std::u8string mark = u8"\xef\xb9\x85",
                                       float gap = 0) {
  return Annotation{.where = std::move(over),
                    .unit = sigil::weave::Unit::Cluster,
                    .readings = {std::move(mark)},
                    .style = std::move(style),
                    .side = Annotation::Side::Before,
                    .gap = gap,
                    .reserve = false};
}

/** HOW FAR A NESTED STYLE RUNS from the start of a block, and what the
 *  text it covers is set in.
 *
 *  A nested style is the opening of a paragraph set differently from the
 *  rest of it — small caps for the first three words after an initial, a
 *  bold lead-in up to the em dash, an initial phrase in the display face.
 *  What makes it a nested style rather than a hand-cut restyle is that the
 *  author states WHERE IT STOPS in the text's own terms and the text
 *  decides where that falls, so the opening keeps its treatment when the
 *  copy or the measure changes.
 *
 *  `Words` counts the paragraph's own line-break words. `Characters`
 *  counts UTF-16 code units of the text, which is what a character range
 *  addresses — a combining mark counts as its own, so a passage of
 *  decomposed text wants `Words` or a delimiter. `Delimiter` runs from the
 *  start THROUGH the first occurrence of a mark the author names,
 *  inclusive, which is how a lead-in that ends at a colon or a dash is
 *  written without counting anything. */
struct NestedStyle {
  enum class Until { Characters, Words, Delimiter };
  Until until = Until::Words;
  /** `Characters` and `Words`: how many. Zero covers nothing. */
  uint32_t count = 1;
  /** `Delimiter`: the mark the run ends on, and includes. */
  std::u8string delimiter;
  /** What the run it names is set in. */
  sigil::weave::TextStyle style;
};

/** THE SELECTOR A NESTED STYLE MEANS.
 *
 *  There is no nested-style mechanism under this and there does not need
 *  to be: a nested style is a span restyle over a range the selector
 *  vocabulary can already name, so this answers the selector and
 *  `Element::spanStyle` does the work. A delimiter becomes an anchored
 *  non-greedy regular expression — literal-quoted, so a mark that is also
 *  a regex operator (`*`, `.`, `(`) means itself — which is the one of the
 *  three that has no counting selector of its own.
 *
 *  It re-resolves with the text, because a selector does: an edit that
 *  adds a word before the delimiter extends the run, and one that removes
 *  the delimiter leaves the run covering nothing rather than covering the
 *  paragraph. */
[[nodiscard]] inline sigil::weave::Selector nestedRun(
    const NestedStyle& nested) {
  switch (nested.until) {
    case NestedStyle::Until::Characters:
      return sigil::weave::sel::range({0, nested.count});
    case NestedStyle::Until::Words:
      return sigil::weave::sel::words(0, nested.count);
    case NestedStyle::Until::Delimiter:
      break;
  }
  if (nested.delimiter.empty()) return sigil::weave::sel::words(0, 0);
  return sigil::weave::sel::regex(u8"\\A[\\s\\S]*?\\Q" + nested.delimiter +
                                  u8"\\E");
}

/** A LIST WHOSE MARKERS HANG IN THE INDENT.
 *
 *  One text leaf per item, indented by the hang on EVERY line, with the
 *  marker set in the room that indent opens. A number is the caller's to
 *  format, which is why `markers` is a list of strings rather than a
 *  numbering scheme: the schemes people want (roman, lettered, restarting,
 *  hierarchical) are data, and this is the shape they are drawn in.
 *
 *  `hang` is the indent in px, and the marker is set at the block's own
 *  start with the item's text beginning one hang in. THE FIRST LINE IS
 *  INDENTED LIKE THE REST: the marker is placed beside the text rather
 *  than set in its run, so a first line pulled back out of the indent —
 *  the shape a marker typed INTO the text wants — would start where the
 *  marker already stands and print through it. */
[[nodiscard]] inline Element bullets(std::span<const std::u8string> items,
                                     std::span<const std::u8string> markers,
                                     sigil::weave::TextStyle style, float hang,
                                     float measure, float gap = 4.0f) {
  Element list = box().column().gap(gap);
  sigil::weave::ParagraphStyle hanging;
  hanging.indent.start = hang;
  for (size_t index = 0; index < items.size(); ++index) {
    const std::u8string& marker =
        markers.empty() ? items[index]
                        : markers[std::min(index, markers.size() - 1)];
    list.child(
        box()
            .child(text(items[index], style)
                       .width(Dim(measure))
                       .paragraph(hanging))
            .child(
                text(marker, style).absolute().left(Dim(0.0f)).top(Dim(0.0f))));
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
 *  Each column takes an equal share of `width` after the gutters.
 *
 *  @p ellipsis ENDS THE CHAIN. The last column threads nowhere, so what
 *  it cannot hold has nowhere to go and draws past its box unless
 *  something stops it; the marker lands on that column's last line and
 *  says the story goes on. The columns before it overflow BY DESIGN and
 *  take no marker whatever is passed here — a mark at every cut would
 *  read as three separate texts rather than one story threaded through
 *  three frames. Empty (the default) is the run-on, for a caller who
 *  clips the chain or knows the story fits. */
[[nodiscard]] inline Element columns(sigil::weave::Story story, int count,
                                     float gutter, float width, float height,
                                     std::string keyPrefix = "column",
                                     std::u8string ellipsis = {}) {
  Element row = box().row().gap(gutter);
  if (count < 1) return row;
  const float measure = (width - gutter * static_cast<float>(count - 1)) /
                        static_cast<float>(count);
  for (int index = 0; index < count; ++index) {
    Element column = frame(story)
                         .key(keyPrefix + std::to_string(index))
                         .width(Dim(measure))
                         .height(Dim(height));
    if (index + 1 < count)
      column.thread(keyPrefix + std::to_string(index + 1));
    else if (!ellipsis.empty())
      column.ellipsis(ellipsis);
    row.child(std::move(column));
  }
  return row;
}

/** SOMETHING THAT STRADDLES A RUN OF COLUMNS: a masthead across the
 *  three columns under a headline, a plate across the middle of a page.
 *
 *  WHERE IT GOES IS A SELECTOR and never a y coordinate: the chain breaks
 *  after the unit `after` names, so the copy above it sets in columns, the
 *  spanner runs the full measure, and the copy below resumes in columns —
 *  all out of ONE story, because the frames above and below are links of
 *  one chain and the second run picks up the word the first ran out on. */
struct Spanner {
  sigil::weave::Selector after;
  Element what;
};

/** N COLUMNS OF ONE STORY, WITH THE THINGS THAT SPAN THEM.
 *
 *      root.child(kit::columns({.story = article, .count = 3,
 *                               .gutter = 28, .width = 760, .height = 420,
 *                               .spanners = {{weave::sel::line(11), plate()}},
 *                               .composer = &composer}));
 *
 *  Each column takes an equal share of `width` after the gutters, and
 *  `height` is the DEEPEST a row of columns may be rather than the depth
 *  it will take: every row above a spanner is BALANCED, filled to the
 *  shallowest depth that still carries the copy down to that spanner, so
 *  its columns come out the same length and the spanner sits under the
 *  copy instead of under the tallest column. The last row keeps the full
 *  depth, because what follows it is the rest of the story.
 *
 *  WHERE EACH SPANNER'S SELECTOR LANDED is read back from the layout the
 *  last draw left standing, which is why the composer is passed. The first
 *  draw has no layout to read, so the first row holds what it can and the
 *  spanners settle on the draw after — the same terms as everything else
 *  here that reads a resolved layout.
 *
 *  @p ellipsis ENDS THE CHAIN, on its last column, exactly as the
 *  positional spelling below says. */
struct ColumnSet {
  sigil::weave::Story story;
  int count = 2;
  float gutter = 24.0f;
  float width = 0.0f;
  float height = 0.0f;
  std::vector<Spanner> spanners;
  const Composer* composer = nullptr;
  std::string keyPrefix = "column";
  std::u8string ellipsis;
};

[[nodiscard]] inline Element columns(ColumnSet set) {
  Element stack = box().column();
  if (set.count < 1) return stack;
  const float measure =
      (set.width - set.gutter * static_cast<float>(set.count - 1)) /
      static_cast<float>(set.count);
  const int rows = static_cast<int>(set.spanners.size()) + 1;
  const auto keyAt = [&](int index) {
    return set.keyPrefix + std::to_string(index);
  };
  // The story line each spanner breaks the chain after, read off the
  // placement the last draw left. `~0u` is "not known yet", which asks the
  // row to hold all it can — what the first draw does.
  std::vector<uint32_t> breakLine(set.spanners.size(), ~0u);
  for (size_t i = 0; i < set.spanners.size() && set.composer; ++i) {
    const std::vector<TextUnit> units = set.composer->units(
        keyAt(0), set.spanners[i].after, sigil::weave::Unit::Line);
    if (!units.empty())
      breakLine[i] = static_cast<uint32_t>(units.back().lineIndex);
  }
  int index = 0;
  for (int r = 0; r < rows; ++r) {
    Element row = box().row().gap(set.gutter);
    for (int i = 0; i < set.count; ++i, ++index) {
      Element column = frame(set.story)
                           .key(keyAt(index))
                           .width(Dim(measure))
                           .height(Dim(set.height));
      // Every row but the last opens a balanced run that must reach the
      // line its spanner breaks after; the last row is the remainder and
      // keeps the depth it was given.
      if (i == 0 && r + 1 < rows) column.balanceChain(breakLine[(size_t)r]);
      if (index + 1 < rows * set.count)
        column.thread(keyAt(index + 1));
      else if (!set.ellipsis.empty())
        column.ellipsis(set.ellipsis);
      row.child(std::move(column));
    }
    stack.child(std::move(row));
    if (r < static_cast<int>(set.spanners.size()))
      stack.child(std::move(set.spanners[(size_t)r].what));
  }
  return stack;
}

/** WHERE A RULE OR A SHADE STANDS relative to the block it dresses. */
struct BlockRule {
  enum class Where { Above, Below, Behind };
  Where where = Where::Above;
  float thickness = 1.0f;  ///< Above/Below: the rule's own weight
  float gap = 4.0f;        ///< Above/Below: clearance from the type
  float inset = 0.0f;      ///< taken off both ends of the extent
  float bleed = 0.0f;      ///< Behind: added above and below the extent
  SkColor4f colour = {0, 0, 0, 1};
};

/** RULES AND SHADING CUT TO WHAT A BLOCK ACTUALLY OCCUPIES.
 *
 *      root.child(kit::rules(composer, "epigraph", sel::all(),
 *                            {.where = BlockRule::Where::Behind,
 *                             .bleed = 4, .colour = tint})
 *                     .absolute().inset(0));
 *
 *  The extent comes from `Composer::units` over `weave::unit::Line`, so a
 *  rule is as wide as the lines it dresses rather than as wide as the box
 *  they sit in — which is the difference between a rule under a heading and
 *  a rule under the column the heading is in. `Behind` fills one box over
 *  the whole run of lines; `Above` and `Below` draw one rule at the run's
 *  two ends.
 *
 *  Describe-time, from the layout the last draw left standing, on the same
 *  terms as everything else here that reads a resolved layout. */
[[nodiscard]] inline Element rules(const Composer& composer,
                                   std::string_view key,
                                   const sigil::weave::Selector& where,
                                   BlockRule rule) {
  Element overlay = positioned();
  const std::vector<TextUnit> lines =
      composer.units(key, where, sigil::weave::Unit::Line);
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
