/** @file
 * paragraph_paints — one long passage, eight times, once under each preset
 * text paint. The subject is a PAGE of body type rather than a word.
 *
 * A wordmark and a column of body copy ask opposite things of a fill. The
 * material's unit square is laid over the whole run — x across the widest
 * line, y from the first line's CAP TOP to the last line's baseline — so
 * on five capitals a ramp crosses the letterforms, and on four hundred
 * lines the same ramp crosses the COLUMN and each line gets a sliver of
 * it. A field whose features read as texture on a headline is smaller
 * than the strokes it is drawn inside at eight point, and aliases. A
 * field that moves is sampled by ten thousand disconnected apertures and
 * shimmers instead of flowing. None of that is visible in a specimen that
 * never sets more than one word, and none of it is an assertion: it is a
 * picture, and the plate is where a picture is judged.
 *
 * Every panel is ONE text leaf. Face, size, measure, leading, breaker,
 * hyphenation and justification are identical across the eight, so the
 * only variable on the sheet is the ink — eight pulls of one forme.
 *
 * The passage is deliberately longer than the panel, and the well clips
 * it: a column of body text is nearly always longer than the window it is
 * read through, and what shows here is the head of each column.
 *
 * The six animated fields are held at one moment; bind the clock and they
 * run. The two chrome ramps are stop lists in unit space and never move.
 *
 * EDIT THESE FIRST
 *   kBodySize — the passage's type size, px. The whole point of the sheet
 *               is what a fill does at BODY size; raise it and every
 *               panel starts flattering the paint again.
 *   kPanel    — one panel's measure, px, and with it the column's grey.
 *   kMoment   — the second every animated field is frozen at.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Gloss.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/kit/TextPaint.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/SkiaCompiler.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/kit/Hyphenation.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace material = sigil::material;
namespace paint = sigil::material::skia;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr SkSize kCanvas = {1200, 900};

constexpr float kPanel = 280;    // one panel's width, px
constexpr float kColumn = 330;   // the visible height of the column, px
constexpr float kInset = 8;      // the well's own padding, px
constexpr float kBodySize = 7;   // the passage's type size, px
constexpr float kMoment = 6.4f;  // the second every field is frozen at

constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};

/// THE PASSAGE — about two thousand words on what a page of type asks of
/// an ink, which is the same question the sheet asks of each preset.

/// The one hyphenator on the sheet: eight justified columns ask for it,
/// and a table borrowed by a layout has to outlive it.
const sigil::weave::kit::PatternHyphenator& hyphenator() {
  static const sigil::weave::kit::PatternHyphenator table(
      "en", sigil::weave::kit::englishHyphenationPatterns());
  return table;
}

/** The passage's own face: a text face with real serifs, because what a
 *  fill does to a thin stroke is half of what the sheet is about. */
weave::TextStyle body() {
  static const sk_sp<SkTypeface> face = weave::ports::face(
      {"Iowan Old Style", "Palatino", "Georgia", "Times New Roman"});
  weave::TextStyle style =
      weave::textStyle({.face = face, .size = kBodySize, .color = kInk});
  style.shaping.languageTag = "en-US";
  return style;
}

/// The block: a book setting — first-line indents, no air between blocks,
/// so the column is one unbroken field of grey.
weave::ParagraphStyle block() {
  weave::ParagraphStyle setting;
  setting.leading = weave::Leading::multiple(1.32f);
  setting.indent.firstLine = kBodySize * 1.6f;
  return setting;
}

/// The bounds every field is parameterised over: the unit square the run's
/// metric space is mapped through, so the eight differ only in their
/// bodies.
SkRect run() { return SkRect::MakeWH(1, 1); }

/** THE COLUMN: the whole passage, set once, painted with @p fill. Every
 *  setting here is the same in all eight panels — the ink is the only
 *  thing the sheet varies. */
Element column(const std::u8string& prose, paint::Paint fill) {
  return text(prose, body())
      .width(Dim(kPanel - kInset * 2))
      .paragraph(block())
      .textAlign(weave::TextAlignment::kJustify)
      .lineBreak(weave::LineBreakStrategy::kKnuthPlass)
      .hyphenation({.patterns = &hyphenator()})
      .textFill(std::move(fill));
}

/** One panel. A second fill, when given, sets a copy of the passage UNDER
 *  the first — which is what a transparent field is drawn over. */
Element panel(const std::u8string& prose, const char* call, const char* note,
              paint::Paint fill, paint::Paint beneath = {}) {
  Element plate =
      sketch::kit::well(
          {.width = Dim(kPanel), .height = Dim(kColumn), .padding = kInset})
          .column();
  if (beneath.isSolid() || beneath.asShader())
    plate.child(
        box().absolute().inset(0).child(column(prose, std::move(beneath))));
  return sketch::kit::caption(
      kPanel, toU8(call), toU8(note),
      std::move(plate).child(column(prose, std::move(fill))));
}

