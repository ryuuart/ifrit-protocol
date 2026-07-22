// stock_materials.cpp — the guard sketch for the split-Skia SkSL rule.
//
// A sketch dylib carries its OWN copy of Skia (vcpkg builds Skia with
// hidden visibility, so sketch dylibs link libskia.a directly rather than
// resolving its symbols from the host — see sketch/README.md). That means
// `SkRuntimeEffect::MakeForShader` allocates the SkSL AST inside the
// SKETCH's Skia image, while `SkRuntimeEffect::getRPProgram` and the SkSL
// inliner run inside the HOST's. Virtual dispatch across that boundary
// faults on pointer authentication.
//
// Shallow shaders never reach the deep inliner path, so the failure looks
// arbitrary — until you notice the ones that crash are exactly the ones
// whose SkSL has nested or repeated helper calls. The rule that falls out:
//
//     every stock SkSL material must keep main() monolithic.
//
// This sketch paints one of each of them, so the rule is enforced by the
// build rather than by memory. It is wired up as the `compose_sketch_stock`
// ctest; if someone adds a helper function to a shader in Patterns.h,
// Material.h or Sdf.h, this test segfaults and says so.
//
// (patterns::grain shipped with a hash21()/vnoise() pair and crashed every
// sketch that touched it. Two agents in the overnight program found it
// within minutes of each other.)

#include <sigilsketch/Sketch.h>

#include <sigilcompose/Material.h>
#include <sigilcompose/Patterns.h>
#include <sigilcompose/Sdf.h>

using namespace sigil::compose;

namespace {

sigil::weave::TextStyle label(float size) {
  sigil::weave::TextStyle s;
  s.shaping.fontSize = size;
  s.shaping.letterSpacing = 1.2f;
  s.paint.foreground.setColor4f({0.86f, 0.88f, 0.92f, 1}, nullptr);
  s.paint.foreground.setAntiAlias(true);
  return s;
}

/** One labelled swatch. The material is PAINTED, which is the whole
 *  point: compiling the effect is not what crashes, running it is. */
Element swatch(const char *name, Material material) {
  return box().width(150).height(112).column()
      .child(box().width(150).height(88)
                 .fill(std::move(material))
                 .foreground(util::stroke(1.0f,
                                          Fill::color({1, 1, 1, 0.25f}))))
      .child(text(util::toU8(name), label(10)).margin(0, 6, 0, 0));
}

} // namespace

struct StockMaterialsSketch : sigil::compose::sketch::Sketch {
  void setup(sketch::SketchContext &ctx) override {
    ctx.canvas(660, 420);
    ctx.background({0.05f, 0.05f, 0.07f, 1});

    const std::vector<Stop> ramp = {{0.0f, {0.95f, 0.35f, 0.25f, 1}},
                                    {0.5f, {0.95f, 0.80f, 0.30f, 1}},
                                    {1.0f, {0.20f, 0.55f, 0.95f, 1}}};

    ctx.composer.render(
        stack()
            .fill(Material::solid({0.05f, 0.05f, 0.07f, 1}))
            .child(text(util::toU8("STOCK SkSL MATERIALS \xc2\xb7 "
                                   "split-Skia guard"),
                        label(12))
                       .absolute().left(24).top(20))
            .child(box().absolute().left(24).top(52).right(24)
                       .row().gap(12)
                       .child(swatch("patterns::grain",
                                     patterns::grain(0.35f, 4, 3.0f)))
                       .child(swatch("patterns::halftoneRamp",
                                     patterns::halftoneRamp(
                                         9, 1.0f, 3.6f,
                                         {0.95f, 0.80f, 0.30f, 1})))
                       .child(swatch("patterns::noise",
                                     patterns::noise(0.05f, 4, 2.0f)))
                       .child(swatch("Material::linearUnit",
                                     Material::linearUnit({0, 0}, {1, 1},
                                                          ramp))))
            .child(box().absolute().left(24).top(184).right(24)
                       .row().gap(12)
                       .child(swatch("Material::radialUnit",
                                     Material::radialUnit({0.5f, 0.5f}, 1.0f,
                                                          ramp)))
                       .child(swatch("sdf::circle",
                                     sdf::material(sdf::circle(),
                                                   {.fill = {0.20f, 0.55f,
                                                             0.95f, 1},
                                                    .borderWidth = 3,
                                                    .borderColor = {1, 1, 1,
                                                                    0.9f},
                                                    .glowRadius = 10,
                                                    .glowColor = {0.4f, 0.7f,
                                                                  1, 0.6f}})))
                       .child(swatch("sdf::roundBox",
                                     sdf::material(sdf::roundBox(14),
                                                   {.fill = {0.95f, 0.35f,
                                                             0.25f, 1},
                                                    .borderWidth = 2,
                                                    .borderColor = {1, 0.9f,
                                                                    0.7f,
                                                                    0.9f}})))
                       .child(swatch("sdf::star",
                                     sdf::material(sdf::star(6, 2.6f),
                                                   {.fill = {0.95f, 0.80f,
                                                             0.30f, 1}}))))
            .child(box().absolute().left(24).top(316).right(24)
                       .row().gap(12)
                       .child(swatch("blend(grain over ramp)",
                                     Material::blend(
                                         {{Material::linearUnit({0, 0},
                                                                {0, 1}, ramp),
                                           SkBlendMode::kSrc},
                                          {patterns::grain(0.5f, 3, 9.0f),
                                           SkBlendMode::kOverlay}})))
                       .child(swatch("blend(sdf over grain)",
                                     Material::blend(
                                         {{patterns::grain(0.2f, 2, 5.0f),
                                           SkBlendMode::kSrc},
                                          {sdf::material(
                                               sdf::circle(),
                                               {.fill = {0.95f, 0.35f, 0.25f,
                                                         0.8f}}),
                                           SkBlendMode::kSrcOver}}))))
            .child(text(util::toU8("every one of these is painted, not just "
                                   "compiled \xe2\x80\x94 running the effect "
                                   "is what crosses the image boundary"),
                        label(10))
                       .absolute().left(24).bottom(16)));
  }
};

SIGIL_SKETCH(StockMaterialsSketch)
