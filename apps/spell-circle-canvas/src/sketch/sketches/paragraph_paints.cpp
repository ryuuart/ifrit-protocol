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
#include <sigilweave/kit/Hyphenation.h>
#include <sigilweave/layout/LayoutOptions.h>
#include <sigilweave/ports/SystemFontManager.h>
#include <sigilweave/style/Type.h>

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

constexpr SkColor4f kGround{0.06f, 0.06f, 0.075f, 1};
constexpr SkColor4f kCellGround{0.10f, 0.105f, 0.125f, 1};
constexpr SkColor4f kInk{0.90f, 0.90f, 0.92f, 1};
constexpr SkColor4f kAsh{0.55f, 0.56f, 0.62f, 1};
constexpr SkColor4f kRule{0.20f, 0.21f, 0.25f, 1};

/// THE PASSAGE — about two thousand words on what a page of type asks of
/// an ink, which is the same question the sheet asks of each preset.
constexpr const char8_t* kPassage =
    u8"A page of running text behaves nothing like a single word set "
    u8"large. A word is a shape; a page is a texture. The eye reads a word "
    u8"by its outline and a page by its overall grey, and the two ask "
    u8"opposite things of whatever paints them. A fill that flatters five "
    u8"capitals across a poster can vanish entirely across four hundred "
    u8"lines of eight-point body, because the same gradient that swept "
    u8"from one letter to the next now has a whole column to cross and "
    u8"moves a hundredth as fast per glyph. The reverse is just as common: "
    u8"a field of fine noise that reads as texture on a headline turns the "
    u8"same body copy into static, since its features are now smaller than "
    u8"the strokes they sit inside. Neither failure shows up in a specimen "
    u8"that only ever paints one word.\n"

    u8"Printers knew this long before anyone wrote a shader. A forme of "
    u8"type was inked by a roller that had to leave the same film on a "
    u8"capital as on a comma, and every trade manual spends its patience "
    u8"on evenness rather than on brilliance. Too much ink and the "
    u8"counters fill, the page goes muddy and the descenders bleed into "
    u8"the line beneath; too little and the letters break up, the serifs "
    u8"drop out and the reader's eye starts to hunt. The compositor's "
    u8"answer was never a clever ink. It was a measured one, applied at a "
    u8"coverage the whole forme could carry, judged by holding the sheet "
    u8"at arm's length and squinting until the words dissolved and only "
    u8"the grey remained. That test still works, and nothing has replaced "
    u8"it.\n"

    u8"Measure is the other half. A line long enough to hold eighteen "
    u8"words is a line the eye can lose its place in on the return sweep, "
    u8"and a line short enough to hold four is a line that breaks every "
    u8"phrase in the wrong place and leaves the right edge in tatters. "
    u8"Somewhere between sixty and seventy-five characters the two "
    u8"problems trade places, which is why so many books of very different "
    u8"centuries and very different faces arrive at nearly the same "
    u8"column. The number is not magic; it follows from how far a reader's "
    u8"eye can jump accurately and how much a single fixation takes in. "
    u8"Narrow the measure and the leading has to come down with it, or the "
    u8"column turns into a ladder of stripes.\n"

    u8"Justification is where a page shows its manners. Setting both edges "
    u8"flush means the difference between the natural width of a line and "
    u8"the measure has to be spent somewhere, and the only places to spend "
    u8"it are the word gaps, the space between the letters, and the "
    u8"letters themselves. Spend it all on the gaps and a loose line opens "
    u8"holes that stack down the column into a river; spend it on the "
    u8"letters and the texture changes weight from line to line, which is "
    u8"worse, because the reader feels it without seeing why. A good "
    u8"setting spends a little in each place and asks the line breaker to "
    u8"find a set of breaks that keeps the whole paragraph even rather "
    u8"than making each line locally as full as it can be.\n"

    u8"The grey a page settles at is a real quantity, not a metaphor. It "
    u8"is coverage: the fraction of the column's area that carries ink, "
    u8"averaged over an area large enough to include several lines. Type "
    u8"size, leading, measure, tracking, the weight of the face and the "
    u8"fit of its sidebearings all move it, and they can be traded against "
    u8"each other. A heavier face at a wider leading can reach the same "
    u8"grey as a lighter one set tight, and the two pages will read as "
    u8"equally dark from across a room while looking nothing alike up "
    u8"close. Anything laid over the text — a tint, a gradient, a field of "
    u8"noise, a photograph — is competing with that grey, and it usually "
    u8"wins.\n"

    u8"Painting the glyphs themselves rather than the space behind them is "
    u8"the harder of the two. A background can be as busy as it likes so "
    u8"long as it stays far enough from the text in value; the letters "
    u8"keep their own contrast and the reader keeps their place. Paint "
    u8"inside the letterforms and every variation in the fill becomes a "
    u8"variation in the stroke, and a stroke a fraction of a millimetre "
    u8"wide has no room to carry a gradient at all. What the reader sees "
    u8"instead is a change of weight: pale letters look lighter, dark ones "
    u8"look bolder, and a passage that should be one voice starts to sound "
    u8"like three.\n"

    u8"The mapping matters as much as the fill. If a material's unit "
    u8"square is laid over the whole block of text — from the cap top of "
    u8"the first line to the baseline of the last — then a ramp authored "
    u8"to cross the capitals of a wordmark now crosses four hundred lines "
    u8"instead, and the first paragraph gets the top three per cent of it. "
    u8"Every line is nearly one colour, and that colour drifts so slowly "
    u8"down the page that no reader would ever call it a gradient. Map the "
    u8"square to each line instead and the ramp repeats down the column "
    u8"like wallpaper. Map it to each glyph and the page turns into "
    u8"confetti. There is no default that is right for all three, which is "
    u8"exactly why a page has to be looked at.\n"

    u8"Fields that move add a further problem, because a page of text is a "
    u8"great many small apertures onto the same moving thing. A ripple "
    u8"travelling across a headline is a legible motion: the word is wide, "
    u8"the wave is wide, and the eye follows one crest. The same ripple "
    u8"behind body copy is sampled by ten thousand disconnected strokes, "
    u8"and what the reader gets is not a wave but a shimmer — every letter "
    u8"changing value on its own schedule, none of them agreeing, the "
    u8"whole column boiling. The fix is never a faster shader. It is a "
    u8"slower wave, or a longer one, or an amplitude small enough that the "
    u8"change in value stays under what the eye notices while reading.\n"

    u8"There is a threshold, and it is lower than most people expect. A "
    u8"reader will tolerate a good deal of colour variation in display "
    u8"type and almost none in a passage they are actually reading, "
    u8"because reading is a motor task as much as a visual one and "
    u8"anything that changes between fixations costs a re-fixation. Ten "
    u8"per cent of value swing across a paragraph is decoration. Thirty is "
    u8"a distraction. Beyond that the text stops being read and starts "
    u8"being looked at, which may well be the intention, but it should be "
    u8"the intention rather than an accident of a fill that was never "
    u8"tested at this size.\n"

    u8"So a sheet like this one exists to be squinted at. Each panel takes "
    u8"the same passage, sets it in the same face at the same size in the "
    u8"same measure, and hands it to one preset paint. Nothing else "
    u8"differs. What the panels show is not whether a shader compiles — a "
    u8"test settles that in a millisecond and does — but what each fill "
    u8"does to a page: which ones hold their grey, which ones stripe, "
    u8"which ones dissolve into shimmer, and which ones are so dark in "
    u8"their lower half that the last lines of the column disappear into "
    u8"the paper.\n"

    u8"Some of the answers are predictable and worth having written down "
    u8"anyway. A ramp in unit space is nearly flat here by construction, "
    u8"since the square it is mapped through is now the height of the "
    u8"whole block. A field with fine features will alias, because the "
    u8"strokes it is drawn inside are a pixel or two wide and the field "
    u8"has no idea. A field that is mostly transparent needs something "
    u8"under it, or the text simply is not there. A field with a strong "
    u8"dark region will eat whichever paragraph lands in it, and which "
    u8"paragraph that is depends on how long the passage is — which means "
    u8"the same paint on a different page is a different paint.\n"

    u8"That last point is the one that argues for a page rather than a "
    u8"word. A wordmark's fill is a fixed composition: the same five "
    u8"letters, the same proportions, the same result every time it is "
    u8"drawn. A page's fill is a function of the text. Add a sentence and "
    u8"every line after it moves; the block grows taller, the unit square "
    u8"stretches, and the band of the gradient that was crossing the third "
    u8"paragraph is now crossing the fourth. Nothing in the code changed. "
    u8"A specimen that never sets more than a word cannot catch that, and "
    u8"a plate that sets a long passage catches it the first time anyone "
    u8"looks.\n"

    u8"Paper is a participant, not a backdrop. A dark ground under light "
    u8"letters thickens them optically: the pale strokes bleed outward "
    u8"into the surrounding dark, and the same face reads half a weight "
    u8"heavier than it does black on white. Printers compensated by "
    u8"choosing a lighter cut for reversed setting, and screens have the "
    u8"same problem for the same reason. It matters here because every "
    u8"panel stands on one ground while the fills that cross the letters "
    u8"range from near-white to near-black, so a single passage can look "
    u8"under-set at the top of a panel and over-inked at the bottom "
    u8"without a single glyph having changed. Judging the ink means "
    u8"judging it against the paper it was pulled on, which is why the "
    u8"ground is stated once for the whole sheet and never varied per "
    u8"panel.\n"

    u8"Leading is the quietest of the controls and the one a fill notices "
    u8"most. The space between the lines is where the ground shows "
    u8"through, and a column set tight offers the paint fewer and thinner "
    u8"windows onto the page beneath it than the same column set open. "
    u8"Open the leading and the block grows taller, which stretches "
    u8"whatever square the fill is mapped through and slows every gradient "
    u8"in it; close it and the block shortens, the gradient quickens, and "
    u8"bands that were spread over a page start landing inside single "
    u8"paragraphs. So the leading is not a neutral setting to be adjusted "
    u8"after the ink has been chosen. It is part of the composition, and a "
    u8"sheet that varies it while varying the paint has learned nothing "
    u8"about either.\n"

    u8"What a good result looks like is easy to state and hard to reach: "
    u8"from a step back the panel should read as an even grey rectangle "
    u8"with no bands, no rivers and no bright or dark patches, and from "
    u8"close up the fill should still be visible as a fill. Those two "
    u8"demands pull against each other, and every preset here resolves the "
    u8"tension differently. The most successful are the ones whose "
    u8"variation is large in scale and small in amplitude — a slow drift "
    u8"the eye reads as a tint rather than as an image. The least "
    u8"successful are not the ugliest; they are the ones that look best on "
    u8"a single word, because that is the specimen everyone reaches for "
    u8"and the one that flatters the widest range of mistakes.\n"

    u8"None of this can be settled by an assertion. A shader that resolves "
    u8"to a non-null value has proved that it resolves, and nothing more; "
    u8"the question of what it does to four hundred lines of eight-point "
    u8"type is a question about a picture, and pictures are settled by "
    u8"looking at them and by keeping the one that was looked at, so that "
    u8"the next change to any of it has something to be different from. "
    u8"That is what this sheet is: eight pulls, one forme, one ink apiece, "
    u8"held at a moment where every moving field has drifted far enough "
    u8"from its start to show what it will do and not so far that it has "
    u8"left the interesting part behind.";

