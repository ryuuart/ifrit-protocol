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

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/pattern/Patterns.h>
#include <sigilmaterial/skia/Paint.h>
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
using sigil::compose::toU8;

namespace {

constexpr float kPanel = 208.0f;
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

/** The house sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::houseTheme();
  look.palette.ground = {0.055f, 0.06f, 0.085f, 1};
  look.palette.ink = {0.90f, 0.93f, 0.97f, 1};
  look.palette.ash = {0.55f, 0.60f, 0.70f, 1};
  look.palette.rule = {0.19f, 0.20f, 0.26f, 1};
  look.type.title = {.size = 15, .track = 2};
  look.type.subtitle = {.size = 11, .track = 0.6f};
  look.type.footer = {.size = 10.5f, .track = 0.2f};
  look.type.captionLabel = {.size = 13, .track = 0.4f};
  look.type.captionNote = {.size = 11, .track = 0.2f};
  look.spacing.marginX = 30;
  look.spacing.marginTop = 22;
  look.spacing.captionGap = 6;
  return look;
}

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
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
const sk_sp<SkImage>& matte() {
  static const sk_sp<SkImage> img = [] {
    const int n = (int)kPanel;
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(n, n));
    SkCanvas* c = s->getCanvas();
    c->clear(SK_ColorTRANSPARENT);
    const float mid = kPanel * kSplit;
    SkPaint p;
    for (int y = 0; y < n; ++y) {
      const float f = (float)y / (float)(n - 1);  // 0 at top, 1 at bottom
      p.setColor4f({1 - f, 1 - f, 1 - f, 1}, nullptr);
      c->drawRect(SkRect::MakeXYWH(0, (float)y, mid, 1), p);
      p.setColor4f({1, 1, 1, 1 - f}, nullptr);
      c->drawRect(SkRect::MakeXYWH(mid, (float)y, kPanel - mid, 1), p);
    }
    return s->makeImageSnapshot();
  }();
  return img;
}

mskia::Paint atPanelSize(const sk_sp<SkImage>& image, float w, float h) {
  return mskia::Paint::image(
      image, SkTileMode::kClamp, SkTileMode::kClamp,
      SkMatrix::Scale(w / (float)image->width(), h / (float)image->height()));
}

/** THE CONTENT — one node, drawn six times. Saturated and structured, so
 *  partial coverage reads as partial coverage and not as a colour
 *  shift. */
Element content(float w, float h) {
  return box()
      .width(w)
      .height(h)
      .fill(mskia::Paint::linearUnit({0, 0}, {1, 1},
                                     {{0.0f, {1.0f, 0.85f, 0.20f, 1}},
                                      {0.5f, {0.95f, 0.32f, 0.42f, 1}},
                                      {1.0f, {0.35f, 0.40f, 0.98f, 1}}}))
      .alignItems(Align::Center)
      .justify(Justify::Center)
      .child(text(u8"MATTE", label(30, {1, 1, 1, 0.92f})));
}

/** A panel: checkerboard, then the content, then the gate. */
Element cell(float w, float h, Element inner) {
  return stack()
      .width(w)
      .height(h)
      .stroke(stroke(1.0f, Fill::color(kFrame)))
      .child(box().inset(0).fill(checker()))
      .child(std::move(inner));
}

/** Which band is which, in the same order the run declares them. */
Element bandLabels(float stripW) {
  Element row = box().row().width(stripW);
  for (const Band& band : kBands)
    row.child(box()
                  .width(stripW / (float)kBands.size())
                  .justify(Justify::Center)
                  .child(text(toU8(band.label), label(10, kDim))));
  return row;
}

}  // namespace

struct MatteLuma final : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    const sketch::kit::Provide look(sheetTheme());
    sketch::kit::stage(ctx, {.size = {1180, 620}});
    // Every gate is a constant: the sheet is complete on the first frame.
    ctx.captureAt(0.05);

    // ONE coverage paint, handed to all four gates.
    const mskia::Paint coverage = atPanelSize(matte(), kPanel, kPanel);

    const auto gated = [&](Gate gate) {
      Element inner = content(kPanel, kPanel);
      inner.mask(std::move(gate));
      return cell(kPanel, kPanel, std::move(inner));
    };
    const auto captioned = [&](const char* call, const char* note,
                               Element body) {
      return sketch::kit::caption(kPanel, toU8(call), toU8(note),
                                  std::move(body));
    };

    // The bottom row: the run as a picture, and the run as a matte.
    const float stripW = kPanel * 4 + 3 * 12;
    const mskia::Paint bands = bandStrip(stripW);
    Element bandMatted = content(stripW, 64);
    bandMatted.mask(by::luma(bands));

    Element gates = kit::cells(
        {.cells = {captioned(
                       "the coverage paint",
                       "greys on the left | white ramping in ALPHA on "
                       "the right",
                       cell(kPanel, kPanel, box().inset(0).fill(coverage))),
                   captioned("by::alpha(coverage)", "keeps what it COVERS",
                             gated(by::alpha(coverage))),
                   captioned("by::alphaOut(coverage)",
                             "\xe2\x80\xa6"
                             "and the complement",
                             gated(by::alphaOut(coverage))),
                   captioned("by::luma(coverage)", "keeps what is BRIGHT",
                             gated(by::luma(coverage))),
                   captioned("by::lumaOut(coverage)",
                             "\xe2\x80\xa6"
                             "and the complement",
                             gated(by::lumaOut(coverage)))},
         .gap = 12});

    Element law =
        box()
            .column()
            .gap(6)
            .child(text(toU8("Rec. 601 on ENCODED values \xc2\xb7 each colour "
                             "paired with its 0.299 R + 0.587 G + 0.114 B "
                             "grey twin"),
                        label(13, kInk)))
            .child(cell(stripW, 64, box().inset(0).fill(bands)))
            .child(bandLabels(stripW))
            .child(text(toU8("\xe2\x80\xa6"
                             "the same eight bands as a by::luma "
                             "matte \xe2\x86\x93 each pair reads the SAME"),
                        label(11, kDim))
                       .margin(0, 6, 0, 0))
            .child(cell(stripW, 64, std::move(bandMatted)));

    ctx.composer.render(sketch::kit::page(
        {.title = toU8("TRACK MATTES \xc2\xb7 by::alpha / alphaOut / "
                       "luma / lumaOut"),
         .subtitle = toU8("one content, one coverage paint, four gates "
                          "\xe2\x80\x94 right halves match between alpha "
                          "and luma because the luma is taken on the "
                          "PREMULTIPLIED colour; left halves do not"),
         .footer = toU8("Y' = 0.299 R' + 0.587 G' + 0.114 B' \xc2\xb7 "
                        "Rec. 709's luminance coefficients on encoded "
                        "values would break every pair above")},
        kit::cells({.cells = {std::move(gates), std::move(law)},
                    .column = true,
                    .gap = 26})));
  }
};

SIGIL_SKETCH(
    MatteLuma, "Kit \xc2\xb7 API",
    "by::alpha / alphaOut / luma / lumaOut on one content \xe2\x80\x94 and "
    "the Rec. 601 pairs that prove the law")
