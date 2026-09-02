// material_child.cpp — ONE API: Material::sksl(...).child(name, Material).
// =============================================================================
// A shader with TWO sources. `uIndex` is not a picture — its red BYTE is a
// palette index — and `uPalette` is the 16-entry LUT that index selects
// from. That rule ("look this number up over there") is expressible in SkSL
// and nowhere else in the library, which is what the child slot is for.
//
// Every panel below is the SAME index texture: a 4x4 chart carrying the
// indices 0..15 in reading order. Only the child changes.
//
// EDIT THESE FIRST
//   kShade  — the `uShade` uniform, i.e. X-COM's `min(i + shade, 15)`.
//             At 0 the chart reads 0..15; at 6 the top cells flatten onto
//             the last LUT entry, which is what index arithmetic looks like.
//   kSwapEvery — seconds between LUT swaps on the LIVE panel.
//   the LUT tables (greyLut / fireLut / iceLut) — swap a colour and only
//             panels holding that LUT move: children ride the prune
//             signature, so two materials with different children are
//             never equal and the reconciler repatches exactly those.
//
// The three ways things move (hello.cpp): the LIVE panel is door 3.
// update() changes DATA (which LUT is current), re-describes, and the
// reconciler diffs — there is no binding and no per-frame work anywhere.

#include <include/core/SkBitmap.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/core/Material.h>
#include <sigilcompose/typography/Type.h>
#include <sigilsketch/canvas/Sketch.h>

#include <vector>

namespace sketch = sigil::sketch;

using namespace sigil::compose;

namespace {

constexpr float kShade = 6.0f;      // uShade on the fourth panel
constexpr double kSwapEvery = 0.8;  // seconds per LUT on the live panel
constexpr int kCells = 4;           // the index chart is kCells x kCells

sigil::weave::TextStyle type(float size, SkColor4f color) {
  return sigil::compose::type({.size = size, .color = color});
}

const SkColor4f kInk{0.90f, 0.93f, 0.97f, 1};
const SkColor4f kDim{0.55f, 0.60f, 0.70f, 1};
const SkColor4f kFrame{0.20f, 0.24f, 0.32f, 1};

/** THE SHADER. Two `uniform shader` slots, one float. Monolithic main() —
 *  see stock_materials.cpp for why a helper function here is a crash. */
sk_sp<SkRuntimeEffect> paletteEffect() {
  static const sk_sp<SkRuntimeEffect> fx = [] {
    auto [effect, error] = SkRuntimeEffect::MakeForShader(
        SkString("uniform shader uIndex;"
                 "uniform shader uPalette;"
                 "uniform float uShade;"
                 "half4 main(float2 xy) {"
                 "  float i = floor(uIndex.eval(xy).r * 255.0 + 0.5);"
                 "  i = min(i + uShade, 15.0);"
                 "  return uPalette.eval(float2(i + 0.5, 0.5));"
                 "}"));
    return effect;
  }();
  return fx;
}

/** A 1-row LUT. No colour space, like every compose surface — the byte
 *  written here is the byte the shader reads. */
sk_sp<SkImage> lut(const std::vector<SkColor>& entries) {
  SkBitmap bm;
  bm.allocN32Pixels((int)entries.size(), 1);
  for (size_t i = 0; i < entries.size(); ++i)
    *bm.getAddr32((int)i, 0) = SkPreMultiplyColor(entries[i]);
  bm.setImmutable();
  return bm.asImage();
}

SkColor grey(int v) { return SkColorSetARGB(255, v, v, v); }

/** The three LUTs. Held as process-wide singletons because Material::image
 *  compares by image POINTER — minting a fresh SkImage per describe would
 *  make every material unequal to every other and defeat every prune. */
const sk_sp<SkImage>& greyLut() {
  static const sk_sp<SkImage> img = [] {
    std::vector<SkColor> v;
    v.reserve(16);
    for (int i = 0; i < 16; ++i) v.push_back(grey(17 * i));
    return lut(v);
  }();
  return img;
}
const sk_sp<SkImage>& fireLut() {
  static const sk_sp<SkImage> img = lut(
      {0xff100005, 0xff2a0410, 0xff450a16, 0xff62111a, 0xff7f1a1c, 0xff9c261b,
       0xffb8351a, 0xffd04718, 0xffe25c17, 0xffee7419, 0xfff58f26, 0xfff9a840,
       0xfffcc063, 0xfffdd68e, 0xfffee8bd, 0xffffffff});
  return img;
}
const sk_sp<SkImage>& iceLut() {
  static const sk_sp<SkImage> img = lut(
      {0xff03060f, 0xff071228, 0xff0b1f42, 0xff102c5c, 0xff143a76, 0xff17498f,
       0xff1a59a7, 0xff1f6bbc, 0xff2a7fcd, 0xff3d93da, 0xff56a7e4, 0xff74baec,
       0xff96cdf2, 0xffbadff7, 0xffdceffb, 0xffffffff});
  return img;
}

/** THE INDEX TEXTURE — a kCells x kCells chart whose red byte is 0..15 in
 *  reading order. It is DATA, so it is sampled NEAREST everywhere: an index
 *  read at kLinear is a blend of two unrelated palette entries, which is
 *  the trap this whole texture kind carries. */
const sk_sp<SkImage>& indexChart() {
  static const sk_sp<SkImage> img = [] {
    SkBitmap bm;
    bm.allocN32Pixels(kCells, kCells);
    for (int y = 0; y < kCells; ++y)
      for (int x = 0; x < kCells; ++x)
        *bm.getAddr32(x, y) =
            SkPreMultiplyColor(SkColorSetARGB(255, y * kCells + x, 0, 0));
    bm.setImmutable();
    return bm.asImage();
  }();
  return img;
}

constexpr float kPanel = 180.0f;

Material indexSource() {
  return Material::image(indexChart(), SkTileMode::kClamp, SkTileMode::kClamp,
                         SkMatrix::Scale(kPanel / kCells, kPanel / kCells),
                         SkSamplingOptions(SkFilterMode::kNearest));
}

Material lutSource(const sk_sp<SkImage>& table) {
  return Material::image(table, SkTileMode::kClamp, SkTileMode::kClamp,
                         SkMatrix::I(),
                         SkSamplingOptions(SkFilterMode::kNearest));
}

/** THE CALL SITE, in one place: one effect, two children, one uniform.
 *  Everything compiles to ONE shader — no saveLayer, no second node. */
Material paletted(const sk_sp<SkImage>& table, float shade) {
  return Material::sksl(paletteEffect())
      .uniform("uShade", shade)
      .child("uIndex", indexSource())
      .child("uPalette", lutSource(table));
}

/** The LUT itself, shown as the 16-swatch strip it is. */
Element lutStrip(const sk_sp<SkImage>& table) {
  return box().width(kPanel).height(14).fill(
      Material::image(table, SkTileMode::kClamp, SkTileMode::kClamp,
                      SkMatrix::Scale(kPanel / 16.0f, 14.0f),
                      SkSamplingOptions(SkFilterMode::kNearest)));
}

Element panel(const char* title, const char* note, const sk_sp<SkImage>& table,
              float shade, const std::string& key) {
  return box()
      .width(kPanel)
      .column()
      .gap(6)
      .child(text(toU8(title), type(13, kInk)))
      .child(box()
                 .key(std::move(key))
                 .width(kPanel)
                 .height(kPanel)
                 .fill(paletted(table, shade))
                 .stroke(stroke(1.0f, Fill::color(kFrame))))
      .child(lutStrip(table))
      .child(text(toU8(note), type(11, kDim)));
}

}  // namespace

