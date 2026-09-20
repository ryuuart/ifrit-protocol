#pragma once

/** @file
 * @ingroup compose-kit
 *
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
 * component names the document role each line is set in, and the
 * `weave::StyleSheet` where the component lands styles that role.
 */

#include <sigilcompose/core/Element.h>
#include <sigilcompose/core/Factories.h>
#include <sigilcompose/core/Layout.h>
#include <sigilcompose/core/Paint.h>
#include <sigilcompose/core/Utf8.h>
#include <sigilcompose/kit/Document.h>
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

/** The leaf a caption's label defaults to: a document label. */
[[nodiscard]] inline Element captionLabel(const Utf8& text) {
  return document::label(text);
}
/** The leaf a caption's note defaults to: a document caption. */
[[nodiscard]] inline Element captionNote(const Utf8& text) {
  return document::caption(text);
}
/** THE LEAF A MEASURED FIGURE DEFAULTS TO: @p text in the class
 *  `readout` — a caption's reading, a readout's value, a figure column's
 *  cells. One leaf, because it is one line wherever it stands. */
[[nodiscard]] inline Element figure(const Utf8& text) {
  return document::paragraph(text).styleClass("readout");
}

/** THE FIXED SURFACE A SPECIMEN IS SHOWN IN. Its size, ground, padding,
 *  corners and keyline are caller decisions; the kit only applies them
 *  together and clips the result, which is the frame every specimen cell
 *  otherwise restates.
 *
 *      kit::well({.width = 240, .height = 160,
 *                 .ground = Fill::color(kGround), .padding = 12},
 *                box().children({subject()}))
 *
 *  THE SECOND ARGUMENT IS THE SURFACE ITSELF: the size, ground, padding,
 *  corners and keyline are written onto that element, so it BECOMES the
 *  well rather than standing in one. Hand it `custom(key, draw)` when the
 *  drawing should receive the well's resolved size directly; hand it
 *  `box().children({body})` when the well contains a laid-out body. An
 *  open width or height leaves that dimension to the surface alone.
 *
 *  `Well::content` is the other reading — the well that HOLDS what it is
 *  handed, at that element's own measure, ranged inside the plate:
 *
 *      kit::well({.width = 200, .height = 200, .content = Well::Content{}},
 *                picture())      // a 132 px picture, centred on its plate
 */
struct Well {
  Dimension width;
  Dimension height;
  SurfacePaint ground;
  /** Across, px. */
  float padding = 0.0f;
  /** Down, where a plate is set tighter or looser than it is wide; unset
   *  is whatever `padding` is. */
  std::optional<float> paddingY;
  bool clip = true;
  /** Rounds the well. 0 is the square corner a specimen sheet uses. */
  float corners = 0.0f;
  /** ONE HAIRLINE ROUND THE WELL, over its ground — what turns a patch of
   *  ground into a PLATE. Unset draws none, and so does `Fill::none()`,
   *  which is the spelling a ground takes.
   *
   *  IT IS DRAWN INSIDE THE WELL'S OWN BOX. A rule centred on the
   *  boundary would put half its width outside, so a plate and the plate
   *  beside it would no longer be the width they were given — which is
   *  the one thing a fixed surface may not do. */
  std::optional<Fill> keyline;
  float keylineWidth = 1.0f;
  /** WHETHER WHAT THE WELL HOLDS IS PLACED RATHER THAN FLOWED. false
   *  (default) is a box, and its children lay out; true is a `stack`,
   *  whose children each keep the rect they were built with — which is
   *  what a plate holding a drawing rather than a reading is, and the
   *  reason the grounded-rounded-ruled plate could not be one call at
   *  those sites. It says what the EMPTY overload builds; a caller who
   *  hands over a surface has already said which it is. */
  bool placed = false;

