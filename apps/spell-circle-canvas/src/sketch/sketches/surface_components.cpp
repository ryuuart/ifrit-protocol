/** @file
 * surface_components — properties and children make a reusable VFX card.
 *
 * One component accepts a solid, a live fill, or a material. The same
 * gel style follows two resolved heights under texture caching. Five
 * cards occupy a three-column grid without widening the last row.
 *
 * EDIT THESE FIRST
 *   Card — the component's properties; content arrives as an Element.
 *   columns — the number of equal tracks in the panel grid.
 */

// TAGS: Materials/Compositing

#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilcompose/kit/Frame.h>
#include <sigilcompose/kit/Gel.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/color/Color.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
#include <sigilweave/layout/StyleSheet.h>
#include <sigilweave/style/Type.h>

#include <cmath>
#include <utility>

namespace sketch = sigil::sketch;
namespace weave = sigil::weave;
namespace material = sigil::material;
using namespace sigil::compose;

namespace {

struct Card {
  std::u8string title;
  SurfacePaint ground;
};

/** The one ink every line on the sheet is set in; the sheet's own lines
 *  differ from a card's in size alone. */
constexpr material::Color kInk{0.94f, 0.95f, 0.98f, 1};

Element card(const Card& properties, Element content) {
  return kit::well(
      {.height = 204, .ground = properties.ground},
      box().column().padding(18).gap(12).borderRadius({12}).children(
          {text(properties.title), std::move(content).flexGrow(1)}));
}

Element gel(float height) {
  return kit::centred(box()
                          .key("gel")
                          .width(112)
                          .height(height)
                          .borderRadius({height / 2})
                          .layerStyle(kit::aquaGel({0.10f, 0.64f, 0.96f, 1}))
                          .cache(Cache::Texture));
}

struct SurfaceComponents {
  choreograph::Output<Fill> ink{Fill::color({0.10f, 0.30f, 0.40f, 1})};
  int sizeStep = -1;

  Element describe(float height) {
    const SurfacePaint slate = Fill::color({0.10f, 0.13f, 0.18f, 1});
    const SurfacePaint ramp = material::skia::Paint::linearUnit(
        {0, 0}, {1, 1},
        {{0, {0.28f, 0.10f, 0.38f, 1}}, {1, {0.07f, 0.28f, 0.35f, 1}}});
    // The sheet's three lines differ from a card's in size alone, so each
    // class is a size over the root's ink and face.
    return kit::sheet(
               {.title = "Components for VFX",
                .subtitle = "Props + children",
                .footer = "One card · three paints · responsive styles · "
                          "a shared grid",
                .marginX = 28,
                .marginTop = 24,
                .ground = Fill::color({0.035f, 0.045f, 0.07f, 1})},
               kit::panelGrid(
                   {.cells = {card({u8"Solid", slate},
                                   text(u8"A plain surface prop.")),
                              card({u8"Material", ramp},
                                   text(u8"The same prop accepts a recipe.")),
                              card({u8"Live fill", &ink},
                                   text(u8"The binding updates in place.")),
                              card({u8"Gel · fixed size", slate}, gel(36)),
                              card({u8"Gel · resizing", slate}, gel(height))},
                    .columns = 3,
                    .gap = 18,
                    .rowGap = 18}))
        .styleSheet(weave::StyleSheet{{"h1", {.size = 30}},
                                      {"lead", {.size = 17}},
                                      {"footer", {.size = 14}}})
        .font({.size = 16})
        .ink(kInk);
  }

  void setup(sketch::SketchContext& ctx) {
    ctx.canvas({.size = {1020, 620}, .captureSeconds = 2.5});
    ctx.composer.render(describe(80));
  }

  void update(double elapsed, sketch::SketchContext& ctx) {
    const float wave = 0.5f + 0.5f * std::sin((float)elapsed);
    ink = Fill::color({0.08f, 0.18f + wave * 0.18f, 0.28f + wave * 0.20f, 1});
    const int step = (int)elapsed % 4;
    if (step != sizeStep) {
      sizeStep = step;
      ctx.composer.render(describe(step < 2 ? 36.0f : 80.0f));
    }
  }
};

}  // namespace

SIGIL_SKETCH(SurfaceComponents, "Kit · API",
             "reusable cards, live surface properties and resizing gel styles")