/// The one hyphenator on the sheet: eight justified columns ask for it,
/// and a table borrowed by a layout has to outlive it.
const weave::kit::PatternHyphenator& hyphenator() {
  static const weave::kit::PatternHyphenator table(
      "en", weave::kit::englishHyphenationPatterns());
  return table;
}

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

weave::TextStyle mono(float size, SkColor4f color) {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
      {"SF Mono", "Menlo", "DejaVu Sans Mono", "monospace"});
  return weave::textStyle({.face = face, .size = size, .color = color});
}

/** The passage's own face: a text face with real serifs, because what a
 *  fill does to a thin stroke is half of what the sheet is about. */
weave::TextStyle body() {
  static const sk_sp<SkTypeface> face = weave::ports::pickTypeface(
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

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = mono(9.5f, kInk),
          .note = label(9, kAsh, 0.2f),
          .gap = 7,
          .noteMeasure = kPanel};
}

/// The bounds every field is parameterised over: the unit square the run's
/// metric space is mapped through, so the eight differ only in their
/// bodies.
SkRect run() { return SkRect::MakeWH(1, 1); }

/** THE COLUMN: the whole passage, set once, painted with @p fill. Every
 *  setting here is the same in all eight panels — the ink is the only
 *  thing the sheet varies. */
Element column(paint::Paint fill) {
  return text(kPassage, body())
      .width(Dim(kPanel - kInset * 2))
      .paragraph(block())
      .textAlign(weave::TextAlignment::kJustify)
      .lineBreak(weave::LineBreakStrategy::kKnuthPlass)
      .hyphenation({.patterns = &hyphenator()})
      .textFill(std::move(fill));
}

