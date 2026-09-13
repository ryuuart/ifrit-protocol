#pragma once

/** @file
 * SigilCompose KIT — the furniture of a SPECIMEN SHEET: a captioned
 * cell (a label, a note and the thing they describe), the fixed well a
 * specimen is shown in, a run of cells along one axis with a hairline
 * between them where the sheet wants one, and the sheet itself — a titled,
 * footed page whose header and footer are ruled off from the content
 * between them.
 *
 * Every component is plain composition over the public API and decides no
 * look. Its props carry the CONTENT and the ARRANGEMENT — the words, the
 * measures, which side of the body a note stands on, what a rule is and
 * where a footer lands. Every face, size and colour is the CASCADE's: a
 * component names the class each line of it is set in, and the
 * `weave::StyleSheet` in scope where the component is called says what
 * that class is.
 */

#include <include/core/SkColor.h>
#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Utf8.h>
#include <sigilcompose/kit/Part.h>
#include <sigilweave/layout/StyleSheet.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace sigil::compose::kit {

// ---------------------------------------------------------------------------
// The captioned cell

/** The leaf a caption's label defaults to: @p text in the class
 *  `captionLabel`, so the sheet in force sets it. */
[[nodiscard]] inline Element captionLabel(const Utf8& text) {
  return compose::text(text.bytes()).styleClass("captionLabel");
}
/** The leaf a caption's note defaults to: @p text in the class
 *  `captionNote`. */
[[nodiscard]] inline Element captionNote(const Utf8& text) {
  return compose::text(text.bytes()).styleClass("captionNote");
}

/** HOW A CELL IS CAPTIONED: where its two lines stand, the air between
 *  them and the body, and the width either wraps at. One value per sheet,
 *  handed to every cell on it, so the sheet has one voice.
 *
 *  NO TYPE IS HERE. The two lines are PARTS: `label` and `note` default to
 *  `captionLabel` and `captionNote`, leaves in the class of that name of
 *  the sheet in force where the cell lands, so what they look like is one
 *  rule each in a sheet. A cell whose call must stand otherwise hands
 *  `label` its own leaf — the register with a font over it, or a leaf of
 *  its own — and the cells under it keep the register, because no sheet
 *  moved. */
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
  /** Between a caption line and the body, px. */
  float gap = 6.0f;
  /** Between the label and the note where the two stand together
   *  (`Above`, `Below`), px. */
  float noteGap = 4.0f;
  /** The label's measure, px. 0 lets the label take the cell's width; a
   *  measure wraps it there, so a long call written out over a narrow
   *  specimen does not widen the cell. A label is usually short enough
   *  that this never comes up, and then it is exactly the width that
   *  makes a run of cells stop lining up. */
  float labelMeasure = 0.0f;
  /** The note's measure, px. 0 lets the note take the cell's width; a
   *  measure wraps it there, so a long remark under a narrow specimen
   *  does not widen the cell. */
  float noteMeasure = 0.0f;
  /** How the caption lines and the body range across the cell: `Start`
   *  ranges everything left, `Center` stands a caption over the middle of
   *  its body. */
  Align align = Align::Start;
  /** THE TWO LINES, as functions of their text and then of this caption;
   *  a part takes the parameters it names. Empty means the default. */
  Part<Utf8, Caption> label = captionLabel;
  Part<Utf8, Caption> note = captionNote;
};

/** ONE CAPTIONED CELL: @p body with @p label and @p note set beside it as
 *  @p caption says.
 *
 *      kit::cell(voice, "blur(14, 14)", "all or nothing",
 *                subject().key("flat").effect(blur))
 *
 *  THE LABEL IS SET IN THE CLASS `captionLabel` and the note in
 *  `captionNote`, of the `weave::StyleSheet` in scope here — nothing else
 *  is said about their type, so a sheet that registers the two names
 *  clothes every cell on it at once.
 *
 *  An empty label or an empty note is simply absent — the cell has fewer
 *  children and spends no gap on the missing line. The result is an
 *  ordinary column: size it, key it, or key the body where a query needs
 *  the body rather than the cell. */
