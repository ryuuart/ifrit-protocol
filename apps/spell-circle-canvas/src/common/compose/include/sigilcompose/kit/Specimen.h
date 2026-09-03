#pragma once

/** @file
 * SigilCompose KIT — the furniture of a SPECIMEN SHEET: a captioned
 * cell (a label, a note and the thing they describe), a run of cells
 * along one axis with a hairline between them where the sheet wants
 * one, and the sheet itself — a titled, footed page whose header and
 * footer are ruled off from the content between them.
 *
 * All three are plain composition over the public API and decide no
 * look. Every face, size, colour and distance is the caller's, handed in
 * once as a `Caption` for the cells and a `Sheet` for the page; what is
 * fixed here is only the ARRANGEMENT — which side of the body a note
 * stands on, what a rule is and where a footer lands — because that is
 * the part every sheet spelled out by hand, and spelled differently.
 */

#include <include/core/SkColor.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilweave/style/TextStyle.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace sigil::compose::kit {

// ---------------------------------------------------------------------------
// The captioned cell

/** HOW A CELL IS CAPTIONED: the two type styles its lines are set in, the
 *  air between them and the body, and where they stand. One value per
 *  sheet, handed to every cell on it, so the sheet has one voice. */
struct Caption {
  /** WHERE THE CAPTION'S LINES STAND relative to the body.
   *
   *  `Split` puts the label over the body and the note under it — the
   *  reading of an API sheet, where the label names the call and the
   *  note, read after the picture, says what it did. `Above` stacks both
   *  over the body — the type-specimen reading, where the note qualifies
   *  the label before the eye reaches the setting. `Below` stacks both
   *  under it, for a swatch or a plot whose picture is the point and
   *  whose name is a legend. */
  enum class Where : uint8_t { Split, Above, Below };
  Where where = Where::Split;
  sigil::weave::TextStyle label;
  sigil::weave::TextStyle note;
  /** Between a caption line and the body, px. */
  float gap = 6.0f;
  /** Between the label and the note where the two stand together
   *  (`Above`, `Below`), px. */
  float noteGap = 4.0f;
  /** The note's measure, px. 0 lets the note take the cell's width; a
   *  measure wraps it there, so a long remark under a narrow specimen
   *  does not widen the cell. */
  float noteMeasure = 0.0f;
  /** How the caption lines and the body range across the cell: `Start`
   *  ranges everything left, `Center` stands a caption over the middle of
   *  its body. */
  Align align = Align::Start;
};

/** ONE CAPTIONED CELL: @p body with @p label and @p note set beside it as
 *  @p caption says.
 *
 *      kit::cell(voice, u8"blur(14, 14)", u8"all or nothing",
 *                subject().key("flat").effect(blur))
 *
 *  An empty label or an empty note is simply absent — the cell has fewer
 *  children and spends no gap on the missing line. The result is an
 *  ordinary column: size it, key it, or key the body where a query needs
 *  the body rather than the cell. */
[[nodiscard]] inline Element cell(const Caption& caption, std::u8string label,
                                  std::u8string note, Element body) {
  Element column = box().column().alignItems(caption.align);
  // The space above each part is that part's own margin rather than the
  // column's gap, because a caption's two distances differ and a line that
  // is absent must leave no space behind it.
  int placed = 0;
  const auto place = [&](Element part, float before) {
    if (placed > 0) part.margin(0, before, 0, 0);
    column.child(std::move(part));
    ++placed;
  };
  const bool hasLabel = !label.empty();
  const bool hasNote = !note.empty();
  Element labelLeaf;
  Element noteLeaf;
  if (hasLabel) labelLeaf = text(std::move(label), caption.label);
  if (hasNote) {
    noteLeaf = text(std::move(note), caption.note);
    if (caption.noteMeasure > 0) noteLeaf.width(Dim(caption.noteMeasure));
  }
  switch (caption.where) {
    case Caption::Where::Split:
      if (hasLabel) place(std::move(labelLeaf), 0.0f);
      place(std::move(body), caption.gap);
      if (hasNote) place(std::move(noteLeaf), caption.gap);
      break;
    case Caption::Where::Above:
      if (hasLabel) place(std::move(labelLeaf), 0.0f);
      if (hasNote) place(std::move(noteLeaf), caption.noteGap);
      place(std::move(body), caption.gap);
      break;
    case Caption::Where::Below:
      place(std::move(body), 0.0f);
      if (hasLabel) place(std::move(labelLeaf), caption.gap);
      if (hasNote)
        place(std::move(noteLeaf), hasLabel ? caption.noteGap : caption.gap);
      break;
  }
  return column;
}

// ---------------------------------------------------------------------------
// A run of cells

/** A RUN OF CELLS along one axis: a shelf of specimens across a sheet, or
 *  a column of them down it, each at its own size, with a hairline
 *  between neighbours where the sheet rules them apart.
 *
 *      kit::cells({.cells = {a, b, c}, .gap = 20,
 *                  .divider = Fill::color(hex(0x241c15, 0.2f))})
 *
 *  It places nothing itself and sizes nothing: the run is an ordinary
 *  box in its parent's flow, and a cell keeps the width it was given. A
 *  grid is a column of runs. */
struct Cells {
  /** In order along the axis. */
  std::vector<Element> cells;
  /** false (default) lays the cells out as a ROW; true stacks them. */
  bool column = false;
  /** Between neighbours, px — and between a cell and the divider beside
   *  it, so a ruled run breathes on both sides of the rule. */
  float gap = 20.0f;
  /** Fill::none() (default) means no rules. */
  Fill divider;
  float dividerWidth = 1.0f;
  /** How the cells range across the axis — `Start` tops a row's cells on
   *  one line, `Center` centres them, `End` bottoms them. */
  Align align = Align::Start;
};

