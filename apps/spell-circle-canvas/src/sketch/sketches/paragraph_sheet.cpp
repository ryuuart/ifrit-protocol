/** @file
 * paragraph_sheet — one story, several block styles: the leading kinds, the
 * one spacing rule, the four indents, a baseline grid two blocks share, a
 * justification that spends past its word gaps, tab stops that align their
 * cells, and the same controls turned a quarter turn into columns.
 */

// A SPECIMEN SHEET FOR THE BLOCK CONTROLS. Everything here is one text
// leaf per panel and a list of ParagraphStyles beside it — there is no
// second text engine for headings, no manual line placement, and nothing
// on the page is positioned by a coordinate that a style could have
// decided.
//
// The six panels, and the one thing each is for:
//
//   · LEADING — the same passage set four ways: the face's own, a
//     multiple of it, an absolute pitch, and a grid. The rules drawn
//     across the grid panel are the grid, so a baseline that misses one
//     is visible rather than arguable.
//   · SPACING — three blocks whose air is the LARGER of after and
//     before, including before the first, which is the whole rule.
//   · INDENTS — first-line, hanging, both ends, and a last line pulled
//     in; four blocks, four IndentOptions.
//   · JUSTIFIED — the same measure set three ways: word gaps alone, gaps
//     then letter spacing, gaps then letter spacing then a glyph scale.
//     The third is deliberately over-asked so the scaling shows.
//   · TABS — a table of figures on four stops: start, centre, end, and a
//     decimal point, the last two with leaders.
//   · COLUMNS — the same block controls in vertical-RL, where the pitch
//     is the column's width and the indents run down it.
//
// EDIT THESE FIRST
//   kGrid       — the rhythm the grid panel lands on, and the rules drawn
//                 across it.
//   kMeasure    — the measure every horizontal panel is set to; narrow it
//                 and the justification panel earns its letter spacing.
//   kInk/kPaper — the sheet's two inks.

// TAGS: Typography/Paragraph

#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilmaterial/color/Color.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/kit/Hyphenation.h>
#include <sigilweave/layout/StyleSheet.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace material = sigil::material;
namespace sketch = sigil::sketch;

using namespace sigil::compose;
namespace weave = sigil::weave;