Element field(const std::u8string& prose, const char* call, const char* note,
              material::Material m) {
  return panel(prose, call, note, paint::Paint::recipe(std::move(m)));
}

/** The house sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.palette.ground = {0.06f, 0.06f, 0.075f, 1};
  look.palette.cellGround = {0.10f, 0.105f, 0.125f, 1};
  look.type.title = {.size = 13, .track = 2.2f};
  look.type.subtitle = {.size = 10.5f, .track = 0.6f};
  look.type.footer = {.size = 10, .track = 0.3f};
  look.type.captionLabel = {.size = 9.5f, .mono = true};
  look.type.captionNote = {.size = 9, .track = 0.2f};
  look.spacing.marginX = 20;
  look.spacing.marginTop = 18;
  look.spacing.marginBottom = 12;
  look.spacing.contentGap = 16;
  return look;
}

}  // namespace

struct ParagraphPaints final : sketch::Sketch {
  /** The two thousand words every panel is set in, read from beside the
   *  sketch: the passage is the sheet's SUBJECT and not its source. */
  std::u8string prose;

  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheetTheme());
    prose = sketch::kit::passage(ctx, "paragraph_paints.txt");
    // the fields are frozen at kMoment, not the clock
    sketch::kit::stage(ctx, {.size = kCanvas, .captureAt = 0.05});
    material::skia::install();  // the SkSL compiler, once per process

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("PARAGRAPH PAINTS \xc2\xb7 the preset text paints "
                       "over a page of body type"),
         .subtitle = toU8("one passage \xc2\xb7 one face, size, measure, "
                          "leading, breaker and justification "
                          "\xc2\xb7 eight inks \xc2\xb7 the moment "
                          "(6.4 s)"),
         .footer = toU8("the material's unit square spans the WHOLE run, "
                        "so a page gets a sliver of what a word gets "
                        "whole \xe2\x80\x94 which is the thing to look "
                        "for here, and the thing a one-word specimen "
                        "cannot show")},
        kit::cells(
            {.cells = {topRow(), bottomRow()}, .column = true, .gap = 14})));
  }

  Element topRow() {
    return kit::cells(
        {.cells =
             {field(prose, "kit::water(bounds, t)",
                    "rippling blue \xc2\xb7 a wave the width of the column, "
                    "sampled by every stroke in it",
                    material::kit::water(run(), kMoment)),
              field(prose, "kit::meshGradient(bounds, t)",
                    "four corners over the whole block \xc2\xb7 a paragraph "
                    "sits inside one corner's region",
                    material::kit::meshGradient(run(), kMoment)),
              panel(
                  prose, "kit::sparkle(bounds, t)",
                  "TRANSPARENT \xc2\xb7 set here over a solid copy of the "
                  "passage; on its own the page is not there",
                  paint::Paint::recipe(material::kit::sparkle(run(), kMoment)),
                  paint::Paint::solid({0.42f, 0.46f, 0.58f, 1})),
              field(prose, "kit::starNest(bounds, t)",
                    "a volumetric raymarch \xc2\xb7 the heaviest of the six, "
                    "and now under every glyph on a page",
                    material::kit::starNest(run(), kMoment))},
         .gap = 12});
  }

  Element bottomRow() {
    return kit::cells(
        {.cells =
             {field(prose, "kit::clouds(bounds, t)",
                    "ridged and fbm noise \xc2\xb7 large features, small "
                    "amplitude \xe2\x80\x94 the shape a page tolerates",
                    material::kit::clouds(run(), kMoment)),
              field(prose, "kit::tunnel(bounds, t)",
                    "a kaleidoscope falling away \xc2\xb7 its dark regions "
                    "swallow whichever paragraph lands in them",
                    material::kit::tunnel(run(), kMoment)),
              panel(prose, "kit::sunsetChromeType()",
                    "a stop list in UNIT space \xc2\xb7 the horizon that "
                    "crosses a wordmark's capitals now crosses the column",
                    kit::sunsetChromeType()),
              panel(prose, "kit::silverChromeType()",
                    "the same construction, colder \xc2\xb7 nearly flat over "
                    "a block this tall, which is the point",
                    kit::silverChromeType())},
         .gap = 12});
  }
};

SIGIL_SKETCH(ParagraphPaints, "Specimen",
             "one two-thousand-word passage under each preset text paint, so "
             "a page of body type judges the fill a single word flatters")