  /** HOW WHAT THE WELL HOLDS RANGES INSIDE IT. */
  struct Content {
    Align across = Align::Center;
    Justify down = Justify::Center;
  };
  /** THE WELL THAT HOLDS, rather than the well that IS.
   *
   *  Unset (default), `well(spec, surface)` writes this spec ONTO the
   *  element it is handed: the surface IS the well, which is what a
   *  drawing sized to its plate wants and what makes a material a
   *  well's own ground.
   *
   *  Stated, the well is a surface of its own and the element stands
   *  INSIDE it, keeping its own measure, ranged both ways as this says —
   *  the specimen SMALLER than the plate it is shown on, which is the
   *  common reading of a sheet of pictures. Centred is what it says
   *  where it says nothing else. */
  std::optional<Content> content;
};

/** @p surface, sized, grounded, padded, rounded and ruled as @p spec
 *  says. */
[[nodiscard]] Element well(const Well& spec, Element surface);

/** An empty specimen well, ready for children to be added fluently — a
 *  box, or the `stack` `Well::placed` asks for. */
[[nodiscard]] Element well(const Well& spec);

/** HOW A CELL IS CAPTIONED: where its two lines stand, the air between
 *  them and the body, and the width either wraps at. One value per sheet,
 *  handed to every cell on it, so the sheet has one voice.
 *
 *  NO TYPE IS HERE. The two lines are PARTS: `label` and `note` default to
 *  `captionLabel` and `captionNote`, document label and caption leaves
 *  styled by the sheet where the cell lands, so what they look like is one
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
  /** HOW THE BODY RANGES INSIDE ITS CELL. `Auto` (default) says nothing,
   *  and the body lies as the caller built it; anything else ranges it
   *  both ways, which is what a word, a figure or a picture standing in
   *  the middle of its well asks for.
   *
   *  WITH `body` STATED IT IS THAT WELL'S `content`: the body is HELD in
   *  the plate at its own measure rather than being stretched to it, so
   *  a picture smaller than its well keeps the size it was drawn at. A
   *  body that IS its plate leaves this alone. */
  Align justify = Align::Auto;
  /** THE BODY'S OWN CELL — the well the specimen is shown in. Unset
   *  leaves the body exactly as it arrives, for a caller that built its
   *  own surface; stated, the body IS that surface, so a cell and its
   *  well are one call. */
  std::optional<Well> body;
  /** A FIGURE READ OFF THE BODY, pinned over the body's top-left corner
   *  on a scrim of the cell's own ground — the count, the cost or the
   *  moment a picture that fills its whole well would otherwise have
   *  nowhere to put. Empty writes none. */
  Utf8 reading;
  /** THE THREE LINES, as functions of their text and then of this
   *  caption; a part takes the parameters it names. Empty means the
   *  default. */
  Part<Utf8, Caption> label = captionLabel;
  Part<Utf8, Caption> note = captionNote;
  Part<Utf8, Caption> readingLine = figure;
};

/** ONE CAPTIONED CELL: @p body with @p label and @p note set beside it as
 *  @p caption says.
 *
 *      kit::cell(voice, "blur(14, 14)", "all or nothing",
 *                subject().key("flat").effect(blur))
 *
 *  THE LABEL HAS ROLE `label` and the note has role `caption`,
 *  resolved through the `weave::StyleSheet` where the cell lands — nothing else
 *  is said about their type, so a sheet that registers the two names
 *  clothes every cell on it at once.
 *
 *  An empty label or an empty note is simply absent — the cell has fewer
 *  children and spends no gap on the missing line. The result is an
 *  ordinary column: size it, key it, or key the body where a query needs
 *  the body rather than the cell.
 *
 *  A CELL STATES ITS BODY'S CELL: with `Caption::body` the specimen is
 *  shown in that well, so the cell and the well are one call, and
 *  `Caption::reading` writes a figure over the body's corner on a scrim
 *  of the well's own ground, in the class `readout`. `Caption::justify`
 *  makes that well HOLD the body rather than write itself onto it, so a
 *  picture smaller than its plate stands where the alignment puts it. */