[[nodiscard]] inline Element cells(Cells run) {
  Element shelf = box().gap(run.gap).alignItems(run.align);
  if (run.column)
    shelf.column();
  else
    shelf.row();
  const bool ruled = run.divider.kind != Fill::Kind::None;
  bool first = true;
  for (Element& cell : run.cells) {
    if (!first && ruled) {
      Element rule = run.column ? box().height(Dim(run.dividerWidth))
                                : box().width(Dim(run.dividerWidth));
      // The rule spans the run's whole cross extent whatever the cells'
      // own alignment is: a rule that stopped at the tallest cell's top
      // would read as a tick.
      shelf.child(rule.fill(run.divider).alignSelf(Align::Stretch));
    }
    first = false;
    shelf.child(std::move(cell));
  }
  return shelf;
}

// ---------------------------------------------------------------------------
// The sheet

/** THE SHEET: a page with a titled header, a footer line, and the content
 *  between them ruled off from both where the page rules at all.
 *
 *      kit::sheet({.title = u8"THE BLOCK CONTROLS",
 *                  .subtitle = u8"one text leaf per panel",
 *                  .footer = u8"Sketchbook · paragraph_sheet",
 *                  .titleStyle = display, .subtitleStyle = small,
 *                  .footerStyle = small, .marginX = 64, .marginTop = 64,
 *                  .marginBottom = 34, .ground = Fill::color(kPaper),
 *                  .rule = Fill::color(kFaint)},
 *                 kit::cells({.cells = panels, .gap = 40}))
 *          .absolute().inset(0)
 *
 *  **It does not size itself.** The page is a padded column: the caller
 *  gives it the canvas (`absolute().inset(0)`) or a rect, and the content
 *  grows to fill whatever stands between the header and the footer, which
 *  is what puts the footer at the bottom. A title or a footer left empty
 *  is absent, and the rule that would have stood beside it is absent too.
 *
 *  `key`, when set, names the parts — `<key>-title`, `<key>-subtitle`,
 *  `<key>-header`, `<key>-content`, `<key>-footer`, `<key>-head-rule` and
 *  `<key>-foot-rule` — so a query can read the page back. */
struct Sheet {
  std::u8string title;
  std::u8string subtitle;
  std::u8string footer;
  sigil::weave::TextStyle titleStyle;
  sigil::weave::TextStyle subtitleStyle;
  sigil::weave::TextStyle footerStyle;
  /** The page margins, px: the two sides, the top and the bottom. */
  float marginX = 30.0f;
  float marginTop = 16.0f;
  float marginBottom = 14.0f;
  /** Between the title and its subtitle, px. */
  float subtitleGap = 6.0f;
  /** Between the header and the content, and between the content and the
   *  footer, px. A rule stands in the middle of that distance rather than
   *  adding to it, so ruling a page moves nothing on it. */
  float contentGap = 18.0f;
  /** The page's own ground. Fill::none() (default) paints nothing, for a
   *  canvas the host already cleared. */
  Fill ground;
  /** The hairline under the header and over the footer. Fill::none()
   *  (default) rules neither. */
  Fill rule;
  float ruleWidth = 1.0f;
  /** The prefix the parts are keyed under; empty keys nothing. */
  std::string key;
};

[[nodiscard]] inline Element sheet(const Sheet& page, Element content) {
  const auto named = [&](Element part, const char* which) {
    if (!page.key.empty()) part.key(page.key + "-" + which);
    return part;
  };
  Element root = box().column().padding(page.marginX, page.marginTop,
                                        page.marginX, page.marginBottom);
  if (page.ground.kind != Fill::Kind::None) root.fill(page.ground);

  const bool ruled = page.rule.kind != Fill::Kind::None;
  // A rule bisects the content gap: the same distance from the header to
  // the content with or without one.
  const float half = std::max(
      0.0f, (page.contentGap - (ruled ? page.ruleWidth : 0.0f)) * 0.5f);
  const auto rule = [&](const char* which) {
    return named(box()
                     .height(Dim(page.ruleWidth))
                     .alignSelf(Align::Stretch)
                     .fill(page.rule)
                     .margin(0, half, 0, half),
                 which);
  };

  const bool hasTitle = !page.title.empty();
  const bool hasSubtitle = !page.subtitle.empty();
  if (hasTitle || hasSubtitle) {
    Element header = named(box().column(), "header");
    if (hasTitle)
      header.child(named(text(page.title, page.titleStyle), "title"));
    if (hasSubtitle) {
      Element subtitle = named(text(page.subtitle, page.subtitleStyle),
                               "subtitle");
      if (hasTitle) subtitle.margin(0, page.subtitleGap, 0, 0);
      header.child(std::move(subtitle));
    }
    root.child(std::move(header));
    if (ruled)
      root.child(rule("head-rule"));
    else
      content.margin(0, page.contentGap, 0, 0);
  }

  content.grow(1);
  root.child(named(std::move(content), "content"));

  if (!page.footer.empty()) {
    Element footer = named(text(page.footer, page.footerStyle), "footer");
    if (ruled)
      root.child(rule("foot-rule"));
    else
      footer.margin(0, page.contentGap, 0, 0);
    root.child(std::move(footer));
  }
  return root;
}

}  // namespace sigil::compose::kit
