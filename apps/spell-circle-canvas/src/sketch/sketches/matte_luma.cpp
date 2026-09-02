// matte_luma.cpp — ONE API FAMILY: by::alpha / by::alphaOut / by::luma /
// by::lumaOut, After Effects' four track mattes.
// =============================================================================
// The four-way comparison IS the sketch: ONE content node, ONE coverage
// Material, four gates. Every panel sits on a checkerboard, so "hidden"
// is unambiguous.
//
// The matte is built to separate the two readings on purpose:
//   LEFT half   opaque greys ramping down  -> alpha is 1 everywhere,
//                                             luma ramps.
//   RIGHT half  white ramping down in ALPHA -> and because the luma law is
//                                             taken on the PREMULTIPLIED
//                                             colour, luma == alpha here.
// So the right halves of the alpha and luma columns MATCH and the left
// halves do not — which is what "luma is taken on the premultiplied
// colour" looks like when you can see both readings side by side.
//
// The bottom row shows the Rec. 601 weights at work: eight bands used as a
// luma matte, paired as (colour, its 601 grey twin). Green shows as much as
// grey 0.587 and blue as little as grey 0.114 — a coloured matte does NOT
// read like a grey one of the same apparent brightness.
//
// EDIT THESE FIRST
//   kSplit  — where the matte's two halves meet, 0..1. Push it to 1 and the
//             alpha column goes fully opaque while the luma column still
//             ramps; push it to 0 and the two columns become identical.
//   kBands  — the bottom row's (colour, grey) pairs. Swap in Rec. 709's
//             0.2126 / 0.7152 / 0.0722 and the pairs stop matching, which
//             is the classic mistake the law names.
//
// The three ways things move: none. Every gate here is a
// constant; a gate's `fraction` / `Spans` can be bound, and each mask
// carries its own animation slot, but a still comparison wants none.

#include <include/core/SkCanvas.h>
#include <include/core/SkSurface.h>
#include <sigilcompose/core/Material.h>
#include <sigilcompose/typography/Type.h>
#include <sigilsketch/canvas/Sketch.h>

#include <array>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr float kPanel = 208.0f;
constexpr float kSplit = 0.5f;  // matte: greys left of here, alpha right

// (colour, its Rec. 601 grey twin). 0.299 R' + 0.587 G' + 0.114 B'.
struct Band {
  SkColor4f color;
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

const SkColor4f kInk{0.90f, 0.93f, 0.97f, 1};
const SkColor4f kDim{0.55f, 0.60f, 0.70f, 1};
const SkColor4f kFrame{0.24f, 0.28f, 0.36f, 1};

/** The "is it there?" backdrop. Anything hidden by a gate shows this. */
const sk_sp<SkImage>& checker() {
  static const sk_sp<SkImage> img = [] {
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(16, 16));
    SkCanvas* c = s->getCanvas();
    c->clear(SkColorSetARGB(255, 26, 28, 36));
    SkPaint p;
    p.setColor(SkColorSetARGB(255, 40, 44, 56));
    c->drawRect(SkRect::MakeXYWH(0, 0, 8, 8), p);
    c->drawRect(SkRect::MakeXYWH(8, 8, 8, 8), p);
    return s->makeImageSnapshot();
  }();
  return img;
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

/** The band strip, also baked at panel size. */
const sk_sp<SkImage>& bandStrip() {
  static const sk_sp<SkImage> img = [] {
    const int w = (int)(kPanel * 4);
    const int h = 64;
    sk_sp<SkSurface> s = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w, h));
    SkCanvas* c = s->getCanvas();
    c->clear(SK_ColorTRANSPARENT);
    SkPaint p;
    const float bw = (float)w / (float)kBands.size();
    for (size_t i = 0; i < kBands.size(); ++i) {
      p.setColor4f(kBands[i].color, nullptr);
      c->drawRect(SkRect::MakeXYWH((float)i * bw, 0, bw, (float)h), p);
    }
    return s->makeImageSnapshot();
  }();
  return img;
}

Material atPanelSize(const sk_sp<SkImage>& image, float w, float h) {
  return Material::image(
      image, SkTileMode::kClamp, SkTileMode::kClamp,
      SkMatrix::Scale(w / (float)image->width(), h / (float)image->height()));
}

/** THE CONTENT — one node, drawn five times. Saturated and structured, so
 *  partial coverage reads as partial coverage and not as a colour shift. */
Element content(float w, float h) {
  return box()
      .width(w)
      .height(h)
      .fill(Material::linearUnit({0, 0}, {1, 1},
                                 {{0.0f, {1.0f, 0.85f, 0.20f, 1}},
                                  {0.5f, {0.95f, 0.32f, 0.42f, 1}},
                                  {1.0f, {0.35f, 0.40f, 0.98f, 1}}}))
      .alignItems(Align::Center)
      .justify(Justify::Center)
      .child(text(u8"MATTE", type({.size = 30, .color = {1, 1, 1, 0.92f}})));
}

