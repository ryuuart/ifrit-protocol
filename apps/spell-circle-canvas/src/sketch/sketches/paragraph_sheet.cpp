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

#include <sigilcompose/kit/Specimen.h>
#include <sigilcompose/typography/Typography.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/kit/Hyphenation.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;
using sigil::compose::toU8;
namespace weave = sigil::weave;

namespace {

constexpr SkSize kSceneSize{1280, 1060};

namespace sheet {

constexpr float kW = kSceneSize.fWidth;
constexpr float kH = kSceneSize.fHeight;
constexpr float kMargin = 64;
constexpr float kMeasure = 330;
constexpr float kGrid = 21;

const SkColor4f kPaper{0.965f, 0.957f, 0.937f, 1};
const SkColor4f kInk{0.114f, 0.106f, 0.098f, 1};
const SkColor4f kFaint{0.114f, 0.106f, 0.098f, 0.30f};
const SkColor4f kRule{0.78f, 0.30f, 0.20f, 0.28f};
const SkColor4f kMark{0.78f, 0.30f, 0.20f, 1};

/// The one hyphenator on the sheet: the justified panel asks for it, and
/// a table borrowed by a layout has to outlive it.
const sigil::weave::kit::PatternHyphenator& hyphenator() {
  static const sigil::weave::kit::PatternHyphenator table(
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

weave::TextStyle body(float size = 13.5f, SkColor4f colour = kInk) {
  weave::TextStyle style =
      weave::textStyle({.face = serif(), .size = size, .color = colour});
  style.shaping.languageTag = "en-US";
  return style;
}
weave::TextStyle label(float size = 9.0f, float track = 1.6f,
                       SkColor4f colour = kFaint) {
  return weave::textStyle(
      {.face = grotesque(), .size = size, .color = colour, .track = track});
}
weave::TextStyle figures(float size = 12.0f) {
  return weave::textStyle({.face = mono(), .size = size, .color = kInk});
}

/// THIS SHEET'S LOOK: paper and its ink, set in the grotesque, with the
/// page's own generous margin.
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look;
  look.palette = {
      .ground = kPaper, .ink = kInk, .ash = kFaint, .rule = kFaint};
  look.type.sans = grotesque();
  look.type.title = {.size = 11, .track = 4.0f};
  look.type.subtitle = {.size = 10, .track = 0.4f};
  look.type.footer = {.size = 9.5f, .track = 0.3f};
  look.spacing.marginX = kMargin;
  look.spacing.marginTop = kMargin;
  look.spacing.marginBottom = kMargin * 0.5f;
  return look;
}

/// THE PANEL'S VOICE: the control's name, what it decides under it, and
/// the specimen under both.
kit::Caption panelVoice() {
  return {.where = kit::Caption::Where::Above,
          .label = label(9.5f, 2.0f, kMark),
          .note = label(9.0f, 0.4f),
          .gap = 13,
          .noteGap = 9,
          .noteMeasure = kMeasure};
}

/// THE SPECIMEN'S VOICE, inside a panel: the call that set it, and the
/// setting under the call.
kit::Caption callVoice(float measure) {
  return {.where = kit::Caption::Where::Above,
          .label = label(8.5f, 1.2f),
          .note = label(8.5f, 1.2f),
          .gap = 5,
          .noteMeasure = measure};
}

/// A panel: a name, what the control decides, and the specimen under it.
Element panel(const char* name, const char* note, Element specimen) {
  return kit::cell(panelVoice(), toU8(name), toU8(note), std::move(specimen));
}

constexpr const char8_t* kFourWays =
    u8"A block states its own pitch, and the extra a leading opens goes "
    u8"above the line, where leading has always gone.";

/// The specimen the leading panel repeats, once per leading kind.
Element leadingSpecimen(const char* caption, weave::Leading leading) {
  weave::ParagraphStyle style;
  style.leading = leading;
  return kit::cell(callVoice(kMeasure * 0.48f), toU8(caption), u8"",
                   text(kFourWays, body(11.5f))
                       .width(Dim(kMeasure * 0.48f))
                       .paragraph(style))
      .width(Dim(kMeasure * 0.48f));
}

}  // namespace sheet

struct ParagraphSheet final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheet::sheetTheme());
    sketch::kit::stage(ctx, {.size = kSceneSize, .captureAt = 0.4});
    ctx.composer.render(describe());
  }

  // ── The panels ─────────────────────────────────────────────────────────

  Element leadingPanel() {
    namespace s = sheet;
    weave::ParagraphStyle grid;
    grid.leading = weave::Leading::grid(s::kGrid);
    return s::panel(
        "LEADING",
        "face \xc2\xb7 multiple \xc2\xb7 absolute \xc2\xb7 grid. The rules "
        "under the fourth are the grid it lands on.",
        box()
            .column()
            .gap(14)
            .child(
                box()
                    .row()
                    .gap(18)
                    .child(s::leadingSpecimen("Leading::face()",
                                              weave::Leading::face()))
                    .child(s::leadingSpecimen("Leading::multiple(1.7)",
                                              weave::Leading::multiple(1.7f))))
            .child(
                box()
                    .row()
                    .gap(18)
                    .child(s::leadingSpecimen("Leading::absolute(22)",
                                              weave::Leading::absolute(22)))
                    .child(kit::cell(
                               s::callVoice(s::kMeasure * 0.48f),
                               toU8("Leading::grid(21)"), u8"",
                               // The grid, drawn: every rule is one
                               // step, so a baseline off the rhythm is
                               // a thing to see rather than to argue
                               // about.
                               box()
                                   .height(Dim(s::kGrid * 4))
                                   .width(Dim(s::kMeasure * 0.48f))
                                   .child(gridRules())
                                   .child(text(s::kFourWays, s::body(11.5f))
                                              .absolute()
                                              .inset(0, 0, 0, 0)
                                              .width(Dim(s::kMeasure * 0.48f))
                                              .paragraph(grid)))
                               .width(Dim(s::kMeasure * 0.48f)))));
  }

  /// Four rules one grid step apart, behind the grid specimen.
  Element gridRules() {
    namespace s = sheet;
    Element stackOfRules = box().absolute().inset(0, 0, 0, 0);
    for (int line = 0; line < 4; ++line)
      stackOfRules.child(box()
                             .absolute()
                             .left(Dim(0.0f))
                             .top(Dim(s::kGrid * static_cast<float>(line + 1)))
                             .width(Dim(s::kMeasure * 0.48f))
                             .height(Dim(1.0f))
                             .fill(Fill::color(s::kRule)));
    return stackOfRules;
  }

  Element spacingPanel() {
    namespace s = sheet;
    weave::ParagraphStyle first;
    first.spaceBefore = 14;  // not suppressed at the head of the flow
    first.spaceAfter = 26;
    weave::ParagraphStyle second;
    second.spaceBefore = 10;  // 26 wins: the gap is the larger, not the sum
    second.spaceAfter = 6;
    weave::ParagraphStyle third;
    third.spaceBefore = 24;  // 24 wins here, over the 6 before it

    return s::panel(
        "SPACING",
        "the gap between two blocks is the LARGER of the first's "
        "spaceAfter and the second's spaceBefore \xe2\x80\x94 26 then 24, "
        "never 36 or 30.",
        text(u8"after 26, before 10 \xe2\x80\x94 the gap under this block "
             u8"is twenty-six.\n"
             u8"after 6, before 24 \xe2\x80\x94 and the gap under THIS one "
             u8"is twenty-four.\n"
             u8"The block above claimed six and the one below claimed "
             u8"twenty-four, so twenty-four stands.",
             s::body())
            .width(Dim(s::kMeasure))
            .paragraphs({first, second, third}));
  }

  Element indentPanel() {
    namespace s = sheet;
    weave::ParagraphStyle firstLine;
    firstLine.indent.firstLine = 22;
    firstLine.spaceAfter = 10;
    weave::ParagraphStyle hanging;
    hanging.indent.start = 22;
    hanging.indent.firstLine = -22;
    hanging.spaceAfter = 10;
    weave::ParagraphStyle bothEnds;
    bothEnds.indent.start = 20;
    bothEnds.indent.end = 20;
    bothEnds.spaceAfter = 10;
    weave::ParagraphStyle lastLine;
    lastLine.indent.lastLine = 40;

    return s::panel(
        "INDENTS",
        "start and end on every line, firstLine and lastLine added to "
        "start on those two \xe2\x80\x94 all of it arithmetic on the "
        "intervals the geometry handed back.",
        text(u8"A first-line indent moves the opening of the block and "
             u8"nothing else, which is the oldest way to mark a "
             u8"paragraph.\n"
             u8"A hanging indent is the same field negative against a "
             u8"start indent, so the first line comes out to the margin "
             u8"and the rest stay in.\n"
             u8"Indenting both ends narrows the measure without moving "
             u8"the block, which is how a quotation stands apart from "
             u8"the text around it.\n"
             u8"A last-line indent pulls the closing line in, and it is "
             u8"the fit that decides which line that is.",
             s::body())
            .width(Dim(s::kMeasure))
            .paragraphs({firstLine, hanging, bothEnds, lastLine}));
  }

  Element justifiedPanel() {
    namespace s = sheet;
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
      return kit::cell(s::callVoice(s::kMeasure * 0.31f), toU8(caption), u8"",
                       text(passage, s::body(11.0f))
                           .width(Dim(s::kMeasure * 0.31f))
                           .textAlign(weave::TextAlignment::kJustify)
                           .lineBreak(weave::LineBreakStrategy::kKnuthPlass)
                           .hyphenation({.patterns = &s::hyphenator()})
                           .justification(spec))
          .width(Dim(s::kMeasure * 0.31f));
    };

    return s::panel(
        "JUSTIFIED",
        "word gaps, then letter spacing, then a glyph scale \xe2\x80\x94 "
        "each bounded by its own two limits, and a pass at its default "
        "contributes nothing.",
        box()
            .row()
            .gap(14)
            .child(column("gaps alone", gapsOnly))
            .child(column("+ letter spacing", withLetters))
            .child(column("+ glyph scale", withScale)));
  }

  Element tabPanel() {
    namespace s = sheet;
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
             u8"verso\tfold\tcodex\t3.5\tIV",
             s::figures(11.5f))
            .width(Dim(520.0f))
            .tabStops(stops));
  }

  Element columnPanel() {
    namespace s = sheet;
    weave::ParagraphStyle heading;
    heading.leading = weave::Leading::multiple(1.35f);
    heading.spaceAfter = 18;
    weave::ParagraphStyle verse;
    verse.leading = weave::Leading::multiple(1.05f);
    verse.indent.firstLine = 26;

    weave::TextStyle vertical = s::body(15.0f);
    return s::panel(
        "COLUMNS",
        "the same controls a quarter turn round: the pitch is the "
        "column's width, the indents run down it, and the air between "
        "blocks is a gap across the page.",
        text(u8"\xe7\xb8\xa6\xe7\xb5\x84\xe3\x81\xbf\n"
             u8"\xe8\xa1\x8c\xe3\x81\xae\xe9\x96\x93\xe9\x9a\x94\xe3\x81\xaf"
             u8"\xe6\xae\xb5\xe8\x90\xbd\xe3\x81\x94\xe3\x81\xa8\xe3\x81\xab"
             u8"\xe6\xb1\xba\xe3\x81\xbe\xe3\x82\x8a\xe3\x80\x81\xe7\xb8\xa6"
             u8"\xe3\x81\xab\xe7\xb5\x84\xe3\x82\x81\xe3\x81\xb0\xe3\x81\x9d"
             u8"\xe3\x82\x8c\xe3\x81\x8c\xe5\x88\x97\xe3\x81\xae\xe5\xb9\x85"
             u8"\xe3\x81\xab\xe3\x81\xaa\xe3\x82\x8b\xe3\x80\x82",
             vertical)
            .width(Dim(210.0f))
            .height(Dim(250.0f))
            .writingMode(weave::WritingMode::kVerticalRL)
            .paragraphs({heading, verse}));
  }

  /// One column of panels, ruled apart the way the sheet rules its
  /// header off from its content.
  Element panels(std::vector<Element> run) {
    return kit::cells({.cells = std::move(run),
                       .column = true,
                       .gap = 22,
                       .divider = Fill::color(sheet::kFaint)});
  }

  Element describe() {
    namespace s = sheet;
    std::vector<Element> left;
    left.push_back(leadingPanel());
    left.push_back(spacingPanel());
    left.push_back(indentPanel());
    std::vector<Element> right;
    right.push_back(justifiedPanel());
    right.push_back(tabPanel());
    right.push_back(columnPanel());

    return sketch::kit::page(
        {.title = u8"THE BLOCK CONTROLS",
         .subtitle = u8"one text leaf per panel, and a list of "
                     u8"ParagraphStyles beside it",
         .footer = u8"a block with no style of its own is set by the leaf's "
                   u8"own alignment, justification, hyphenation and tab "
                   u8"stops \u2014 which is what every text that never "
                   u8"mentions a block gets"},
        kit::cells({.cells = {panels(std::move(left)),
                              panels(std::move(right))},
                    .gap = 40}));
  }
};

}  // namespace

SIGIL_SKETCH_AS(ParagraphSheet, "paragraph_sheet", "Specimen",
                "the block controls \xe2\x80\x94 leading, spacing, indents, "
                "justification, tabs")
