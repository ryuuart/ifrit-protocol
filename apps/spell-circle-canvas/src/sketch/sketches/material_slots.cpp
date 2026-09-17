/** @file
 * material_slots — a material filling another's slot, twice over:
 * the SkSL slot a shader samples, and the three slots `over()` stacks.
 *
 * TOP ROW — `Paint::sksl(...).children({name, Paint})`. A shader with TWO
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
 *
 * A LUT STRIP IS A STEP FUNCTION OF THE SAMPLER'S OWN COORDINATE: sixteen
 * texels magnified eleven times and sampled NEAREST, so a texel boundary
 * that lands on a device pixel boundary is a tie and the sample takes one
 * whole palette entry or the next. It reads as a stress test of the
 * runtime's caching contract and it is one — what decides the tie is the
 * inverse matrix, and a device-space bake now stands on the canvas's own
 * grid, so the matrix it inverts is the live paint's to the bit and the
 * tie falls the same way on both sides.
 */

// TAGS: Materials/Shaders

#include <include/core/SkBitmap.h>
#include <include/effects/SkRuntimeEffect.h>
#include <sigilcompose/core/Core.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/core/Combine.h>
#include <sigilmaterial/field/Field.h>
#include <sigilmaterial/kit/Grained.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilsketch/kit/Kit.h>

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace sketch = sigil::sketch;
namespace mat = sigil::material;
namespace mskia = sigil::material::skia;
namespace field = sigil::material::field;
namespace mkit = sigil::material::kit;

using namespace sigil::compose;

