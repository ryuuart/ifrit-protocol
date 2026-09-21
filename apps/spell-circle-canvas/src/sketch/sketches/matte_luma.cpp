/** @file
 * matte_luma — `by::alpha` / `by::alphaOut` / `by::luma` / `by::lumaOut`,
 * After Effects' four track mattes.
 *
 * The four-way comparison IS the sketch: ONE content node, ONE coverage
 * paint, four gates. Every panel sits on a checkerboard, so "hidden" is
 * unambiguous.
 *
 * The matte is built to separate the two readings on purpose:
 *   LEFT half   opaque greys ramping down  -> alpha is 1 everywhere,
 *                                             luma ramps.
 *   RIGHT half  white ramping down in ALPHA -> and because the luma law is
 *                                             taken on the PREMULTIPLIED
 *                                             colour, luma == alpha here.
 * So the right halves of the alpha and luma columns MATCH and the left
 * halves do not — which is what "luma is taken on the premultiplied
 * colour" looks like when you can see both readings side by side.
 *
 * The bottom row shows the Rec. 601 weights at work: eight bands used as
 * a luma matte, paired as (colour, its 601 grey twin). Green shows as
 * much as grey 0.587 and blue as little as grey 0.114 — a coloured matte
 * does NOT read like a grey one of the same apparent brightness. The
 * strip and the checkerboard are `pattern::` tiles, generated from their
 * parameters; only the two-halved matte is drawn by hand, because no
 * generator produces one field of greys beside one field of alpha.
 *
 * EDIT THESE FIRST
 *   kSplit  — where the matte's two halves meet, 0..1. Push it to 1 and
 *             the alpha column goes fully opaque while the luma column
 *             still ramps; push it to 0 and the two columns become
 *             identical.
 *   kBands  — the bottom row's (colour, grey) pairs. Swap in Rec. 709's
 *             0.2126 / 0.7152 / 0.0722 and the pairs stop matching, which
 *             is the classic mistake the law names.
 *
 * The three ways things move: none. Every gate here is a constant; a
 * gate's `fraction` / `Spans` can be bound, and each mask carries its own
 * animation slot, but a still comparison wants none.
 */

// TAGS: Materials/Compositing

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Document.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilmaterial/skia/Ramp.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>
#include <sigilweave/style/Type.h>

#include <array>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace mat = sigil::material;
namespace mskia = sigil::material::skia;
namespace ptn = sigil::material::pattern;

using namespace sigil::compose;

namespace {

constexpr float kPanel = 240.0f;
constexpr float kSplit = 0.5f;  // matte: greys left of here, alpha right

// (colour, its Rec. 601 grey twin). 0.299 R' + 0.587 G' + 0.114 B'.
struct Band {
  mat::Color color;
  const char* label;
};
const std::array<Band, 8> kBands{{
    {{1, 0, 0, 1}, "R"},
    {{0.299f, 0.299f, 0.299f, 1}, ".299"},
    {{0, 1, 0, 1}, "G"},
    {{0.587f, 0.587f, 0.587f, 1}, ".587"},
    {{0, 0, 1, 1}, "B"},
    {{0.114f, 0.114f, 0.114f, 1}, ".114"},
    {{1, 1, 1, 0.5f}, "W 50%a"},
    {{0.5f, 0.5f, 0.5f, 1}, "grey .5"},
}};

constexpr SkColor4f kInk{0.90f, 0.93f, 0.97f, 1};
constexpr SkColor4f kDim{0.55f, 0.60f, 0.70f, 1};
constexpr SkColor4f kFrame{0.24f, 0.28f, 0.36f, 1};

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.ground = {0.055f, 0.06f, 0.085f, 1};
  look.palette.ink = kInk;
  look.palette.ash = kDim;
  look.palette.rule = {0.19f, 0.20f, 0.26f, 1};
  look.type.captionLabel = {.size = 13, .track = 0.4f};
  look.spacing.marginX = 40;
  look.spacing.marginTop = 40;
  look.spacing.captionGap = 6;
  return look;
}

