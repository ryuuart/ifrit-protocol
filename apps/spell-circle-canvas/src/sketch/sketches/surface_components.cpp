/** @file
 * surface_components — props and children make a reusable VFX card.
 *
 * One component accepts a solid, a live fill, or a material. The same
 * gel style follows two resolved heights under texture caching. Five
 * cards occupy a three-column grid without widening the last row.
 *
 * EDIT THESE FIRST
 *   Card — the component's props; content arrives as an Element.
 *   columns — the number of equal tracks in the panel grid.
 */

#include <sigilcompose/core/Core.h>
#include <sigilcompose/core/SurfacePaint.h>
#include <sigilcompose/kit/Gel.h>
#include <sigilcompose/kit/Specimen.h>
#include <sigilmaterial/skia/Paint.h>
#include <sigilsketch/canvas/Sketch.h>
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

weave::TextStyle label(float size = 16) {
  return weave::textStyle({.size = size, .color = {0.94f, 0.95f, 0.98f, 1}});
}

Element card(const Card& props, Element content) {
  return kit::well({.height = 204, .ground = props.ground},
                   box()
                       .column()
                       .padding(18)
                       .gap(12)
                       .corners({12})
                       .child(text(props.title, label()))
                       .child(std::move(content).grow(1)));
}

Element gel(float height) {
  return box()
      .alignItems(Align::Center)
      .justify(Justify::Center)
      .child(box()
                 .key("gel")
                 .width(112)
                 .height(height)
                 .corners({height / 2})
                 .style(kit::aquaGel({0.10f, 0.64f, 0.96f, 1}))
                 .cache(Cache::Texture));
}

struct SurfaceComponents : sketch::Sketch {
  choreograph::Output<Fill> ink{Fill::color({0.10f, 0.30f, 0.40f, 1})};
  int sizeStep = -1;

  Element describe(float height) {
    const SurfacePaint slate = Fill::color({0.10f, 0.13f, 0.18f, 1});
    const SurfacePaint ramp = material::skia::Paint::linearUnit(
        {0, 0}, {1, 1},
        {{0, {0.28f, 0.10f, 0.38f, 1}}, {1, {0.07f, 0.28f, 0.35f, 1}}});
    return kit::sheet(
        {.title = u8"Components for VFX",
         .subtitle = u8"Props + children",
         .footer =
             u8"One card · three paints · responsive styles · a shared grid",
         .titleStyle = label(30),
         .subtitleStyle = label(17),
         .footerStyle = label(14),
         .marginX = 28,
         .marginTop = 24,
         .ground = Fill::color({0.035f, 0.045f, 0.07f, 1})},
        kit::panelGrid(
            {.cells = {card({u8"Solid", slate},
                            text(u8"A plain surface prop.", label())),
                       card({u8"Material", ramp},
                            text(u8"The same prop accepts a recipe.", label())),
                       card({u8"Live fill", &ink},
                            text(u8"The binding updates in place.", label())),
                       card({u8"Gel · fixed size", slate}, gel(36)),
                       card({u8"Gel · resizing", slate}, gel(height))},
             .columns = 3,
             .gap = 18,
             .rowGap = 18}));
  }

  void setup(sketch::SketchContext& ctx) override {
    ctx.canvas({.size = {1020, 620}, .captureSeconds = 2.5});
    ctx.composer.render(describe(80));
  }

  void update(double elapsed, sketch::SketchContext& ctx) override {
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
             "reusable cards, live surface props and resizing gel styles")