[[nodiscard]] inline Element cell(const Caption& caption, Utf8 label, Utf8 note,
                                  Element body) {
  if (!caption.reading.empty()) {
    // The scrim is a box around the line rather than padding on the line
    // itself, and the reading is pinned in a box the body shares, because
    // a body that is one custom leaf has nowhere to pin it to.
    Element scrim =
        box()
            .absolute()
            .left(Dimension(caption.gap))
            .top(Dimension(caption.gap))
            .padding(Dimension(caption.gap), Dimension(caption.gap * 0.5f))
            .children({caption.readingLine
                           ? caption.readingLine(caption.reading, caption)
                           : figure(caption.reading)});
    // The scrim is the cell's own ground, so the reading stands off
    // whatever the body draws under it.
    if (caption.body && !caption.body->ground.none())
      scrim.fill(caption.body->ground);
    body = box().children({std::move(body), std::move(scrim)});
  }
  const Justify down = caption.justify == Align::Center ? Justify::Center
                       : caption.justify == Align::End  ? Justify::End
                                                        : Justify::Start;
  if (caption.body) {
    Well plate = *caption.body;
    // The caption's own alignment is the plate's: a body ranged inside
    // its well is HELD by it, which is the reading that leaves a picture
    // at the measure it was drawn at.
    if (caption.justify != Align::Auto && !plate.content)
      plate.content = Well::Content{.across = caption.justify, .down = down};
    body = well(plate, std::move(body));
  } else if (caption.justify != Align::Auto) {
    body.alignItems(caption.justify);
    body.justifyContent(down);
  }
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
// The specimen reading

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
  /** THE WIDTH THE SHARES ARE CUT FROM. Unset (Auto) is the parent's,
   *  which is what a grid across a page wants; a grid standing in a
   *  column whose own width comes from its CONTENT has no width to
   *  divide, and its cells would be dealt nothing and drawn over each
   *  other — so a grid inside one says here how wide it is. */
  Dimension measure;
};

[[nodiscard]] Element panelGrid(PanelGrid grid);

// ---------------------------------------------------------------------------
// The sheet

/** The leaf a sheet's title defaults to: a level-one document heading. */
[[nodiscard]] inline Element sheetTitle(const Utf8& text) {
  return document::h1(text);
}
/** The leaf a sheet's subtitle defaults to: a document lead. */
[[nodiscard]] inline Element sheetSubtitle(const Utf8& text) {
  return document::lead(text);
}
/** The leaf a sheet's footer defaults to: a document footer. */
[[nodiscard]] inline Element sheetFooter(const Utf8& text) {
  return document::footer(text);
}

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
 *  ITS THREE LINES HAVE DOCUMENT ROLES `h1`, `lead` and `footer`,
 *  resolved through the `weave::StyleSheet` where the sheet lands, and nothing
 * else is said about their type. Each is a PART — `titleLine`, `subtitleLine`,
 *  `footerLine` — so a page whose title must stand otherwise hands in its
 *  own leaf and everything under the page keeps its registers, because no
 *  sheet moved.
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
  /** THE THREE LINES, as functions of their text and then of this sheet;
   *  a part takes the parameters it names. Empty means the default. The
   *  plain names are the words themselves, so each part carries `Line`. */
  Part<Utf8, Sheet> titleLine = sheetTitle;
  Part<Utf8, Sheet> subtitleLine = sheetSubtitle;
  Part<Utf8, Sheet> footerLine = sheetFooter;
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
      header.children({named(page.titleLine ? page.titleLine(page.title, page)
                                            : sheetTitle(page.title),
                             "title")});
    if (hasSubtitle) {
      Element subtitle =
          named(page.subtitleLine ? page.subtitleLine(page.subtitle, page)
                                  : sheetSubtitle(page.subtitle),
                "subtitle");
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
    Element footer = named(page.footerLine ? page.footerLine(page.footer, page)
                                           : sheetFooter(page.footer),
                           "footer");
    if (ruled)
      root.children({rule("foot-rule")});
    else
      footer.margin(0, page.contentGap, 0, 0);
    root.children({std::move(footer)});
  }
  return root;
}

}  // namespace sigil::compose::kit