namespace {

constexpr float kShade = 6.0f;      // uShade on the fourth panel
constexpr double kSwapEvery = 0.8;  // seconds per LUT on the live panel
constexpr int kCells = 4;           // the index chart is kCells x kCells

constexpr float kMaskContrast = 3.2f;  // how hard the grain field's cut is

constexpr SkColor4f kFrame{0.20f, 0.24f, 0.32f, 1};
constexpr float kPanel = 188.0f;

/** The specimen sheet, in this one's own look. */
sketch::kit::Theme sheetTheme() {
  sketch::kit::Theme look = sketch::kit::studyTheme();
  look.palette.ground = {0.055f, 0.06f, 0.085f, 1};
  look.palette.ink = {0.90f, 0.93f, 0.97f, 1};
  look.palette.rule = {0.19f, 0.20f, 0.26f, 1};
  look.type.captionLabel = {.size = 12.5f, .track = 0.4f};
  look.spacing.marginX = 40;
  look.spacing.marginTop = 40;
  look.spacing.captionGap = 6;
  return look;
}

/** THE SHADER. Two `uniform shader` slots, one float, and main() kept
 *  MONOLITHIC: a sketch dylib carries its own copy of Skia, so the SkSL
 *  AST is allocated in one Skia image and inlined in another, and virtual
 *  dispatch across that boundary faults on pointer authentication. A
 *  helper function here is a crash, not a style choice. */
sk_sp<SkRuntimeEffect> paletteEffect() {
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

/** The three LUTs. Built ONCE and HELD FOR THE SKETCH'S LIFE, because
 *  Material::image compares by image POINTER — minting a fresh SkImage
 *  per describe would make every material unequal to every other and
 *  defeat every prune. The sketch is what holds them, not a
 *  function-local static: this file is compiled into a dylib a reload
 *  unloads, and the tables must not outlive it. */
sk_sp<SkImage> greyLut() {
  std::vector<SkColor> v;
  v.reserve(16);
  for (int i = 0; i < 16; ++i) v.push_back(grey(17 * i));
  return lut(v);
}
sk_sp<SkImage> fireLut() {
  return lut({0xff100005, 0xff2a0410, 0xff450a16, 0xff62111a, 0xff7f1a1c,
              0xff9c261b, 0xffb8351a, 0xffd04718, 0xffe25c17, 0xffee7419,
              0xfff58f26, 0xfff9a840, 0xfffcc063, 0xfffdd68e, 0xfffee8bd,
              0xffffffff});
}
sk_sp<SkImage> iceLut() {
  return lut({0xff03060f, 0xff071228, 0xff0b1f42, 0xff102c5c, 0xff143a76,
              0xff17498f, 0xff1a59a7, 0xff1f6bbc, 0xff2a7fcd, 0xff3d93da,
              0xff56a7e4, 0xff74baec, 0xff96cdf2, 0xffbadff7, 0xffdceffb,
              0xffffffff});
}

/** THE INDEX TEXTURE — a kCells x kCells chart whose red byte is 0..15 in
 *  reading order. It is DATA, so it is sampled NEAREST everywhere: an index
 *  read at kLinear is a blend of two unrelated palette entries, which is
 *  the trap this whole texture kind carries. */
sk_sp<SkImage> indexChart() {
  SkBitmap bm;
  bm.allocN32Pixels(kCells, kCells);
  for (int y = 0; y < kCells; ++y)
    for (int x = 0; x < kCells; ++x)
      *bm.getAddr32(x, y) =
          SkPreMultiplyColor(SkColorSetARGB(255, y * kCells + x, 0, 0));
  bm.setImmutable();
  return bm.asImage();
}

/** The palettes, in the order the live panel cycles them. */
enum Table : size_t { Grey, Fire, Ice, TableCount };

/** EVERY TABLE THIS SHEET DRAWS WITH, held together for the sketch's
 *  life: the index chart, the three palettes and the one effect that
 *  reads them. One value, so nothing below has to be handed five. */
struct Tables {
  sk_sp<SkImage> index = indexChart();
  std::array<sk_sp<SkImage>, TableCount> luts{greyLut(), fireLut(), iceLut()};
  sk_sp<SkRuntimeEffect> effect = paletteEffect();
};

mskia::Paint indexSource(const Tables& tables) {
  return mskia::Paint::image(tables.index, SkTileMode::kClamp,
                             SkTileMode::kClamp,
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
mskia::Paint paletted(const Tables& tables, const sk_sp<SkImage>& table,
                      float shade) {
  return mskia::Paint::sksl(tables.effect)
      .uniform("uShade", shade)
      .slot("uIndex", indexSource(tables))
      .slot("uPalette", lutSource(table));
}

/** The LUT itself, shown as the 16-swatch strip it is. */
Element lutStrip(const sk_sp<SkImage>& table) {
  return box().width(kPanel).height(14).fill(
      mskia::Paint::image(table, SkTileMode::kClamp, SkTileMode::kClamp,
                          SkMatrix::Scale(kPanel / 16.0f, 14.0f),
                          SkSamplingOptions(SkFilterMode::kNearest)));
}

sketch::kit::ComparisonCase panel(const char* caseTitle, const Tables& tables,
                                  const char* call, const char* note,
                                  const sk_sp<SkImage>& table, float shade,
                                  std::string key) {
  return {.title = caseTitle,
          .control = call,
          .figure = box().column().gap(6).children(
              {box()
                   .key(std::move(key))
                   .width(kPanel)
                   .height(kPanel)
                   .fill(paletted(tables, table, shade))
                   .stroke(stroke(1.0f, Fill::color(kFrame))),
               lutStrip(table)}),
          .note = note};
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

sketch::kit::ComparisonCase operand(const char* caseTitle, const char* call,
                                    const char* note, mat::Material material,
                                    std::string key) {
  return {.title = caseTitle,
          .control = call,
          .figure = box()
                        .key(std::move(key))
                        .width(kPanel)
                        .height(kPanel)
                        .fill(mskia::Paint::recipe(std::move(material)))
                        .stroke(stroke(1.0f, Fill::color(kFrame))),
          .note = note};
}

sketch::kit::ComparisonCase stacked(const char* caseTitle, const char* call,
                                    const char* note, mat::Blend blend,
                                    std::string key) {
  return operand(caseTitle, call, note,
                 mat::over(stackBase(), stackTop(), stackMask(), blend),
                 std::move(key));
}

}  // namespace

struct MaterialChild {
  int live = 0;
  const Tables tables;

  Element describe() {
    // The theme is bound where the tree is DESCRIBED, not where setup
    // runs: this sketch describes again when the live panel changes
    // lane, and a scope that ended with setup would not be there.
    const sketch::kit::Provide look(sheetTheme());
    return sketch::kit::page(
        {.title = "A material can be an input",
         .subtitle =
             "A palette lookup and a masked stack expose their child slots",
         .footer = "The live palette is data: describing an equal tree prunes "
                   "it, while changing the table updates its consumer."},
        box().column().gap(32).children(
            {sketch::kit::sectionHeader(
                 {.label = "LOOK UP A COLOUR",
                  .note = "The chart stores indices. The strip below each "
                          "result is its palette."}),
             sketch::kit::comparison(
                 {.cases = {panel(
                                "GREY", tables, "slot(\"uPalette\", grey)",
                                "Indices 0\u201315, read through a grey ramp.",
                                tables.luts[Grey], 0.0f, "grey"),
                            panel("FIRE", tables, "slot(\"uPalette\", fire)",
                                  "The same indices through the fire table.",
                                  tables.luts[Fire], 0.0f, "fire"),
                            panel("ICE", tables, "slot(\"uPalette\", ice)",
                                  "The same indices through the ice table.",
                                  tables.luts[Ice], 0.0f, "ice"),
                            panel("SHADE +6", tables, "uniform(\"uShade\", 6)",
                                  "Add six to the index; clamp at entry 15.",
                                  tables.luts[Ice], kShade, "shade"),
                            panel("LIVE TABLE", tables,
                                  "the LUT swapped by update()",
                                  "The table changes every 0.8 seconds.",
                                  tables.luts[(size_t)live % TableCount], 0.0f,
                                  "live")},
                  .measure = 1020,
                  .gap = 20}),
             box().column().gap(18).children(
                 {box()
                      .row()
                      .alignItems(Align::Start)
                      .gap(20)
                      .children({sketch::kit::sectionHeader(
                                     {.label = "THREE INPUTS",
                                      .note = "Base + top + a scalar mask"})
                                     .width(604),
                                 sketch::kit::sectionHeader(
                                     {.label = "TWO COMPOSITING LAWS",
                                      .note = "The operands stay the same."})
                                     .width(396)}),
                  sketch::kit::comparison(
                      {.cases = {operand("BASE", "kit::latten({.level = 0.62})",
                                         "Latten provides the base.",
                                         stackBase(), "base"),
                                 operand("TOP", "kit::stone({.bedAngle = 62})",
                                         "Stone provides the top.", stackTop(),
                                         "top"),
                                 operand("MASK",
                                         "field::grain(0.018, 4, contrast 3.2)",
                                         "Red-channel grain supplies coverage.",
                                         stackMask(), "mask"),
                                 stacked("MIX", "over(base, top, mask)",
                                         "The mask interpolates between "
                                         "base and top.",
                                         mat::Blend::Mix, "over.mix"),
                                 stacked("MULTIPLY", "over(…, Blend::Multiply)",
                                         "The mask controls a "
                                         "multiplicative blend.",
                                         mat::Blend::Multiply, "over.mul")},
                       .measure = 1020,
                       .gap = 20})})}));
  }

  void setup(sketch::SketchContext& ctx) {
    const sketch::kit::Provide look(sheetTheme());
    // the live panel is on the fire LUT here
    sketch::kit::stage(ctx, {.size = {1100, 930}, .captureAt = 1.0});
    ctx.composer.render(describe());
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    // Derived from `elapsed`, not accumulated: a still at a declared time
    // is then the same still every run.
    const int want = (int)(elapsed / kSwapEvery);
    if (want == live) return;
    live = want;
    ctx.composer.render(describe());
  }
};

SIGIL_SKETCH(MaterialChild, "Kit · API",
             "child slots — an index texture through a palette LUT, and "
             "over(base, top, mask) stacking three materials into one")