/** The "is it there?" backdrop — the stock checker tile, 8 px cells. */
mskia::Paint checker() {
  return mskia::Paint::shader(
      ptn::checker(8, mat::rgb(0x1a1c24), mat::rgb(0x282c38))
          .texture()
          .shader());
}

/** The eight bands as one repeating run along +x — the generator the
 *  strip is, rather than eight rectangles drawn into a bitmap. */
mskia::Paint bandStrip(float width) {
  const float bandWidth = width / (float)kBands.size();
  std::vector<std::pair<float, mat::Color>> runs;
  runs.reserve(kBands.size());
  for (const Band& band : kBands) runs.emplace_back(bandWidth, band.color);
  return mskia::Paint::shader(ptn::sequence(runs).texture().shader());
}

/** THE MATTE, baked at panel size so its local matrix is the identity.
 *  Left of kSplit: OPAQUE greys (alpha 1, luma ramps). Right: white whose
 *  ALPHA ramps — premultiplied, so its luma ramps identically. */
sk_sp<SkImage> matte() {
  const int n = (int)kPanel;
  sk_sp<SkSurface> surface =
      SkSurfaces::Raster(SkImageInfo::MakeN32Premul(n, n));
  SkCanvas* canvas = surface->getCanvas();
  canvas->clear(SK_ColorTRANSPARENT);
  const float mid = kPanel * kSplit;
  SkPaint pen;
  pen.setShader(
      mskia::verticalRamp(0, kPanel, {{0, {1, 1, 1, 1}}, {1, {0, 0, 0, 1}}}));
  canvas->drawRect(SkRect::MakeWH(mid, kPanel), pen);
  pen.setShader(
      mskia::verticalRamp(0, kPanel, {{0, {1, 1, 1, 1}}, {1, {1, 1, 1, 0}}}));
  canvas->drawRect(SkRect::MakeXYWH(mid, 0, kPanel - mid, kPanel), pen);
  return surface->makeImageSnapshot();
}

/** THE CONTENT — one node, drawn six times. Saturated and structured, so
 *  partial coverage reads as partial coverage and not as a colour
 *  shift. */
Element content(float w, float h) {
  return kit::centred()
      .width(w)
      .height(h)
      .fill(mskia::Paint::linearUnit({0, 0}, {1, 1},
                                     {{0.0f, {1.0f, 0.85f, 0.20f, 1}},
                                      {0.5f, {0.95f, 0.32f, 0.42f, 1}},
                                      {1.0f, {0.35f, 0.40f, 0.98f, 1}}}))
      .children({text(u8"MATTE")
                     .font({.size = 30, .track = 0})
                     .ink(SkColor4f{1, 1, 1, 0.92f})});
}

/** A panel: the checkerboard as its ground, the content on it, the gate on
 *  that, and one hairline round the lot. */
Element cell(float w, float h, Element inner) {
  return kit::well({.width = Dimension(w),
                    .height = Dimension(h),
                    .ground = checker(),
                    .keyline = Fill::color(kFrame),
                    .placed = true})
      .children({std::move(inner)});
}

/** Which band is which, in the same order the run declares them: equal
 *  shares of the strip, one per band, so the words cannot drift off it. */
Element bandLabels(float stripW) {
  return kit::panelGrid(
      {.cells = each(
           kBands,
           [](const Band& band) {
             return kit::centred(
                 text(band.label).font({.size = 10, .track = 0}).ink(kDim));
           }),
       .columns = (int)kBands.size(),
       .gap = 0,
       .measure = stripW});
}

}  // namespace