[[nodiscard]] inline Element cell(const Caption& caption, Utf8 label, Utf8 note,
                                  Element body) {
  Element column = box().column().alignItems(caption.align);
  // The space above each part is that part's own margin rather than the
  // column's gap, because a caption's two distances differ and a line that
  // is absent must leave no space behind it.
  int placed = 0;
  const auto place = [&](Element part, float before) {
    if (placed > 0) part.margin(0, before, 0, 0);
    column.children({std::move(part)});
    ++placed;
  };
  const bool hasLabel = !label.empty();
  const bool hasNote = !note.empty();
  Element labelLeaf;
  Element noteLeaf;
  if (hasLabel) {
    labelLeaf =
        caption.label ? caption.label(label, caption) : captionLabel(label);
    if (caption.labelMeasure > 0)
      labelLeaf.width(Dimension(caption.labelMeasure));
  }
  if (hasNote) {
    noteLeaf = caption.note ? caption.note(note, caption) : captionNote(note);
    if (caption.noteMeasure > 0) noteLeaf.width(Dimension(caption.noteMeasure));
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
// The specimen well

/** THE FIXED SURFACE A SPECIMEN IS SHOWN IN. Its size, ground and padding
 *  are caller decisions; the kit only applies them together and clips the
 *  result, which is the frame every specimen cell otherwise restates.
 *
 *      kit::well({.width = 240, .height = 160,
 *                 .ground = Fill::color(kGround), .padding = 12},
 *                box().children({subject()}))
 *
 *  The second argument is the surface itself, not a child wrapped in a new
 *  box. Hand it `custom(key, draw)` when the drawing should receive the
 *  well's resolved size directly; hand it `box().children({body})` when the
 * well contains a laid-out body. An open width or height leaves that dimension
 *  already carried by the surface alone. */
struct Well {
  Dimension width;
  Dimension height;
  SurfacePaint ground;
  float padding = 0.0f;
  bool clip = true;
};

[[nodiscard]] inline Element well(const Well& spec, Element surface) {
  if (spec.width.unit != Dimension::Unit::Auto) surface.width(spec.width);
  if (spec.height.unit != Dimension::Unit::Auto) surface.height(spec.height);
  if (!spec.ground.none()) surface.fill(spec.ground);
  if (spec.padding != 0.0f) surface.padding(spec.padding);
  if (spec.clip) surface.clip();
  return surface;
}

/** An empty specimen well, ready for children to be added fluently. */
[[nodiscard]] inline Element well(const Well& spec) {
  return well(spec, box());
}

/** A SPECIMEN READING, filled in and sized to its result rather than to a
 *  guessed stack buffer. printf's grammar, because the readings it
 *  replaces already carry printf format strings — which is also why it is
 *  not spelled `format`: `std::format` is a different grammar, and one
 *  name for two of them is a trap every time a reading is edited. */
[[nodiscard]] inline std::string formatted(const char* text) {
  return text != nullptr ? std::string(text) : std::string{};
}

/** Only trivially-copyable arguments: the pack is passed through the C
 *  ellipsis of `snprintf`, and a type with a non-trivial copy has
 *  undefined behaviour there — `std::string` is the one an author reaches
 *  for first, and `.c_str()` is what the pattern wants. The format
 *  attribute puts the compiler's own printf check on the call, so a `%d`
 *  fed a float is a diagnostic here rather than a wrong reading on a
 *  plate. */
#if defined(__clang__)
// Clang honours the attribute on a template pack; GCC accepts it only on a
// function with a real ellipsis and says so, which is noise in every
// translation unit that includes this header.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgcc-compat"
#endif
template <typename... Args>
  requires(sizeof...(Args) > 0 && (std::is_trivially_copyable_v<Args> && ...))
[[nodiscard]]
__attribute__((format(printf, 1, 2))) std::string formatted(const char* pattern,
                                                            Args... args) {
  if (pattern == nullptr) return {};
  const int length = std::snprintf(nullptr, 0, pattern, args...);
  if (length <= 0) return {};
  std::string result((size_t)length + 1, '\0');
  std::snprintf(result.data(), result.size(), pattern, args...);
  result.resize((size_t)length);
  return result;
}
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

// ---------------------------------------------------------------------------
// A run of cells

/** A RUN OF CELLS along one axis: a shelf of specimens across a sheet, or
 *  a column of them down it, each at its own size, with a hairline
 *  between neighbours where the sheet rules them apart.
 *
 *      kit::cells({.cells = {a, b, c}, .gap = 20,
 *                  .divider = Fill::color(hexColor(0x241c15, 0.2f))})
 *
 *  It places nothing itself and sizes nothing: the run is an ordinary
 *  box in its parent's flow, and a cell keeps the width it was given. A
 *  wrapped panel grid shares its width across equal columns. */
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

/** A flex row or column, with dividers spanning its cross axis. */
[[nodiscard]] Element cells(Cells run);

/** Equal-width panels with optional dividers. A partial last row keeps
 *  the same column widths as the full rows above it. */
struct PanelGrid {
  std::vector<Element> cells;
  /** Positive counts wrap; zero or negative puts every panel on one row. */
  int columns = 3;
  float gap = 20.0f;
  /** Unset uses the horizontal gap. */
  std::optional<float> rowGap;
  Fill divider;
  float dividerWidth = 1.0f;
  /** Cross-axis alignment for a single row. Wrapped rows stretch panels
   *  to the height of the tallest panel in that row. */
  Align align = Align::Stretch;
};

[[nodiscard]] Element panelGrid(PanelGrid grid);

// ---------------------------------------------------------------------------
// The sheet

/** THE SHEET: a page with a titled header, a footer line, and the content
 *  between them ruled off from both where the page rules at all.
 *
 *      kit::sheet({.title = "THE BLOCK CONTROLS",
 *                  .subtitle = "one text leaf per panel",
 *                  .footer = "Sketchbook · paragraph_sheet",
 *                  .marginX = 64, .marginTop = 64,
 *                  .marginBottom = 34, .ground = Fill::color(kPaper),
 *                  .rule = Fill::color(kFaint)},
 *                 kit::cells({.cells = panels, .gap = 40}))
 *          .absolute().inset(0)
 *
 *  ITS THREE LINES ARE SET IN THE CLASSES `title`, `subtitle` and
 *  `footer`, of the `weave::StyleSheet` in scope here, and nothing else is
 *  said about their type.
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
  Utf8 title;
  Utf8 subtitle;
  Utf8 footer;
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
  SurfacePaint ground;
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
  if (!page.ground.none()) root.fill(page.ground);

  const bool ruled = page.rule.kind != Fill::Kind::None;
  // A rule bisects the content gap: the same distance from the header to
  // the content with or without one.
  const float half = std::max(
      0.0f, (page.contentGap - (ruled ? page.ruleWidth : 0.0f)) * 0.5f);
  const auto rule = [&](const char* which) {
    return named(box()
                     .height(Dimension(page.ruleWidth))
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
      header.children(
          {named(text(page.title.bytes()).styleClass("title"), "title")});
    if (hasSubtitle) {
      Element subtitle =
          named(text(page.subtitle.bytes()).styleClass("subtitle"), "subtitle");
      if (hasTitle) subtitle.margin(0, page.subtitleGap, 0, 0);
      header.children({std::move(subtitle)});
    }
    root.children({std::move(header)});
    if (ruled)
      root.children({rule("head-rule")});
    else
      content.margin(0, page.contentGap, 0, 0);
  }

  content.grow(1);
  root.children({named(std::move(content), "content")});

  if (!page.footer.empty()) {
    Element footer =
        named(text(page.footer.bytes()).styleClass("footer"), "footer");
    if (ruled)
      root.children({rule("foot-rule")});
    else
      footer.margin(0, page.contentGap, 0, 0);
    root.children({std::move(footer)});
  }
  return root;
}

}  // namespace sigil::compose::kit
