/** @file
 * material_child — a material filling another's child slot, twice over:
 * the SkSL slot a shader samples, and the three slots `over()` stacks.
 *
 * TOP ROW — `Paint::sksl(...).child(name, Paint)`. A shader with TWO
 * sources. `uIndex` is not a picture — its red BYTE is a palette index —
 * and `uPalette` is the 16-entry LUT that index selects from. That rule
 * ("look this number up over there") is expressible in SkSL and nowhere
 * else in the library, which is what the child slot is for. Every panel
 * is the SAME index texture: a 4×4 chart carrying the indices 0..15 in
 * reading order. Only the child changes.
 *
 * BOTTOM ROW — `over(base, top, mask)`. The same idea one level up: a
 * stack is not a bespoke recipe per pair but a material whose three
 * operands are its children, so it compares, animates and resolves as one
 * material. The mask is an ordinary material read as a scalar (its red
 * channel), which is why a grain field can be a mask without being
 * anything special. Base, top and mask stand beside the stack they make,
 * and the same stack is shown under each of the three blends.
 *
 * EDIT THESE FIRST
 *   kShade  — the `uShade` uniform, i.e. X-COM's `min(i + shade, 15)`.
 *             At 0 the chart reads 0..15; at 6 the top cells flatten onto
 *             the last LUT entry, which is what index arithmetic looks
 *             like.
 *   kSwapEvery — seconds between LUT swaps on the LIVE panel.
 *   the LUT tables (greyLut / fireLut / iceLut) — swap a colour and only
 *             panels holding that LUT move: children ride the prune
 *             signature, so two materials with different children are
 *             never equal and the reconciler repatches exactly those.
 *   kMaskContrast — how hard the grain field cuts as the stack's mask.
 *
 * The three ways things move: the LIVE panel is door 3. update() changes
 * DATA (which LUT is current), re-describes, and the reconciler diffs —
 * there is no binding and no per-frame work anywhere.
 */

#include <include/core/SkBitmap.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/style/Type.h>

#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace mat = sigil::material;
namespace mskia = sigil::material::skia;
namespace field = sigil::material::field;
namespace mkit = sigil::material::kit;

using namespace sigil::compose;
using sigil::compose::toU8;

namespace {

constexpr float kShade = 6.0f;      // uShade on the fourth panel
constexpr double kSwapEvery = 0.8;  // seconds per LUT on the live panel
constexpr int kCells = 4;           // the index chart is kCells x kCells

constexpr float kMaskContrast = 3.2f;  // how hard the grain field's cut is

constexpr SkColor4f kGround{0.055f, 0.06f, 0.085f, 1};
constexpr SkColor4f kInk{0.90f, 0.93f, 0.97f, 1};
constexpr SkColor4f kDim{0.55f, 0.60f, 0.70f, 1};
constexpr SkColor4f kFrame{0.20f, 0.24f, 0.32f, 1};
constexpr SkColor4f kRule{0.19f, 0.20f, 0.26f, 1};
constexpr float kPanel = 180.0f;

weave::TextStyle label(float size, SkColor4f color, float track = 0) {
  return weave::textStyle({.size = size, .color = color, .track = track});
}

kit::Caption voice() {
  return {.where = kit::Caption::Where::Split,
          .label = label(12.5f, kInk, 0.4f),
          .note = label(10.5f, kDim, 0.2f),
          .gap = 6,
          .noteMeasure = kPanel};
}

/** THE SHADER. Two `uniform shader` slots, one float, and main() kept
 *  MONOLITHIC: a sketch dylib carries its own copy of Skia, so the SkSL
 *  AST is allocated in one Skia image and inlined in another, and virtual
 *  dispatch across that boundary faults on pointer authentication. A
 *  helper function here is a crash, not a style choice. */
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

mskia::Paint indexSource() {
  return mskia::Paint::image(indexChart(), SkTileMode::kClamp, SkTileMode::kClamp,
                         SkMatrix::Scale(kPanel / kCells, kPanel / kCells),
                         SkSamplingOptions(SkFilterMode::kNearest));
}

mskia::Paint lutSource(const sk_sp<SkImage>& table) {
  return mskia::Paint::image(table, SkTileMode::kClamp, SkTileMode::kClamp,
                         SkMatrix::I(),
                         SkSamplingOptions(SkFilterMode::kNearest));
}

/** THE CALL SITE, in one place: one effect, two children, one uniform.
 *  Everything compiles to ONE shader — no saveLayer, no second node. */
mskia::Paint paletted(const sk_sp<SkImage>& table, float shade) {
  return mskia::Paint::sksl(paletteEffect())
      .uniform("uShade", shade)
      .child("uIndex", indexSource())
      .child("uPalette", lutSource(table));
}

/** The LUT itself, shown as the 16-swatch strip it is. */
Element lutStrip(const sk_sp<SkImage>& table) {
  return box().width(kPanel).height(14).fill(
      mskia::Paint::image(table, SkTileMode::kClamp, SkTileMode::kClamp,
                      SkMatrix::Scale(kPanel / 16.0f, 14.0f),
                      SkSamplingOptions(SkFilterMode::kNearest)));
}

Element panel(const char* call, const char* note, const sk_sp<SkImage>& table,
              float shade, std::string key) {
  return kit::cell(voice(), toU8(call), toU8(note),
                   box()
                       .column()
                       .gap(6)
                       .child(box()
                                  .key(std::move(key))
                                  .width(kPanel)
                                  .height(kPanel)
                                  .fill(paletted(table, shade))
                                  .stroke(stroke(1.0f, Fill::color(kFrame))))
                       .child(lutStrip(table)));
}

// ------------------------------------------------------- over(base, top, mask)
// The other child slot: a stack's three operands. Latten under a crust of
// stone, cut by a grain field read as a scalar — no recipe was written for
// the pair, and the stack answers every query over all three.

mat::Material stackBase() {
  return mkit::latten({.level = 0.62f, .sheen = 0.55f, .seed = 4});
}
mat::Material stackTop() {
  return mkit::stone({.hi = {0.36f, 0.55f, 0.42f, 1},
                      .lo = {0.13f, 0.26f, 0.21f, 1},
                      .bedAngle = 62,
                      .bedLength = 70,
                      .speckle = 0.5f,
                      .seed = 9});
}
/** The MASK: an ordinary material whose red channel is read as coverage.
 *  Nothing about it is a mask — it is the grain field, and the contrast
 *  is what pushes its middle out to both ends so the cut is a patch
 *  rather than a haze. */
mat::Material stackMask() {
  return field::grain(0.018f, 4, 21.0f, kMaskContrast);
}

Element operand(const char* call, const char* note, mat::Material material,
                std::string key) {
  return kit::cell(voice(), toU8(call), toU8(note),
                   box()
                       .key(std::move(key))
                       .width(kPanel)
                       .height(kPanel)
                       .fill(mskia::Paint::recipe(std::move(material)))
                       .stroke(stroke(1.0f, Fill::color(kFrame))));
}

Element stacked(const char* call, const char* note, mat::Blend blend,
                std::string key) {
  return operand(call, note,
                 mat::over(stackBase(), stackTop(), stackMask(), blend),
                 std::move(key));
}

}  // namespace