struct MaterialChild : sketch::Sketch {
  int live = 0;

  const sk_sp<SkImage>& liveLut() const {
    switch (live % 3) {
      case 0:
        return greyLut();
      case 1:
        return fireLut();
      default:
        return iceLut();
    }
  }

  Element describe(sketch::SketchContext& ctx) {
    (void)ctx;
    return stack()
        .child(text(toU8("Material::sksl(...).child(name, Material) \xc2\xb7 "
                         "an index texture through a palette LUT"),
                    type(15, kInk))
                   .left(30)
                   .top(16))
        .child(
            box()
                .row()
                .left(30)
                .top(50)
                .gap(20)
                .child(panel("the indices", "grey LUT: 0..15 as a staircase",
                             greyLut(), 0.0f, "grey"))
                .child(panel("palette A", "child(\"uPalette\", fire)",
                             fireLut(), 0.0f, "fire"))
                .child(panel("palette B", "…the SAME index texture", iceLut(),
                             0.0f, "ice"))
                .child(panel("+ uShade = 6", "min(i + 6, 15): top cells clip",
                             iceLut(), kShade, "shade"))
                .child(panel("LIVE swap", "update() re-describes; 1 patch",
                             liveLut(), 0.0f, "live")))
        .child(text(toU8("one effect, two children, ONE draw \xc2\xb7 children "
                         "ride the prune signature, so a swapped LUT "
                         "repatches and an identical one prunes"),
                    type(11, kDim))
                   .left(30)
                   .bottom(14));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1060, 335);
    ctx.background({0.055f, 0.06f, 0.085f, 1});
    ctx.captureAt(1.0);  // the live panel is on the fire LUT here
    ctx.composer.render(describe(ctx));
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    // Derived from `elapsed`, not accumulated: a still at a declared time
    // is then the same still every run.
    const int want = (int)(elapsed / kSwapEvery);
    if (want == live) return;
    live = want;
    ctx.composer.render(describe(ctx));
  }
};

SIGIL_SKETCH(
    MaterialChild, "Kit \xc2\xb7 API",
    "Material::child() \xe2\x80\x94 an index texture through a palette LUT, "
    "with the LUT swapped live by update()")