namespace {

constexpr SkSize kSceneSize{1280, 1060};

namespace sheet {

constexpr float kW = kSceneSize.fWidth;
constexpr float kH = kSceneSize.fHeight;
constexpr float kMargin = 64;
constexpr float kMeasure = 330;
constexpr float kGrid = 21;

const material::Color kPaper{0.965f, 0.957f, 0.937f, 1};
const material::Color kInk{0.114f, 0.106f, 0.098f, 1};
const material::Color kFaint{0.114f, 0.106f, 0.098f, 0.30f};
const material::Color kRule{0.78f, 0.30f, 0.20f, 0.28f};
const material::Color kMark{0.78f, 0.30f, 0.20f, 1};

/// The one hyphenator on the sheet: the justified panel asks for it, and
/// every layout it reaches keeps a share of it.
std::shared_ptr<const sigil::weave::Hyphenator> hyphenator() {
  static const std::shared_ptr<const sigil::weave::Hyphenator> table =
      std::make_shared<const sigil::weave::kit::PatternHyphenator>(
          "en", sigil::weave::kit::englishHyphenationPatterns());
  return table;
}

sk_sp<SkTypeface> serif() {
  return weave::ports::face(
      {"Iowan Old Style", "Palatino", "Georgia", "Times New Roman"});
}
sk_sp<SkTypeface> grotesque() {
  return weave::ports::face({"Helvetica Neue", "Inter", "Helvetica", "Arial"});
}
sk_sp<SkTypeface> mono() {
  return weave::ports::face({"SF Mono", "Menlo", "Courier New"});
}

weave::Type label(float size = 9.0f, float track = 1.6f,
                  material::Color colour = kFaint) {
  return {.face = grotesque(), .size = size, .color = material::skia::toSkColor(colour), .track = track};
}

/// THIS SHEET'S LOOK: paper and its ink, set in the grotesque, with the
/// page's own generous margin.
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look;
  look.palette = {.ground = kPaper, .ink = kInk, .ash = kFaint, .rule = kFaint};
  look.type.sans = grotesque();
  look.type.title = {.size = 11, .track = 4.0f};
  look.type.subtitle = {.size = 10, .track = 0.4f};
  look.type.footer = {.size = 9.5f, .track = 0.3f};
  look.spacing.marginX = kMargin;
  look.spacing.marginTop = kMargin;
  look.spacing.marginBottom = kMargin * 0.5f;
  return look;
}

/// THE SHEET'S TWO CLASSES beside the theme's registers: `body`, the story
/// in the serif under an English tag, and `figures`, the table in the mono.
/// Both are set in the page's ink and, under a page whose running text is
/// tracked, at no tracking; a leaf says only the size it differs by.
weave::StyleSheet classes() {
  weave::StyleSheet sheet = sheetTheme().styleSheet();
  sheet.set(
      "body",
      weave::Type{
          .face = serif(), .size = 13.5f, .track = 0.0f, .language = "en-US"});
  sheet.set("figures",
            weave::Type{.face = mono(), .size = 12.0f, .track = 0.0f});
  return sheet;
}

/// THE PANEL'S VOICE: the control's name, what it decides under it, and
/// the specimen under both.
kit::Caption panelVoice() {
  return {.where = kit::Caption::Where::Above,
          .gap = 13,
          .noteGap = 9,
          .noteMeasure = kMeasure};
}

/// THE SPECIMEN'S VOICE, inside a panel: the call that set it, and the
/// setting under the call.
kit::Caption callVoice(float measure) {
  return {
      .where = kit::Caption::Where::Above, .gap = 5, .noteMeasure = measure};
}

/// A PANEL'S TWO CAPTION CLASSES, over the sheet's own: the control's name
/// in the mark colour, what it decides a size under it.
weave::StyleSheet panelClasses() {
  weave::StyleSheet sheet = classes();
  sheet.set("label", label(9.5f, 2.0f, kMark));
  sheet.set("caption", label(9.0f, 0.4f));
  return sheet;
}

/// A SPECIMEN'S TWO, one step under a panel's: a call and its setting read
/// as one line, so both are the same size.
weave::StyleSheet callClasses() {
  weave::StyleSheet sheet = classes();
  sheet.set("label", label(8.5f, 1.2f));
  sheet.set("caption", label(8.5f, 1.2f));
  return sheet;
}

/// A panel: a name, what the control decides, and the specimen under it.
Element panel(const char* name, const char* note, Element specimen) {
  return kit::cell(panelVoice(), name, note, std::move(specimen))
      .styleSheet(panelClasses());
}

constexpr const char8_t* kFourWays =
    u8"A block states its own pitch, and the extra a leading opens goes "
    u8"above the line, where leading has always gone.";

/// The specimen the leading panel repeats, once per leading kind.
Element leadingSpecimen(const char* caption, weave::Leading leading) {
  weave::ParagraphStyle style;
  style.leading = leading;
  return kit::cell(callVoice(kMeasure * 0.48f), caption, "",
                   document::paragraph(kFourWays)
                       .styleClass("body")
                       .font({.size = 11.5f})
                       .width(kMeasure * 0.48f)
                       .paragraphStyles({style}))
      .styleSheet(callClasses())
      .width(kMeasure * 0.48f);
}

}  // namespace sheet

namespace s = sheet;