struct MatteLuma {
  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = {1100, 1180}});
    // Every gate is a constant: the sheet is complete on the first frame.
    ctx.captureAt(0.05);

    // ONE coverage paint, handed to all four gates.
    const mskia::Paint coverage = mskia::Paint::image(matte());

    const auto gated = [&](Gate gate) {
      Element inner = content(kPanel, kPanel);
      inner.mask(std::move(gate));
      return cell(kPanel, kPanel, std::move(inner));
    };
    const auto captioned = [&](const char* caseTitle, const char* call,
                               const char* note,
                               Element body) -> sketch::kit::ComparisonCase {
      return {.title = caseTitle,
              .control = call,
              .figure = std::move(body),
              .note = note};
    };

    // The bottom row: the run as a picture, and the run as a matte.
    const float stripW = 1020;
    const mskia::Paint bands = bandStrip(stripW);
    Element bandMatted = content(stripW, 64);
    bandMatted.mask(by::luma(bands));

    Element law = box().column().gap(6).children(
        {text("Rec. 601 on ENCODED values · each "
              "colour paired with its 0.299 R + 0.587 G + "
              "0.114 B grey twin")
             .font({.size = 13, .track = 0}),
         cell(stripW, 64, box().inset(0).fill(bands)), bandLabels(stripW),
         text("…"
              "the same eight bands as a by::luma "
              "matte ↓ each pair reads the SAME")
             .font({.track = 0})
             .ink(kDim)
             .margin(6, 0, 0, 0),
         cell(stripW, 64, std::move(bandMatted))});

    ctx.composer.render(sketch::kit::page(
        {.title = "Coverage and brightness are different",
         .subtitle =
             "A source, a matte, four gates—and the colour law behind them",
         .footer = "Luma uses encoded, premultiplied colour: Y′ = 0.299 R′ + "
                   "0.587 G′ + 0.114 B′."},
        box().column().gap(26).children(
            {box()
                 .row()
                 .alignItems(Align::Start)
                 .gap(20)
                 .children(
                     {sketch::kit::comparison(
                          {.cases = {captioned(
                                         "THE MATTE", "the coverage paint",
                                         "Left: opaque grey. Right: white with "
                                         "falling "
                                         "alpha.",
                                         cell(kPanel, kPanel,
                                              box().inset(0).fill(coverage))),
                                     captioned("THE CONTENT", "before the gate",
                                               "The same colour ramp and "
                                               "lettering feed all four gates.",
                                               content(kPanel, kPanel))},
                           .measure = 500,
                           .gap = 20}),
                      box().column().gap(18).children(
                          {sketch::kit::sectionHeader(
                               {.label = "TWO READINGS OF ONE MATTE",
                                .note = ""}),
                           document::caption(
                               "The left half changes colour while staying "
                               "opaque. The right half changes opacity while "
                               "staying white. This separates alpha coverage "
                               "from premultiplied brightness.")
                               .width(360),
                           document::caption(
                               "Keep and remove are complements. Checkerboard "
                               "means the content is hidden, rather than "
                               "painted black.")
                               .width(360)})}),
             sketch::kit::sectionHeader(
                 {.label = "KEEP / REMOVE",
                  .note = "Alpha pair on the left · luma pair on the right"}),
             sketch::kit::comparison(
                 {.cases =
                      {captioned("ALPHA · KEEP", "by::alpha(coverage)",
                                 "The grey half remains fully visible.",
                                 gated(by::alpha(coverage))),
                       captioned("ALPHA · REMOVE", "by::alphaOut(coverage)",
                                 "The grey half disappears.",
                                 gated(by::alphaOut(coverage))),
                       captioned("LUMA · KEEP", "by::luma(coverage)",
                                 "Brightness reduces both halves.",
                                 gated(by::luma(coverage))),
                       captioned(
                           "LUMA · REMOVE", "by::lumaOut(coverage)",
                           "The brightness complement reveals both halves.",
                           gated(by::lumaOut(coverage)))},
                  .measure = 1020,
                  .gap = 20}),
             sketch::kit::sectionHeader(
                 {.label = "THE COLOUR WEIGHTS",
                  .note = "The coloured bands and their grey twins must "
                          "produce the same coverage."}),
             std::move(law)})));
  }
};

SIGIL_SKETCH(MatteLuma, "Kit · API",
             "by::alpha / alphaOut / luma / lumaOut on one content — and "
             "the Rec. 601 pairs that prove the law")