struct MaterialChild final : sketch::Sketch {
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

  Element describe() {
    Element slots = kit::cells(
        {.cells = {panel("child(\"uPalette\", grey)",
                         "the indices themselves: a 0..15 staircase",
                         greyLut(), 0.0f, "grey"),
                   panel("child(\"uPalette\", fire)",
                         "the SAME index texture, another table", fireLut(),
                         0.0f, "fire"),
                   panel("child(\"uPalette\", ice)",
                         "\xe2\x80\xa6" "and another", iceLut(), 0.0f, "ice"),
                   panel("uniform(\"uShade\", 6)",
                         "min(i + 6, 15): the top cells flatten onto the "
                         "last entry \xe2\x80\x94 index arithmetic, drawn",
                         iceLut(), kShade, "shade"),
                   panel("the LUT swapped by update()",
                         "door 3: data changes, the tree is described again, "
                         "one node patches", liveLut(), 0.0f, "live")},
         .gap = 20});

    Element stack = kit::cells(
        {.cells = {operand("kit::latten({.level = 0.62})",
                           "the BASE of the stack", stackBase(), "base"),
                   operand("kit::stone({.bedAngle = 62})",
                           "the TOP \xe2\x80\x94 a crust with a bed of its "
                           "own", stackTop(), "top"),
                   operand("field::grain(0.018, 4, contrast 3.2)",
                           "the MASK: an ordinary material, read as its red "
                           "channel", stackMask(), "mask"),
                   stacked("over(base, top, mask)",
                           "Blend::Mix \xe2\x80\x94 the base moves toward "
                           "the top where the mask says",
                           mat::Blend::Mix, "over.mix"),
                   stacked("over(\xe2\x80\xa6, Blend::Multiply)",
                           "the same three operands, the other law: one "
                           "material, three children",
                           mat::Blend::Multiply, "over.mul")},
         .gap = 20});

    return kit::sheet(
               {.title = toU8("CHILD SLOTS \xc2\xb7 a material filling "
                              "another's"),
                .subtitle = toU8("top: Paint::sksl(\xe2\x80\xa6).child() "
                                 "\xe2\x80\x94 an index texture read "
                                 "through a palette LUT \xc2\xb7 bottom: "
                                 "over(base, top, mask) \xe2\x80\x94 the "
                                 "same idea one level up"),
                .footer = toU8("one effect, two children, ONE draw \xc2\xb7 "
                               "children ride the prune signature, so a "
                               "swapped LUT repatches and an identical one "
                               "prunes"),
                .titleStyle = label(15, kInk, 2.0f),
                .subtitleStyle = label(11, kDim, 0.6f),
                .footerStyle = label(10.5f, kDim, 0.2f),
                .marginX = 30,
                .marginTop = 22,
                .marginBottom = 16,
                .ground = Fill::color(kGround),
                .rule = Fill::color(kRule)},
               kit::cells({.cells = {std::move(slots), std::move(stack)},
                           .column = true,
                           .gap = 26}))
        .absolute()
        .inset(0);
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas(1060, 690);
    ctx.background(kGround);
    ctx.captureAt(1.0);  // the live panel is on the fire LUT here
    ctx.composer.render(describe());
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
    // Derived from `elapsed`, not accumulated: a still at a declared time
    // is then the same still every run.
    const int want = (int)(elapsed / kSwapEvery);
    if (want == live) return;
    live = want;
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(
    MaterialChild, "Kit \xc2\xb7 API",
    "child slots \xe2\x80\x94 an index texture through a palette LUT, and "
    "over(base, top, mask) stacking three materials into one")