/** A panel: checkerboard, then the content, then the gate. */
Element cell(float w, float h, Element inner) {
  return stack()
      .width(w)
      .height(h)
      .stroke(stroke(1.0f, Fill::color(kFrame)))
      .child(box().inset(0).fill(
          Material::image(checker(), SkTileMode::kRepeat, SkTileMode::kRepeat)))
      .child(std::move(inner));
}

/** Which band is which, in the same order the strip bakes them. */
Element bandLabels(float stripW) {
  Element row = box().row().width(stripW);
  for (const Band& band : kBands)
    row.child(
        box()
            .width(stripW / (float)kBands.size())
            .justify(Justify::Center)
            .child(text(toU8(band.label), type({.size = 10, .color = kDim}))));
  return row;
}

Element captioned(const char* title, const char* note, Element body) {
  return box()
      .column()
      .gap(5)
      .child(text(toU8(title), type({.size = 13, .color = kInk})))
      .child(std::move(body))
      .child(text(toU8(note), type({.size = 11, .color = kDim})));
}

}  // namespace

struct MatteLuma : sketch::Sketch {
  void setup(sketch::SketchContext& ctx) override {
    ctx.captureAt(6.0);
    ctx.canvas(1180, 620);
    ctx.background({0.055f, 0.06f, 0.085f, 1});

    // ONE coverage material, handed to all four gates.
    const Material coverage = atPanelSize(matte(), kPanel, kPanel);

    const auto gated = [&](Gate gate) {
      Element inner = content(kPanel, kPanel);
      inner.mask(std::move(gate));
      return cell(kPanel, kPanel, std::move(inner));
    };

    // The bottom row: the strip as a picture, and the strip as a matte.
    const float stripW = kPanel * 4 + 3 * 12;
    const Material bands = atPanelSize(bandStrip(), stripW, 64);
    Element bandMatted = content(stripW, 64);
    bandMatted.mask(by::luma(bands));

    ctx.composer.render(
        stack()
            .child(text(toU8("by::alpha / alphaOut / luma / lumaOut \xc2\xb7 "
                             "one content, one coverage Material, four gates"),
                        type({.size = 15, .color = kInk}))
                       .left(30)
                       .top(16))

            .child(box()
                       .row()
                       .left(30)
                       .top(48)
                       .gap(12)
                       .child(captioned(
                           "the coverage material", "greys | white in alpha",
                           cell(kPanel, kPanel, box().inset(0).fill(coverage))))
                       .child(captioned("by::alpha", "keeps what it COVERS",
                                        gated(by::alpha(coverage))))
                       .child(captioned("by::alphaOut", "…and the complement",
                                        gated(by::alphaOut(coverage))))
                       .child(captioned("by::luma", "keeps what is BRIGHT",
                                        gated(by::luma(coverage))))
                       .child(captioned("by::lumaOut", "…and the complement",
                                        gated(by::lumaOut(coverage)))))

            .child(text(toU8("right halves match between alpha and luma "
                             "because the luma is taken on the PREMULTIPLIED "
                             "colour \xc2\xb7 left halves do not"),
                        type({.size = 11, .color = kDim}))
                       .left(30)
                       .top(336))

            .child(box()
                       .column()
                       .left(30)
                       .top(378)
                       .gap(6)
                       .child(text(toU8("Rec. 601 on ENCODED values \xc2\xb7 "
                                        "each colour paired with its 0.299 R "
                                        "+ 0.587 G + 0.114 B grey twin"),
                                   type({.size = 13, .color = kInk})))
                       .child(cell(stripW, 64, box().inset(0).fill(bands)))
                       .child(bandLabels(stripW))
                       .child(text(toU8("…the same eight bands as a "
                                        "by::luma matte \xe2\x86\x93 each "
                                        "pair reads the SAME"),
                                   type({.size = 11, .color = kDim}))
                                  .margin(0, 6, 0, 0))
                       .child(cell(stripW, 64, std::move(bandMatted))))

            .child(text(toU8("Y' = 0.299 R' + 0.587 G' + 0.114 B' \xc2\xb7 "
                             "Rec. 709's luminance coefficients on encoded "
                             "values would break every pair above"),
                        type({.size = 11, .color = kDim}))
                       .left(30)
                       .bottom(14)));
  }
};

SIGIL_SKETCH(
    MatteLuma, "Kit \xc2\xb7 API",
    "by::alpha / alphaOut / luma / lumaOut on one content \xe2\x80\x94 and "
    "the Rec. 601 pairs that prove the law")