/** One panel. A second fill, when given, sets a copy of the passage UNDER
 *  the first — which is what a transparent field is drawn over. */
Element panel(const char* call, const char* note, paint::Paint fill,
              paint::Paint beneath = {}) {
  Element plate = kit::well({.width = Dim(kPanel),
                             .height = Dim(kColumn),
                             .ground = Fill::color(kCellGround),
                             .padding = kInset})
                      .column();
  if (beneath.isSolid() || beneath.asShader())
    plate.child(box().absolute().inset(0).child(column(std::move(beneath))));
  return kit::cell(voice(), toU8(call), toU8(note),
                   std::move(plate).child(column(std::move(fill))));
}

Element field(const char* call, const char* note, material::Material m) {
  return panel(call, note, paint::Paint::recipe(std::move(m)));
}

}  // namespace

struct ParagraphPaints final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(kCanvas.width(), kCanvas.height());
    ctx.background(kGround);
    ctx.captureAt(0.05);  // the fields are frozen at kMoment, not the clock
    material::skia::install();  // the SkSL compiler, once per process

    ctx.composer.render(
        kit::sheet(
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
                            "cannot show"),
             .titleStyle = label(13, kInk, 2.2f),
             .subtitleStyle = label(10.5f, kAsh, 0.6f),
             .footerStyle = label(10, kAsh, 0.3f),
             .marginX = 20,
             .marginTop = 18,
             .marginBottom = 12,
             .contentGap = 16,
             .ground = Fill::color(kGround),
             .rule = Fill::color(kRule)},
            kit::cells({.cells = {topRow(), bottomRow()},
                        .column = true,
                        .gap = 14}))
            .absolute()
            .inset(0));
  }

  Element topRow() {
    return kit::cells(
        {.cells =
             {field("kit::water(bounds, t)",
                    "rippling blue \xc2\xb7 a wave the width of the column, "
                    "sampled by every stroke in it",
                    material::kit::water(run(), kMoment)),
              field("kit::meshGradient(bounds, t)",
                    "four corners over the whole block \xc2\xb7 a paragraph "
                    "sits inside one corner's region",
                    material::kit::meshGradient(run(), kMoment)),
              panel("kit::sparkle(bounds, t)",
                    "TRANSPARENT \xc2\xb7 set here over a solid copy of the "
                    "passage; on its own the page is not there",
                    paint::Paint::recipe(material::kit::sparkle(run(),
                                                                kMoment)),
                    paint::Paint::solid({0.42f, 0.46f, 0.58f, 1})),
              field("kit::starNest(bounds, t)",
                    "a volumetric raymarch \xc2\xb7 the heaviest of the six, "
                    "and now under every glyph on a page",
                    material::kit::starNest(run(), kMoment))},
         .gap = 12});
  }

  Element bottomRow() {
    return kit::cells(
        {.cells =
             {field("kit::clouds(bounds, t)",
                    "ridged and fbm noise \xc2\xb7 large features, small "
                    "amplitude \xe2\x80\x94 the shape a page tolerates",
                    material::kit::clouds(run(), kMoment)),
              field("kit::tunnel(bounds, t)",
                    "a kaleidoscope falling away \xc2\xb7 its dark regions "
                    "swallow whichever paragraph lands in them",
                    material::kit::tunnel(run(), kMoment)),
              panel("kit::sunsetChromeType()",
                    "a stop list in UNIT space \xc2\xb7 the horizon that "
                    "crosses a wordmark's capitals now crosses the column",
                    kit::sunsetChromeType()),
              panel("kit::silverChromeType()",
                    "the same construction, colder \xc2\xb7 nearly flat over "
                    "a block this tall, which is the point",
                    kit::silverChromeType())},
         .gap = 12});
  }
};

SIGIL_SKETCH(ParagraphPaints, "Specimen",
             "one two-thousand-word passage under each preset text paint, so "
             "a page of body type judges the fill a single word flatters")