struct ParagraphSheet {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(s::sheetTheme());
    sketch::kit::stage(ctx, {.size = kSceneSize, .captureAt = 0.4});
    ctx.composer.render(describe());
  }

  // ── The panels ─────────────────────────────────────────────────────────

  Element leadingPanel() {
    // The grid specimen is captioned as the three beside it are, so it
    // states the same specimen voice; the grid behind it is drawn, so a
    // baseline off the rhythm is a thing to see rather than to argue about.
    Element gridCell =
        kit::cell(
            s::callVoice(s::kMeasure * 0.48f), "Leading::grid(21)", "",
            box()
                .width(s::kMeasure * 0.48f)
                .height(s::kGrid * 4)
                .children(
                    {gridRules(),
                     document::paragraph(s::kFourWays)
                         .styleClass("body")
                         .font({.size = 11.5f})
                         .inset(0)
                         .width(s::kMeasure * 0.48f)
                         .paragraphStyles({weave::ParagraphStyle{
                             .leading = weave::Leading::grid(s::kGrid)}})}))
            .styleSheet(s::callClasses())
            .width(s::kMeasure * 0.48f);
    return s::panel(
        "LEADING",
        "face · multiple · absolute · grid. The rules "
        "under the fourth are the grid it lands on.",
        kit::panelGrid(
            {.cells = {s::leadingSpecimen("Leading::face()",
                                          weave::Leading::face()),
                       s::leadingSpecimen("Leading::multiple(1.7)",
                                          weave::Leading::multiple(1.7f)),
                       s::leadingSpecimen("Leading::absolute(22)",
                                          weave::Leading::absolute(22)),
                       std::move(gridCell)},
             .columns = 2,
             .gap = 18,
             .rowGap = 14,
             .align = Align::Start,
             // The panel this grid stands in sizes itself from its
             // CONTENT, so the shares are cut from a width stated here.
             .measure = s::kMeasure}));
  }

  /// Four rules one grid step apart, behind the grid specimen: the grid
  /// itself, so the panel's claim is checkable.
  static Element gridRules() {
    return kit::ladder(
        {.count = 4, .pitch = s::kGrid, .fill = Fill::color(s::kRule)});
  }

  Element spacingPanel() {
    // 14 is not suppressed at the head of the flow; then 26 wins over the
    // 10 under it, and 24 over the 6 under that — the larger, not the sum.
    const weave::ParagraphStyle first{.spaceBefore = 14, .spaceAfter = 26};
    const weave::ParagraphStyle second{.spaceBefore = 10, .spaceAfter = 6};
    const weave::ParagraphStyle third{.spaceBefore = 24};

    return s::panel(
        "SPACING",
        "the gap between two blocks is the LARGER of the first's "
        "spaceAfter and the second's spaceBefore — 26 then 24, "
        "never 36 or 30.",
        document::paragraph(
            u8"after 26, before 10 — the gap under this block "
            u8"is twenty-six.\n"
            u8"after 6, before 24 — and the gap under THIS one "
            u8"is twenty-four.\n"
            u8"The block above claimed six and the one below claimed "
            u8"twenty-four, so twenty-four stands.")
            .styleClass("body")
            .width(s::kMeasure)
            .paragraphStyles({first, second, third}));
  }

  Element indentPanel() {
    const weave::ParagraphStyle firstLine{.spaceAfter = 10,
                                          .indent = {.firstLine = 22}};
    const weave::ParagraphStyle hanging{
        .spaceAfter = 10, .indent = {.start = 22, .firstLine = -22}};
    const weave::ParagraphStyle bothEnds{.spaceAfter = 10,
                                         .indent = {.start = 20, .end = 20}};
    const weave::ParagraphStyle lastLine{.indent = {.lastLine = 40}};

    return s::panel(
        "INDENTS",
        "start and end on every line, firstLine and lastLine added to "
        "start on those two — all of it arithmetic on the "
        "intervals the geometry handed back.",
        document::paragraph(
            u8"A first-line indent moves the opening of the block and "
            u8"nothing else, which is the oldest way to mark a "
            u8"paragraph.\n"
            u8"A hanging indent is the same field negative against a "
            u8"start indent, so the first line comes out to the margin "
            u8"and the rest stay in.\n"
            u8"Indenting both ends narrows the measure without moving "
            u8"the block, which is how a quotation stands apart from "
            u8"the text around it.\n"
            u8"A last-line indent pulls the closing line in, and it is "
            u8"the fit that decides which line that is.")
            .styleClass("body")
            .width(s::kMeasure)
            .paragraphStyles({firstLine, hanging, bothEnds, lastLine}));
  }

  Element justifiedPanel() {
    const char8_t* passage =
        u8"Justification spends in three passes, each on what the one "
        u8"before it could not: the word gaps first, then the space "
        u8"between the letters, then a horizontal scale on the letters "
        u8"themselves.";

    weave::JustificationOptions gapsOnly;
    weave::JustificationOptions withLetters = gapsOnly;
    withLetters.letterSpacingMaximum = 0.06f;
    withLetters.letterSpacingMinimum = -0.02f;
    weave::JustificationOptions withScale = withLetters;
    withScale.glyphScaleMinimum = 0.96f;
    withScale.glyphScaleMaximum = 1.04f;

    const auto column = [&](const char* caption,
                            const weave::JustificationOptions& spec) {
      return kit::cell(s::callVoice(s::kMeasure * 0.31f), caption, "",
                       document::paragraph(passage)
                           .styleClass("body")
                           .font({.size = 11.0f})
                           .width(s::kMeasure * 0.31f)
                           .block({.alignment = weave::TextAlignment::kJustify,
                                   .justification = spec,
                                   .hyphenation =
                                       weave::HyphenationOptions{
                                           .patterns = s::hyphenator()},
                                   .lineBreak =
                                       weave::LineBreakStrategy::kKnuthPlass}))
          .styleSheet(s::callClasses())
          .width(s::kMeasure * 0.31f);
    };

    return s::panel(
        "JUSTIFIED",
        "word gaps, then letter spacing, then a glyph scale — "
        "each bounded by its own two limits, and a pass at its default "
        "contributes nothing.",
        box().row().gap(14).children({column("gaps alone", gapsOnly),
                                      column("+ letter spacing", withLetters),
                                      column("+ glyph scale", withScale)}));
  }

  Element tabPanel() {
    weave::TabStopOptions stops;
    stops.stops = {
        weave::TabStop{130, weave::TabStop::Align::kStart},
        weave::TabStop{240, weave::TabStop::Align::kCenter},
        weave::TabStop{360, weave::TabStop::Align::kCharacter, u'.'},
        weave::TabStop{470, weave::TabStop::Align::kEnd},
    };
    stops.stops[3].leader = u" .";

    return s::panel(
        "TABS",
        "one stop each: start, centre, a decimal point, and an end with a "
        "leader set across the gap it opened.",
        text(u8"folio\tquire\tcatch\t14.5\tI\n"
             u8"leaf\tgathering\tsignature\t9.25\tII\n"
             u8"recto\tsheet\tvolume\t128.75\tIII\n"
             u8"verso\tfold\tcodex\t3.5\tIV")
            .styleClass("figures")
            .font({.size = 11.5f})
            .width(520.0f)
            .block({.tabStops = stops}));
  }

  Element columnPanel() {
    const weave::ParagraphStyle heading{
        .leading = weave::Leading::multiple(1.35f), .spaceAfter = 18};
    const weave::ParagraphStyle verse{
        .leading = weave::Leading::multiple(1.05f),
        .indent = {.firstLine = 26}};

    return s::panel(
        "COLUMNS",
        "the same controls a quarter turn round: the pitch is the "
        "column's width, the indents run down it, and the air between "
        "blocks is a gap across the page.",
        document::paragraph(u8"縦組み\n"
                            u8"行の間隔は"
                            u8"段落ごとに"
                            u8"決まり、縦"
                            u8"に組めばそ"
                            u8"れが列の幅"
                            u8"になる。")
            .styleClass("body")
            .font({.size = 15.0f})
            .width(210.0f)
            .height(250.0f)
            .block({.writingMode = weave::WritingMode::kVerticalRL})
            .paragraphStyles({heading, verse}));
  }

  /// One column of panels, ruled apart the way the sheet rules its
  /// header off from its content.
  Element panels(std::vector<Element> run) {
    return kit::cells({.cells = std::move(run),
                       .column = true,
                       .gap = 16,
                       .divider = Fill::color(s::kFaint)});
  }

  Element describe() {
    return sketch::kit::page(
               {.title = u8"THE BLOCK CONTROLS",
                .subtitle = u8"one text leaf per panel, and a list of "
                            u8"ParagraphStyles beside it",
                .footer =
                    u8"a block with no style of its own is set by the leaf's "
                    u8"own alignment, justification, hyphenation and tab "
                    u8"stops \u2014 which is what every text that never "
                    u8"mentions a block gets"},
               kit::cells({.cells = {panels({leadingPanel(), spacingPanel(),
                                             indentPanel()}),
                                     panels({justifiedPanel(), tabPanel(),
                                             columnPanel()})},
                           .gap = 40}))
        .styleSheet(s::classes());
  }
};

}  // namespace

SIGIL_SKETCH_AS(ParagraphSheet, "paragraph_sheet", "Specimen",
                "the block controls — leading, spacing, indents, "
                "justification, tabs")
